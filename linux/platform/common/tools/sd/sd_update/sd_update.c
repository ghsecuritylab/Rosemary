#include <stdio.h>
#include <string.h>  /* strerror */
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <wchar.h>
#include <linux/fs.h>

//#define DEBUG

#if defined (DEBUG)
#define debug(msg...) { printf(msg); }
#else
#define debug(msg...) do{} while (0)
#endif

#define MAX_PARTITION_TABLE		20
#define MMC_BLOCK_SIZE			512

#define	BL_TO_SIZE(n)			(n*MMC_BLOCK_SIZE)
#define	SIZE_TO_BL(n)			(n/MMC_BLOCK_SIZE)

/* support fs type */
#define	PART_FS_2NDBOOT		(1<<0)	/*  name "boot" <- bootable */
#define	PART_FS_BOOT		(1<<1)	/*  name "boot" <- bootable */
#define	PART_FS_RAW			(1<<2)	/*  name "raw" */
#define	PART_FS_FAT			(1<<4)	/*  name "fat" */
#define	PART_FS_EXT4		(1<<5)	/*  name "ext4" */
#define	PART_FS_UBI			(1<<6)	/*  name "ubi" */
#define	PART_FS_UBIFS		(1<<7)	/*  name "ubifs" */
#define	PART_FS_RAW_PART	(1<<8)	/*  name "emmc" */

#define	PART_MBR_MASK		(PART_FS_EXT4 | PART_FS_FAT | PART_FS_UBI | PART_FS_UBIFS | PART_FS_RAW_PART)

typedef struct part_fstype {
	char *name;
	unsigned int fs_type;
} part_fstype;

static struct part_fstype part_fs[] = {
	/* no MBR */
	{ "2nd"		, PART_FS_2NDBOOT  	},
	{ "boot"	, PART_FS_BOOT  	},
	/* set MBR */
	{ "raw"		, PART_FS_RAW		},
	{ "fat"		, PART_FS_FAT		},
	{ "ext4"	, PART_FS_EXT4		},
	{ "emmc"	, PART_FS_RAW_PART	},
	{ "ubi"		, PART_FS_UBI		},
	{ "ubifs"	, PART_FS_UBIFS		},
};

typedef struct partinfo {
	long bl_start;
	long bl_size;
	bool partition;
	int  part_num;
	char image[256];
} partinfo;

static int file_test(char *file)
{
	if (0 != access(file, F_OK)) {
		printf("cannot access file (%s).\n", file);
		return -errno;
	}
	return 0;
}

static void print_usage(void)
{
	printf("usage: options\n"
		"-d device  node default : none \n"
		"-p partmap file default : ./partmap.txt \n"
		"-i images path : default path is \"partmap.txt\" directory \n"
		"-s show device partitioin \n"
		"-h print this help text\n\n");

	printf("usage: partmap.txt\n"
		"flash= <device>.<dev no>:<partition>:<fstype>:<start>,<length>:<file name>;\n"
		"device    : disk device name, not use \n"
		"dev no    : disk device number, not use \n"
		"partition : partition name, not use \n"
		"fstype    : file system type, MBR = raw, fat, ext4\n"
		"start     : disk partition start address (hex) \n"
		"length    : disk partition length (hex) \n"
		"file name : write file name to partition\n"
		);
}

/* cat /sys/block/<dev>/size */
static long long get_disk_capacity(char *file)
{
	int fd;
	long long size = 0;

	fd = open(file, O_RDONLY);
	if (0 > fd) {
		printf("Fail, open %s\n", file);
		return -1;
	}
	ioctl(fd, BLKGETSIZE64, &size);
	close(fd);

	return SIZE_TO_BL(size);
}

static bool is_mbr_part(char *type)
{
	struct part_fstype *fs = part_fs;
	int array_size = (sizeof(part_fs)/sizeof(part_fs[0]));
	int i = 0;

	for ( ; array_size > i; i++, fs++) {
		if (0 == strcmp(fs->name, type)) {
			if (PART_MBR_MASK & fs->fs_type)
			return true;
		}
	}
	return false;
}

extern int mmc_write(char *path, long  long size, long long bl_start, char *buffer);
extern int mmc_read (char *path, long  long size, long long bl_start, char *buffer);

extern int disk_part_table_info(char *path, long ex_sector, long relative,
                     long (*parts)[3], int *part_num,
                     int (*cb_read)(char*, long long, long long, char*));

int disk_part_table_make(char *path, long capacity_bl,
				long (*parts)[2], int part_num,
				int (*cb_write)(char*, long long, long long, char*));

int main(int argc, char **argv)
{
	int opt;
	FILE *fp = NULL;
	char *dev_path = NULL, *map_path = NULL;
	char  img_path[MAXPATHLEN] = {0,} ;

	long parts_disk[MAX_PARTITION_TABLE][3] = {{0,},};	/* [0]=start, [1]=length, [2]=part type */
	long parts_mbr[MAX_PARTITION_TABLE][2] = {{0,},};	/* [0]=start, [1]=length, new partition for MBR */
	partinfo pt_map[MAX_PARTITION_TABLE];

	int disk_parts = 0, disk_mbr = 0;
	int map_parts = 0, mbr_parts = 0;
	bool new_MBR = true;
	long long disk_bl_size = 0;
	char buffer[256];
	int i = 0, m = 0, ret = 0;
	int o_show_parts = 0;

	while (-1 != (opt = getopt(argc, argv, "d:p:i:sh"))) {
		switch (opt) {
	  	case 'p': map_path = strdup(optarg); break;
		case 'd': dev_path = strdup(optarg); break;
		case 'i': strcpy(img_path, optarg); break;
		case 's': o_show_parts = 1; break;
		case 'h': print_usage();  exit(0);	break;
        default:
        	break;
	  }
	}

	/*
	 * check path
	 */
	if (0 > file_test(dev_path)) {
		printf("fail device open %s ...\n", dev_path);
		print_usage();
		exit(0);
	}

	if (!o_show_parts && (0 > file_test(map_path))) {
		printf("fail partition map %s ...\n", map_path);
		print_usage();
		exit(0);
	}

	/*
	 * check dist size and partition table
	 */
	disk_bl_size = get_disk_capacity(dev_path);
	if (0 >= disk_bl_size) {
		printf("invalid disk size (%lld) %s !!!\n", disk_bl_size, dev_path);
		exit(0);
	}

	printf("-----------------------------------------------------------------------------\n");
	printf("[%s] capacity = %lldMB, %lld bytes\n",
		dev_path, BL_TO_SIZE(disk_bl_size)/1024/1024, BL_TO_SIZE(disk_bl_size));

	ret = disk_part_table_info(dev_path, 0, 0, parts_disk, &disk_parts, &mmc_read);
	if (0 > ret) {
		printf("invalid partition info ret = %d !!!\n", ret);
		exit(0);
	}

	printf("current %s partition:\n", dev_path);
	for(i = 0 ; disk_parts > i; i++) {
		if (0x05 != parts_disk[i][2])
			disk_mbr++;
		printf("MBR.%d start : 0x%010lx size 0x%010lx %s kB\n", i,
			BL_TO_SIZE(parts_disk[i][0]), BL_TO_SIZE(parts_disk[i][1]),
			parts_disk[i][2]==0x05?"Ext":"");
	}

	if (o_show_parts)
		exit(0);

	/*
	 * Parsing Partmap.txt
	 */
	fp = fopen(map_path,"r");

	printf("-----------------------------------------------------------------------------\n");
	printf("parsing %s:\n", map_path);
	do {
		char parse[8][256] = {{0,},};
		partinfo *pt = &pt_map[map_parts];
		int len = 0;

		memset(buffer, 0, 256);
		memset(pt, 0, sizeof(*pt));

		if (fgets(buffer, 256, fp)==NULL)
			break;

		if('#' == buffer[0])
			continue;

		/*
		 * partmap > flash=mmc,0:2ndboot:2nd:0x...:0x...:boot.img;
		 * parsing > 0=flash, 1=mmc, 2=0, 3=2ndboot, 4=2nd, 5=0x..., 6=0x..., 7=boot.img
		 */
		sscanf(buffer, "%[^=]=%[^,],%[^:]:%[^:]:%[^:]:%[^,],%[^:]:%[^;]",
			parse[0], parse[1], parse[2], parse[3],
			parse[4], parse[5], parse[6], parse[7]);

		pt->bl_start = SIZE_TO_BL(strtoll(parse[5], NULL, 16));
		pt->bl_size  = SIZE_TO_BL(strtoll(parse[6], NULL, 16));
		pt->partition =	is_mbr_part(parse[4]);

		if (map_path && 0 == strlen(img_path))
			strcpy(img_path, dirname(map_path));

		if (strlen(parse[7]))
			len = sprintf(pt->image, "%s/", img_path);

		strcpy(pt->image+len, parse[7]);

		if (pt->partition) {
			parts_mbr[mbr_parts][0] = pt->bl_start;
			parts_mbr[mbr_parts][1] = pt->bl_size;
			pt->part_num = mbr_parts;
			mbr_parts++;
		}
		printf("part.%d %s=%s,%s:%s:%s:0x%lx,0x%lx:%s:[%s] %s\n",
			map_parts, parse[0], parse[1], parse[2], parse[3],
			parse[4],BL_TO_SIZE(pt->bl_start), BL_TO_SIZE(pt->bl_size),
			strlen(parse[7])?parse[7]:"", pt->partition?"MBR":"RAW", pt->image);

		map_parts++;	/* next */

	} while (1);

	if (fp)
		fclose(fp);

	/*
	 * create new partition and write to disk
	 */
	printf("-----------------------------------------------------------------------------\n");
	if (true == new_MBR) {
		printf("create new MBR %d:\n", mbr_parts);
		for (i = 0; mbr_parts > i; i++) {
			printf("[MBR.%d] start : 0x%010lx size 0x%010lx \n",
				i, BL_TO_SIZE(parts_mbr[i][0]), BL_TO_SIZE(parts_mbr[i][1]));
		}
		ret = disk_part_table_make(dev_path, disk_bl_size,
					parts_mbr, mbr_parts, mmc_write);
		if (0 > ret)
			return ret;
	}

	/*
	 * copy image to partition
	 */
	printf("-----------------------------------------------------------------------------\n");
	printf("copy from: %s to %s\n", img_path, dev_path);

	for (i = 0, m = 0; map_parts > i; i++) {
		long long availiable;
		char *source = NULL;
		long long file_size = 0;
		partinfo *pt = &pt_map[i];

		if (!strlen(pt->image))
			continue;

		fp = fopen(pt->image, "r");
		if (NULL == fp) {
			printf("fail open %s, ret = %s, continue ...\n",
				pt->image, strerror(errno));
			continue;
		}

		fseek(fp, 0L, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		availiable = disk_bl_size - (pt->bl_start + pt->bl_size);
		if (0 == pt->bl_size)
			pt->bl_size = availiable;

		if (file_size > BL_TO_SIZE(pt->bl_size)) {
			printf("copy part.%d start : 0x%lx size 0x%lx (0x%llx) more than capacity %lx : %s\n",
				i, BL_TO_SIZE(pt->bl_start), BL_TO_SIZE(pt->bl_size),
				file_size, BL_TO_SIZE(pt->bl_size), pt->image);
			continue;
		}

		pt->partition ? printf("[MBR.%d] ", pt->part_num) : printf("[RAW.%d] ", i);
		printf("part.%d start : 0x%lx size 0x%lx (0x%llx) : %s ",
			i, BL_TO_SIZE(pt->bl_start), BL_TO_SIZE(pt->bl_size),
			file_size, pt->image);

		debug("cap:%lld, ava:%lld, len:%lld\n",
			BL_TO_SIZE(disk_bl_size), BL_TO_SIZE(availiable),
			(long long)BL_TO_SIZE(pt->bl_size));

		source = malloc(file_size);
		if(source == NULL) {
			printf(": fail alloc for %s (%lld), ret = %s\n",
				pt->image, file_size, strerror(errno));
			fclose(fp);
			exit(0);
		}

		ret = fread(source, 1, file_size, fp);
		if (ret != file_size) {
			printf(": fail read %s (%lld), ret = %s\n",
				pt->image, file_size, strerror(errno));
			fclose(fp);
			exit(0);
		}

		write_image(dev_path,
				BL_TO_SIZE(pt->bl_start), BL_TO_SIZE(pt->bl_size),
				source, file_size,
				mmc_write);

		printf(": done.\n");

		free(source);
		fclose(fp);
	}
	printf("-----------------------------------------------------------------------------\n");

	return 0;
}
