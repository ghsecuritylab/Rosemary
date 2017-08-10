#ifndef __NX_BINGEN__
#define __NX_BINGEN__

/* UTIL MACRO */
#define __ALIGN(x, a)		__ALIGN_MASK(x, (a) - 1)
#define __ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

#define ALIGN(x, a)		__ALIGN((x), (a))
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define IS_POWER_OF_2(n)	((n) && ((n) & ((n)-1)) == 0)


/* Defines */
#define ROMBOOT_MAXPAGE		(4096)
#define NSIH_LEN			(512)


/* CRC */
#define CRCSIZE				(4)
#define POLY				(0x04C11DB7L)

typedef union crc
{
	int iCrc;
	char chCrc[4];
} CRC;



#endif /* __NX_BINGEN__ */
