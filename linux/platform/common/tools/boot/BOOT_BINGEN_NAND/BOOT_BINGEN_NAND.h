#ifndef __BOOT_BINGEN_H__
#define __BOOT_BINGEN_H__

#define POLY 0x04C11DB7L

#define BOOT_DEBUG	
#define NX_DEBUG_MSG(...)


#define S32 int
#define S16 short
#define S8	char
#define U32 unsigned int
#define U16 unsigned short
#define U8  unsigned char

#define CBOOL	char

#define CTRUE	    TRUE
#define CFALSE	    FALSE
#define CFAILED     (-1)

#define NSIHSIZE	(0x200)	
#define CRCSIZE		(4)

#define BLOCKSIZE	(0x200)

#define ROMBOOT_STACK_SIZE          (4*1024)        

#define NXP4330_SRAM_SIZE			(16*1024)
#define NXP5430_SRAM_SIZE			(64*1024)
#define S5P4418_SRAM_SIZE			(32*1024)

#define SECONDBOOT_FSIZENCRC		(16*1024)
#define SECONDBOOT_SSIZENCRC		(8*1024)
#define SECONDBOOT_FSIZE			(SECONDBOOT_FSIZENCRC-(128/8))
#define SECONDBOOT_SSIZE			(SECONDBOOT_SSIZENCRC-(128/8))
#define CHKSTRIDE					8

#define SECURE_BOOT					(0)

#define BOOT_BINGEN_VER				(912)

/* S5P4418 */
#define ARCH_S5P4418


/* For NAND Device */
#define ROMBOOT_MAXPAGE		(4096)
#define ARCH_NXP4330



enum
{
	FALSE,
	TRUE,
};

typedef union crc
{
	int iCrc;
	char chCrc[4];
} CRC;

/* Secondboot Header -> Structure, Base Address ...ETC 													*/
/* NSIH VER:2015.01.26 */
//------------------------------------------------------------------------------------------------------//
#define HEADER_ID		((((U32)'N')<< 0) | (((U32)'S')<< 8) | (((U32)'I')<<16) | (((U32)'H')<<24))

enum
{
    BOOT_FROM_USB   = 0UL,
    BOOT_FROM_SPI   = 1UL,
    BOOT_FROM_NAND  = 2UL,
    BOOT_FROM_SDMMC = 3UL,
    BOOT_FROM_SDFS  = 4UL,
    BOOT_FROM_UART  = 5UL
};

struct NX_NANDBootInfo
{
	U8	AddrStep;
	U8	tCOS;
	U8	tACC;
	U8	tOCH;
    U8	PageSize;			// 512bytes unit
    U8	TIOffset;			// 3rd boot Image copy Offset. 1MB unit.
    U8	CopyCount;			// 3rd boot image copy count
    U8	LoadDevice;			// device chip select number
    U32	CRC32;
};

struct NX_SPIBootInfo
{
	U8  AddrStep;
	U8  _Reserved0[3];
	U32 _Reserved1 : 24;
	U32 LoadDevice : 8;
	U32 CRC32;
};

struct NX_UARTBootInfo
{
	U32	_Reserved0;
	U32	_Reserved1 : 24;
	U32 LoadDevice : 8;
	U32 CRC32;
};

struct NX_SDMMCBootInfo
{
	U8	PortNumber;
	U8	_Reserved0[3];
	U32	_Reserved1 : 24;
	U32 LoadDevice : 8;
	U32	CRC32;
};

struct NX_SDFSBootInfo
{
	char BootFileName[12];		// 8.3 format ex)"NXDATA.TBL"
};

union NX_DeviceBootInfo
{
	struct NX_NANDBootInfo NANDBI;
	struct NX_SPIBootInfo SPIBI;
	struct NX_SDMMCBootInfo SDMMCBI;
	struct NX_SDFSBootInfo SDFSBI;
	struct NX_UARTBootInfo UARTBI;
};

struct NX_DDRInitInfo
{
	U8	ChipNum;
	U8	ChipRow;
	U8	BusWidth;
	U8	_Reserved0;

	U16	ChipMask;
	U16	ChipBase;

#if 0
	U8	CWL;
	U8	WL;
	U8	RL;
	U8	DDRRL;
#else
    U8  CWL;				// 0x90
    U8  CL;					// 0x91
    U8  MR1_AL;				// 0x92
    U8  MR0_WR;				// 0x93
#endif

    U32 READDELAY;			// 0x94
    U32 WRITEDELAY;			// 0x98

    U32 TIMINGPZQ;			// 0x9C
    U32 TIMINGAREF;			// 0xA0
    U32 TIMINGROW;			// 0xA4
    U32 TIMINGDATA;			// 0xA8
    U32 TIMINGPOWER;		// 0xAC
};

struct NX_SecondBootInfo
{
	U32 VECTOR[8];					// 0x000 ~ 0x01C
	U32 VECTOR_Rel[8];				// 0x020 ~ 0x03C

	U32 DEVICEADDR;					// 0x040			// 3rd boot image start address

	U32 LOADSIZE;					// 0x044
	U32 LOADADDR;					// 0x048
	U32 LAUNCHADDR;					// 0x04C
	union NX_DeviceBootInfo DBI;	// 0x050~0x058

	U32 PLL[4];						// 0x05C ~ 0x068
	U32 PLLSPREAD[2];				// 0x06C ~ 0x070
#if defined(ARCH_NXP4330) || defined(ARCH_S5P4418)
    U32 DVO[5];						// 0x074 ~ 0x084

    struct NX_DDRInitInfo DII;		// 0x088 ~ 0x0AC

    U32 Stub[(0x1F0-0x0B0)/4];		// 0x0B0 ~ 0x1EC
#endif
#if defined(ARCH_NXP5430)
    U32 DVO[9];						// 0x074 ~ 0x094

    struct NX_DDRInitInfo DII;		// 0x098 ~ 0x0BC

    U32 Stub[(0x1F0-0x0C0)/4];		// 0x0C0 ~ 0x1EC
#endif
    U32 MemTestAddr;				// 0x1F0
    U32 MemTestSize;				// 0x1F4
    U32 MemTestTryCount;			// 0x1F8

    U32 SIGNATURE;					// 0x1FC    "NSIH"
};
//------------------------------------------------------------------------------------------------------//

void to_upper( char* string );
void to_lower( char* string );

CBOOL	NX_SHELLU_HexDump	( U32 SrcAddr, U32 PrintDataSize, U32 PrintDataWidth );

void print_hexdump( U32* pdwBuffer, U32 Size );

static inline U32 iget_fcs(U32 fcs, U32 data);
static inline U32 get_fcs(U32 fcs, U8 data);

static inline unsigned int __icalc_crc (void *addr, int len);
static inline unsigned int __calc_crc(void *addr, int len);
int ProcessNSIH( const char *pfilename, U8 *pOutData );
U32 HexAtoInt( const char *string );

static void usage(void);
static void print_bingen_info( void );


#endif
