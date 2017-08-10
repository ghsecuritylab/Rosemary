#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//#define DEBUG

#if defined (DEBUG)
#define debug(msg...) 	{ printf(msg); }
#else
#define debug(msg...) 	do{} while (0)
#endif

/**
 * this code from u-boot/common/decompress_ext4.h,c
*/
typedef struct ext4_file_header {
	unsigned int magic;
	unsigned short major;
	unsigned short minor;
	unsigned short file_header_size;
	unsigned short chunk_header_size;
	unsigned int block_size;
	unsigned int total_blocks;
	unsigned int total_chunks;
	unsigned int crc32;
} ext4_file_header;

typedef struct ext4_chunk_header {
	unsigned short type;
	unsigned short reserved;
	unsigned int chunk_size;
	unsigned int total_size;
} ext4_chunk_header;

#define EXT4_FILE_HEADER_MAGIC  0xED26FF3A
#define EXT4_FILE_HEADER_MAJOR  0x0001
#define EXT4_FILE_HEADER_MINOR  0x0000
#define EXT4_FILE_BLOCK_SIZE    0x1000

#define EXT4_FILE_HEADER_SIZE   (sizeof(struct ext4_file_header))
#define EXT4_CHUNK_HEADER_SIZE  (sizeof(struct ext4_chunk_header))

#define EXT4_CHUNK_TYPE_RAW     0xCAC1
#define EXT4_CHUNK_TYPE_FILL    0xCAC2
#define EXT4_CHUNK_TYPE_NONE    0xCAC3
#define SECTOR_BITS				9	/* 512B */

static int check_ext4_compress(char *source, long long part_size)
{
	ext4_file_header *file_header = (ext4_file_header*)source;

	if ((EXT4_FILE_HEADER_MAGIC != file_header->magic) ||
		(EXT4_FILE_HEADER_MAJOR != file_header->major) ||
		(EXT4_FILE_HEADER_SIZE  != file_header->file_header_size) ||
		(EXT4_CHUNK_HEADER_SIZE != file_header->chunk_header_size) ||
		(EXT4_FILE_BLOCK_SIZE   != file_header->block_size)) {
		return -1;
	}

	debug("\npartion size 0x%llx, image size 0x%llx \n",
		(unsigned long long)file_header->total_blocks*file_header->block_size, part_size);

	if (file_header->total_blocks > (part_size/file_header->block_size)) {
		printf("\nInvalid volume size! image is bigger than partition !!!\n");
		printf("image size %lld, partion size %lld\n",
			(long long)file_header->total_blocks*file_header->block_size, part_size);
		exit(0);
		return -ENOMEM;
	}
	return 0; /* compressed ext4 */
}

int ext4_compress_write(char *path, long long start, long long length, char *source)
{
    ext4_chunk_header *chunk_header;
    ext4_file_header *file_header;
    unsigned int sector_size;
    int total_chunks;
    char *s_base = (char *)source;
	int ret = 0;

    file_header = (ext4_file_header *)s_base;
    total_chunks = file_header->total_chunks;
	s_base += EXT4_FILE_HEADER_SIZE;

    while (total_chunks) {
		chunk_header = (struct ext4_chunk_header *)s_base;
        sector_size = (chunk_header->chunk_size*file_header->block_size);

	    debug("%s: total chunk = %d (%d:%d)\n",
    		 __func__, total_chunks,
    		 (chunk_header->chunk_size*file_header->block_size), chunk_header->total_size);

        switch (chunk_header->type) {
        case EXT4_CHUNK_TYPE_RAW:
         	ret = mmc_write(path, start, sector_size, s_base + EXT4_CHUNK_HEADER_SIZE);
            if (0 > ret)
				return ret;
             start += sector_size;
             break;
		case EXT4_CHUNK_TYPE_FILL:
		case EXT4_CHUNK_TYPE_NONE:
        default:
	        start += sector_size;
    		break;
         }

         total_chunks--;
         s_base += chunk_header->total_size;
    }

    debug("%s: write done\n", __func__);
    return 0;
}

int write_image(char *path, long long start, long long capacity,
				char *buffer, long long length,
				int (*cb_write)(char*, long long, long long, char*))
{
    int ret = 0;

	ret = check_ext4_compress(buffer, capacity);
	if (0 == ret) {
		ret = ext4_compress_write(path, start, length, buffer);
	} else {
		if (-ENOMEM == ret)
			return ret;
		ret = cb_write(path, start, length, buffer);
	}
    return ret;
}

