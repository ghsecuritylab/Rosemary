#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>  /* strerror */
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <wchar.h>
#include <linux/fs.h>

//#define DEBUG

#if defined(DEBUG)
#define debug(msg...) { printf(msg); }
#else
#define debug(msg...) do{} while (0)
#endif

#define MMC_BLOCK_SIZE			(512)
#define DOS_EBR_BLOCK			(0x100000/MMC_BLOCK_SIZE)
#define PART_TYPE_DOS			0x02
#define	MAX_PART_TABLE			(20)

#ifndef _DISK_PART_DOS_H
#define _DISK_PART_DOS_H

#define DOS_PART_DISKSIG_OFFSET 0x1b8
#define DOS_PART_TBL_OFFSET 0x1be
#define DOS_PART_MAGIC_OFFSET   0x1fe
#define DOS_PBR_FSTYPE_OFFSET   0x36
#define DOS_PBR32_FSTYPE_OFFSET 0x52
#define DOS_PBR_MEDIA_TYPE_OFFSET   0x15
#define DOS_MBR 0
#define DOS_PBR 1

typedef struct dos_partition {
    unsigned char boot_ind;     /* 0x80 - active            */
    unsigned char head;     /* starting head            */
    unsigned char sector;       /* starting sector          */
    unsigned char cyl;      /* starting cylinder            */
    unsigned char sys_ind;      /* What partition type          */
    unsigned char end_head;     /* end head             */
    unsigned char end_sector;   /* end sector               */
    unsigned char end_cyl;      /* end cylinder             */
    unsigned char start4[4];    /* starting sector counting from 0  */
    unsigned char size4[4];     /* nr of sectors in partition       */
} dos_partition_t;

#endif  /* _DISK_PART_DOS_H */

static inline int le32_to_int(unsigned char *le32)
{
    return ((le32[3] << 24) +
        (le32[2] << 16) +
        (le32[1] << 8) +
         le32[0]
       );
}

static inline void int_to_le32(unsigned char *le32, unsigned int blocks)
{
	le32[3] = (blocks >> 24) & 0xff;
	le32[2] = (blocks >> 16) & 0xff;
	le32[1] = (blocks >>  8) & 0xff;
	le32[0] = (blocks >>  0) & 0xff;
}

static inline int is_extended(int part_type)
{
    return (part_type == 0x5 ||
        part_type == 0xf ||
        part_type == 0x85);
}

static inline int is_bootable(dos_partition_t *p)
{
    return p->boot_ind == 0x80;
}

static inline void part_mmc_chs(dos_partition_t *pt, int lba_start, int lba_size)
{
	int head = 1, sector = 1, cys = 0;
	int end_head = 254, end_sector = 63, end_cys = 1023;

	/* default CHS */
	pt->head = (unsigned char)head;
	pt->sector = (unsigned char)(sector + ((cys & 0x00000300) >> 2) );
	pt->cyl = (unsigned char)(cys & 0x000000FF);
	pt->end_head = (unsigned char)end_head;
	pt->end_sector = (unsigned char)(end_sector + ((end_cys & 0x00000300) >> 2) );
	pt->end_cyl = (unsigned char)(end_cys & 0x000000FF);

	int_to_le32(pt->start4, lba_start);
	int_to_le32(pt->size4, lba_size);
}

static int check_part_type(unsigned char *buffer)
{
	debug("signature 0x%02x:0x%02x\n",
		buffer[DOS_PART_MAGIC_OFFSET + 0],
		buffer[DOS_PART_MAGIC_OFFSET + 1]);

    if((buffer[DOS_PART_MAGIC_OFFSET + 0] != 0x55) ||
        (buffer[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) ) {
        return (-1);
    } /* no DOS Signature at all */
    if (strncmp((char *)&buffer[DOS_PBR_FSTYPE_OFFSET],"FAT",3)==0 ||
        strncmp((char *)&buffer[DOS_PBR32_FSTYPE_OFFSET],"FAT32",5)==0) {
        return DOS_PBR; /* is PBR */
    }
    return DOS_MBR;     /* Is MBR */
}

static void print_one_part(dos_partition_t *p, int ex_sector,
               int part_num, unsigned int disk_sig, long (*parts)[3])
{
    int lba_start = ex_sector + le32_to_int(p->start4);
    int lba_size  = le32_to_int (p->size4);

    debug("part.%3d\t%-10d\t%-10d\t%08x-%02x\t%02x%s%s\n",
        part_num, lba_start, lba_size, disk_sig, part_num, p->sys_ind,
        (is_extended(p->sys_ind) ? " Extd" : ""),
        (is_bootable(p) ? " Boot" : ""));
        parts[part_num][0] = lba_start;
        parts[part_num][1] = lba_size;
        parts[part_num][2] = p->sys_ind;;
}

/* parts[3] -> [0]=start, [1]=length, [2]=type */
int disk_part_table_info(char *path, long ex_sector, long relative,
                     long (*parts)[3], int *part_num,
                     int (*cb_read)(char*, long long, long long, char*))
{

    dos_partition_t *pt;
	unsigned char buffer[MMC_BLOCK_SIZE];
	unsigned int disk_sig = 0;
    int i, ret = 0;

	if (NULL == cb_read) {
		printf("Error, not defined disk read function ptr !!!\n");
		return -EINVAL;
	}

    ret = cb_read(path, ex_sector*MMC_BLOCK_SIZE, MMC_BLOCK_SIZE, buffer);
	if (0 > ret)
		return ret;

    i = check_part_type(buffer);
    if (i != DOS_MBR &&
    	i != DOS_PBR) {
        printf ("bad MBR sector signature 0x%02x%02x\n",
            buffer[DOS_PART_MAGIC_OFFSET],
            buffer[DOS_PART_MAGIC_OFFSET + 1]);
		return -EINVAL;
    }

    if (!ex_sector)
        disk_sig = le32_to_int(&buffer[DOS_PART_DISKSIG_OFFSET]);

    /* Print all primary/logical partitions */
    pt = (dos_partition_t *) (buffer + DOS_PART_TBL_OFFSET);
    for (i = 0; i < 4; i++, pt++) {
		int lba_s = le32_to_int(pt->start4);
		int lba_l = le32_to_int(pt->size4);

        /*
         * fdisk does not show the extended partitions that
         * are not in the MBR
         */
        if ((pt->sys_ind != 0) &&
            (ex_sector == 0 || !is_extended (pt->sys_ind)) ) {
            print_one_part(pt, ex_sector, *part_num, disk_sig, parts);
        }

        /* Reverse engr the fdisk part# assignment rule! */
        if (lba_s && lba_l &&
        	((ex_sector == 0) ||
            (pt->sys_ind != 0 && !is_extended (pt->sys_ind))) ) {
            *part_num += 1;
        }
    }

    /* Follows the extended partitions */
    pt = (dos_partition_t *) (buffer + DOS_PART_TBL_OFFSET);
    for (i = 0; i < 4; i++, pt++) {
        if (is_extended (pt->sys_ind)) {
            int lba_start = le32_to_int (pt->start4) + relative;

            ret = disk_part_table_info(path, lba_start,
                		ex_sector == 0 ? lba_start : relative,
                		parts, part_num, cb_read);
			if (0 > ret)
				return ret;
        }
    }
    return 0;
}

static int part_table_make_extended(char *path, long capacity_bl,
					long lba_start, long relative,
					long (*parts)[2], int part_num,
					int (*cb_write)(char*, long long, long long, char*))
{
	unsigned char buffer[512] = { 0, };
	dos_partition_t *pt;
	long lba_s = 0, lba_l = 0, last_lba = lba_start;
	int i = 0, ret = 0;

	memset(buffer, 0x0, sizeof(buffer));
	buffer[DOS_PART_MAGIC_OFFSET] = 0x55;
	buffer[DOS_PART_MAGIC_OFFSET + 1] = 0xAA;

	pt = (dos_partition_t *)(buffer + DOS_PART_TBL_OFFSET);
	for (i = 0; 2 > i && part_num > i; i++, pt++) {
		lba_s = (int)(parts[i][0]);
		lba_l = (int)(parts[i][1]);

		if (0 == lba_s) {
			printf("-- Fail: invalid part.%d start 0x%lx --\n", i, parts[i][0]);
			return -1;
		}

		if (last_lba > lba_s) {
			printf ("** Overlapped primary part table 0x%llx previous 0x%llx (EBR) **\n",
				(unsigned long long)(lba_s)*MMC_BLOCK_SIZE,
				(unsigned long long)(last_lba)*MMC_BLOCK_SIZE);
			return -1;
		}

		if (0 == lba_l)
			lba_l = capacity_bl - last_lba - DOS_EBR_BLOCK;

		if (lba_l > (capacity_bl - last_lba)) {
			printf("-- Fail: part %d invalid length 0x%llx, avaliable (0x%llx)(EBR) --\n",
				i, (unsigned long long)(lba_l)*MMC_BLOCK_SIZE,
				(unsigned long long)(capacity_bl-last_lba)*MMC_BLOCK_SIZE);
			return -1;
		}

		pt->boot_ind = 0x00;
		pt->sys_ind  = 0x83;
		if (i == 1) {
			pt->sys_ind = 0x05;
			lba_s -= relative + DOS_EBR_BLOCK;
			lba_l += DOS_EBR_BLOCK;
		} else {
			lba_s -= lba_start;
		}

		debug("%s=\t0x%llx \t~ \t0x%llx \n",
			i == 0 ? "Prim p" : "Extd p",
			i == 0 ? (unsigned long long)(lba_s + lba_start)*MMC_BLOCK_SIZE :
					 (unsigned long long)(lba_s)*MMC_BLOCK_SIZE,
			(unsigned long long)(lba_l)*MMC_BLOCK_SIZE);

		part_mmc_chs(pt, lba_s, lba_l);
		last_lba = lba_s + lba_l;
	}

	ret = cb_write(path, lba_start*MMC_BLOCK_SIZE, MMC_BLOCK_SIZE, buffer);
	if (0 > ret)
		return ret;

	if (part_num)
		return part_table_make_extended(path, capacity_bl,
				lba_s + relative, relative, &parts[1], part_num-1, cb_write);

	return ret;
}

/* parts[2] -> [0]=start, [1]=length */
int disk_part_table_make(char *path, long capacity_bl,
				long (*parts)[2], int part_num,
				int (*cb_write)(char*, long long, long long, char*))
{
	unsigned char buffer[MMC_BLOCK_SIZE];
	dos_partition_t *pt;
	long lba_s, lba_l, last_lba = 0;
	int part_tables = part_num, part_EBR = 0;
	int i = 0, ret = 0;

	if (NULL == cb_write) {
		printf("Error, not defined disk write callback fn !!!\n");
		return -EINVAL;
	}

	if (1 > part_num || part_num > MAX_PART_TABLE) {
		printf ("** Can't make partition tables %d (1 ~ %d) **\n", part_num, MAX_PART_TABLE);
		return -1;
	}

	memset(buffer, 0x0, sizeof(buffer));
	buffer[DOS_PART_MAGIC_OFFSET] = 0x55;
	buffer[DOS_PART_MAGIC_OFFSET + 1] = 0xAA;

	if (part_num > 4) {
		part_tables = 3;
		part_EBR = 1;
	}

	pt = (dos_partition_t *) (buffer + DOS_PART_TBL_OFFSET);
	last_lba = (int)(parts[0][0]);

	for (i = 0; part_tables > i; i++, pt++) {
		lba_s = (int)(parts[i][0]);
		lba_l = parts[i][1] ? (int)(parts[i][1]) : (capacity_bl - last_lba);

		if (0 == lba_s) {
			printf("-- Fail: invalid part.%d start %ld block --\n", i, parts[i][0]);
			return -1;
		}

		if (last_lba > lba_s) {
			printf ("** Overlapped primary part table 0x%llx previous 0x%llx **\n",
				(unsigned long long)(lba_s)*MMC_BLOCK_SIZE,
				(unsigned long long)(last_lba)*MMC_BLOCK_SIZE);
			return -1;
		}

		if (lba_l > (capacity_bl - last_lba)) {
			printf("-- Fail: part %d invalid length 0x%llx, avaliable (0x%llx) --\n",
				i, (unsigned long long)(lba_l)*MMC_BLOCK_SIZE,
				(unsigned long long)(capacity_bl-last_lba)*MMC_BLOCK_SIZE);
			return -1;
		}

		/* Linux partition */
		pt->boot_ind = 0x00;
		pt->sys_ind  = 0x83;
		part_mmc_chs(pt, lba_s, lba_l);
		last_lba = lba_s + lba_l;

		debug("part.%d=\t0x%llx \t~ \t0x%llx \n",
			i, (unsigned long long)(lba_s)*MMC_BLOCK_SIZE, (unsigned long long)(lba_l)*MMC_BLOCK_SIZE);
	}

	debug("---- part_EBR:%d, part = %d,%d ----\n", part_EBR, part_tables, part_num);
	if (part_EBR) {
		lba_s = (int)(parts[3][0]) - DOS_EBR_BLOCK;
		lba_l = (int)(capacity_bl - last_lba);

		if (last_lba > lba_s) {
			printf ("** Overlapped extended part table 0x%llx (with offset -0x%llx) previous 0x%llx **\n",
				(unsigned long long)(lba_s)*MMC_BLOCK_SIZE,
				(unsigned long long)DOS_EBR_BLOCK*MMC_BLOCK_SIZE,
				(unsigned long long)(last_lba)*MMC_BLOCK_SIZE);
			return -1;
		}

		/* Extended partition */
		pt->boot_ind = 0x00;
		pt->sys_ind  = 0x05;
		part_mmc_chs(pt, lba_s, lba_l);
	}

	ret = cb_write(path, 0, MMC_BLOCK_SIZE, buffer);
	if (0 > ret)
		return ret;

	if (part_EBR)
		return part_table_make_extended(path, capacity_bl, lba_s, lba_s,
					&parts[part_tables], part_num - part_tables, cb_write);

	return ret;

}


