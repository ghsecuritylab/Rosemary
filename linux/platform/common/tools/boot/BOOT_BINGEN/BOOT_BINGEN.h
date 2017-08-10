#ifndef __BOOT_BINGEN_H__
#define __BOOT_BINGEN_H__

#define POLY 0x04C11DB7L

//#define BOOT_DEBUG	

/* PRINT MACRO */
#ifdef BOOT_DEBUG
#define NX_DEBUG(msg...)	do { printf("[DEBUG] " msg); } while (0)
#else
#define NX_DEBUG(msg...)	do {} while (0)
#endif
#define NX_ERROR(msg...)	do { printf("[ERROR] " msg); } while (0)

/* BASE TYPE MACRO */
#define S64 long long
#define S32 int
#define S16 short
#define S8	char
#define U64 unsigned long long
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

#define BOOT_BINGEN_VER				(1011)


/* NAND Macro */
//-----------------------------------------------------------
#define __ALIGN(x, a)		__ALIGN_MASK(x, (a) - 1)
#define __ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

#define ALIGN(x, a)		__ALIGN((x), (a))
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define IS_POWER_OF_2(n)	((n) && ((n) & ((n)-1)) == 0)
//-----------------------------------------------------------

enum
{
	FALSE,
	TRUE,
};

enum
{
	IS_3RDBOOT = 0,	
	IS_2NDBOOT = 1
};

typedef union crc
{
	int  iCrc;
	char chCrc[4];
} CRC;

/* Secondboot Header -> Structure, Base Address ...ETC 													*/
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

struct nand_info {
	S8     *nsih_name;
	U32		load_addr;
	U32		launch_addr;
	U32		dev_addr;
	S32		image_type;
	U32		page_size;
	U32		max_2ndboot;
};

#define MEM_TYPE_DDR3		1
#define ARCH_NXP4330		1

struct NX_NANDBootInfo
{
    U8  AddrStep;
    U8  tCOS;
    U8  tACC;
    U8  tOCH;
#if 0
    U32 PageSize    :24;    // 1 byte unit
    U32 LoadDevice  :8;
#else
    U8  PageSize;           // 512bytes unit
    U8  TIOffset;           // 3rd boot Image copy Offset. 1MB unit.
    U8  CopyCount;          // 3rd boot image copy count
    U8  LoadDevice;         // device chip select number
#endif
    U32 CRC32;
};

struct NX_SPIBootInfo
{
    U8  AddrStep;
    U8  _Reserved0[2];
    U8  PortNumber;

    U32 _Reserved1 : 24;
    U32 LoadDevice : 8;

    U32 CRC32;
};

struct NX_UARTBootInfo
{
    U32 _Reserved0;

    U32 _Reserved1 : 24;
    U32 LoadDevice : 8;

    U32 CRC32;
};

struct NX_SDMMCBootInfo
{
#if 1
    U8  PortNumber;
    U8  _Reserved0[3];
#else
    U8  _Reserved0[3];
    U8  PortNumber;
#endif

    U32 _Reserved1	: 24;
    U32 LoadDevice	: 8 ;

    U32 CRC32;
};

struct NX_DDR3DEV_DRVDSInfo
{
    U8  MR2_RTT_WR;
    U8  MR1_ODS;
    U8  MR1_RTT_Nom;
    U8  _Reserved0;
};

struct NX_LPDDR3DEV_DRVDSInfo
{
    U8  MR3_DS      : 4;
    U8  MR11_DQ_ODT : 2;
    U8  MR11_PD_CON : 1;
    U8  _Reserved0  : 1;

    U8  _Reserved1;
    U8  _Reserved2;
    U8  _Reserved3;
};

struct NX_DDRPHY_DRVDSInfo
{
    U8  DRVDS_Byte0;    // Data Slice 0
    U8  DRVDS_Byte1;    // Data Slice 1
    U8  DRVDS_Byte2;    // Data Slice 2
    U8  DRVDS_Byte3;    // Data Slice 3

    U8  DRVDS_CK;       // CK
    U8  DRVDS_CKE;      // CKE
    U8  DRVDS_CS;       // CS
    U8  DRVDS_CA;       // CA[9:0], RAS, CAS, WEN, ODT[1:0], RESET, BANK[2:0]

    U8  ZQ_DDS;         // zq mode driver strength selection.
    U8  ZQ_ODT;
    U8  _Reserved0[2];
};

struct NX_SDFSBootInfo
{
    char BootFileName[12];      // 8.3 format ex)"NXDATA.TBL"
};

union NX_DeviceBootInfo
{
    struct NX_NANDBootInfo  NANDBI;
    struct NX_SPIBootInfo   SPIBI;
    struct NX_SDMMCBootInfo SDMMCBI;
    struct NX_SDFSBootInfo  SDFSBI;
    struct NX_UARTBootInfo  UARTBI;
};

struct NX_DDRInitInfo
{
    U8  ChipNum;        // 0x88
    U8  ChipRow;        // 0x89
    U8  BusWidth;       // 0x8A
    U8  ChipCol;        // 0x8B

    U16 ChipMask;       // 0x8C
    U16 ChipBase;       // 0x8E

#if 0
    U8  CWL;            // 0x90
    U8  WL;             // 0x91
    U8  RL;             // 0x92
    U8  DDRRL;          // 0x93
#else
    U8  CWL;            // 0x90
    U8  CL;             // 0x91
    U8  MR1_AL;         // 0x92, MR2_RLWL (LPDDR3)
    U8  MR0_WR;         // 0x93, MR1_nWR (LPDDR3)
#endif

    U32 READDELAY;      // 0x94
    U32 WRITEDELAY;     // 0x98

    U32 TIMINGPZQ;      // 0x9C
    U32 TIMINGAREF;     // 0xA0
    U32 TIMINGROW;      // 0xA4
    U32 TIMINGDATA;     // 0xA8
    U32 TIMINGPOWER;    // 0xAC
};

struct NX_SecondBootInfo
{
    U32 VECTOR[8];                  // 0x000 ~ 0x01C
    U32 VECTOR_Rel[8];              // 0x020 ~ 0x03C

    U32 DEVICEADDR;                 // 0x040

    U32 LOADSIZE;                   // 0x044
    U32 LOADADDR;                   // 0x048
    U32 LAUNCHADDR;                 // 0x04C
    union NX_DeviceBootInfo DBI;    // 0x050~0x058

    U32 PLL[4];                     // 0x05C ~ 0x068
    U32 PLLSPREAD[2];               // 0x06C ~ 0x070

#if defined(ARCH_NXP4330) || defined(ARCH_S5P4418)
    U32 DVO[5];                     // 0x074 ~ 0x084

    struct NX_DDRInitInfo DII;      // 0x088 ~ 0x0AC

#if defined(MEM_TYPE_DDR3)
    struct NX_DDR3DEV_DRVDSInfo     DDR3_DSInfo;    // 0x0B0
#endif
#if defined(MEM_TYPE_LPDDR23)
    struct NX_LPDDR3DEV_DRVDSInfo   LPDDR3_DSInfo;  // 0x0B0
#endif
    struct NX_DDRPHY_DRVDSInfo      PHY_DSInfo;     // 0x0B4 ~ 0x0BC

    U16 LvlTr_Mode;                 // 0x0C0 ~ 0x0C1
    U16 FlyBy_Mode;                 // 0x0C2 ~ 0x0C3

    U32 Stub[(0x1EC-0x0C4)/4];      // 0x0C4 ~ 0x1EC
#endif
#if defined(ARCH_NXP5430)
    U32 DVO[9];                     // 0x074 ~ 0x094

    struct NX_DDRInitInfo DII;      // 0x098 ~ 0x0BC

#if defined(MEM_TYPE_DDR3)
    struct NX_DDR3DEV_DRVDSInfo     DDR3_DSInfo;    // 0x0C0
#endif
#if defined(MEM_TYPE_LPDDR23)
    struct NX_LPDDR3DEV_DRVDSInfo   LPDDR3_DSInfo;  // 0x0C0
#endif
    struct NX_DDRPHY_DRVDSInfo      PHY_DSInfo;     // 0x0C4 ~ 0x0CC

    U16 LvlTr_Mode;                 // 0x0D0 ~ 0x0D1
    U16 FlyBy_Mode;                 // 0x0D2 ~ 0x0D3

    U32 Stub[(0x1EC-0x0D4)/4];      // 0x0D4 ~ 0x1EC
#endif

    U32 MemTestAddr;                // 0x1EC
    U32 MemTestSize;                // 0x1F0
    U32 MemTestTryCount;            // 0x1F4

    U32 BuildInfo;                  // 0x1F8

    U32 SIGNATURE;                  // 0x1FC    "NSIH"
} __attribute__ ((packed,aligned(4)));

//------------------------------------------------------------------------------------------------------//

void usage(void);
void print_bingen_info( void );


#endif
