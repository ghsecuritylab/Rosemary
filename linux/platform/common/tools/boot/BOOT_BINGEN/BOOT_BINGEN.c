#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>

#include "BOOT_BINGEN.h"
#include "GEN_NANDBOOTEC.h"

#define DEBUG			0
#define DUMP_DEBUG		0

struct NX_SecondBootInfo* pBootInfo;

static U8* g_cpu_name		= NULL;
static U8* g_option_name	= NULL;
static U8* g_boot_mode		= NULL;
static U8* g_input_name 	= NULL;
static U8* g_nsih_name 		= NULL; 
static U8* g_output_name	= NULL;

static U8* SwapEnb			= NULL;

static U32 g_device_addr    = CFAILED;
static U32 g_device_portnum	= CFAILED;

static U32 g_OutputSize		= NULL; 
static U32 g_InputSize		= NULL;

static U32 g_load_addr		= CFAILED;
static U32 g_launch_addr	= CFAILED;

static U8* g_view_option	= NULL;

// NAND Relation Variable.
#define ROMBOOT_MAXPAGE		(4096)

static U32 g_nand_sectorsize	= 0;
static U32 g_nand_pagesize		= 0;					/* UNIT: 512B 	*/ 
static U32 g_nand_offset		= 1;					/* UNIT: 1MB 	*/
static U32 g_nand_repeat		= 1;					/* UNIT: Count 	*/


/* Data Dump */
static void print_hexdump( U32* pdwBuffer, U32 Size )
{
	register S32 i = 0;
	
	for(i = 0; i < Size/4; i++)
	{
		printf("%8X  ", pdwBuffer[i] );
		if( ((i+1) % 8) == 0 )
			printf("\r\n");	
	}
	printf("\r\n");
}

/* It Converted to Uppercase.  */
static void to_upper( char* string )
{
    char*   str = (char*)string;
    S32     ndx = 0;

    for(ndx = 0; ndx < strlen(str); ndx++)
    {
        str[ndx] = (char)toupper(str[ndx]);
    }
}

/* It Converted to Lowercase.  */
static void to_lower( char* string )
{
    char*   str = (char*)string;
    S32     ndx = 0;

    for(ndx = 0; ndx < strlen(str); ndx++)
    {
        str[ndx] = (char)tolower(str[ndx]);
    }
}

/* Convert hexadecimal strings to integers.  */
U32 hexa_toint( const char *string )
{
	char 	ch;
	U32 	result = 0;

	while( (ch = *string++) != 0 )
	{
		if( ch >= '0' && ch <= '9' )
		{
			result = result * 16 + (ch - '0');
		}
		else if( ch >= 'a' && ch <= 'f' )
		{
			result = result * 16 + (ch - 'a') + 10;
		}
		else if( ch >= 'A' && ch <= 'F' )
		{
			result = result * 16 + (ch - 'A') + 10;
		}
	}

	return result;
}

static size_t get_file_size( FILE *fd )
{
	size_t fileSize;
	long curPos;

	curPos = ftell( fd );

	fseek(fd, 0, SEEK_END);
	g_InputSize = fileSize = ftell( fd );
	fseek(fd, curPos, SEEK_SET );

	return fileSize;
}

/*                     4 bytes of CRC value calculated on the generation.                     */
/* --------------------------------------------------------- */
static inline U32 iget_fcs(U32 fcs, U32 data)
{
	register S32 i;
	fcs ^= data;
	for(i=0; i<32; i++)
	{
		if(fcs & 0x01)
			fcs ^= POLY;
		fcs >>= 1;
	}
	return fcs;
}

static inline U32 __icalc_crc (void *addr, S32 len)
{
	U32 *c = (U32*)addr;
	U32 crc = 0, chkcnt = ((len+3)/4);
	U32 i;

	for (i = 0; chkcnt > i; i += CHKSTRIDE, c += CHKSTRIDE) {
		crc = iget_fcs(crc, *c);
	}

	return crc;
}
/* --------------------------------------------------------- */


/*                     1 bytes of CRC value calculated on the generation.                     */
/* ---------------------------------------------------------  */
static U32 globalk = 0;
static inline U32 get_fcs(U32 fcs, U8 data)
{
	register S32 i;
	fcs ^= (U32)data;
	for(i = 0; i < 8; i++)
	{
	   if(fcs & 0x01) 
	   		fcs ^= POLY; 
	   fcs >>= 1;
	}
#if 0
	if( fcs == 0x01729E99 )
		printf("[%d]St FCS Match : %8X\r\n", globalk, fcs );
	else
		printf("[%d]St FCS Not Match : %8X\r\n", globalk, fcs );
	globalk += 1;
//#else
	if( ((globalk++) % 8) == 0 )
		printf("\r\n[%d]: ", globalk);
	printf("%8X ", fcs );
#endif
	return fcs;
}

static inline U32 __calc_crc (void *addr, S32 len)
{
	U8 *c = (U8*)addr;
	U32 crc = 0;
	S32 i;
	for (i = 0; len > i; i++)
	{
		crc = get_fcs(crc, c[i]);
	#if 0
		printf("[%8X]crc : %08X(%02X) \r\n", i, crc, c[i] );
		if( crc == 0x029F2953 )
		{
		    printf("Success \r\n");
		    break;
		}
	#endif	
	}
	return crc;
}
/* ---------------------------------------------------------  */


// Nexell System Information Header
//------------------------------------------------------------------------------
S32 process_nsih( const char *pfilename, U8 *pOutData )
{
	FILE	*fp;
	char ch;
	S32 writesize, skipline, line, bytesize, i;
	U32 writeval;

	fp = fopen( pfilename, "rb" );
	if( !fp )
	{
		printf( "process_nsih : ERROR - Failed to open %s file.\n", pfilename );
		return 0;
	}

	bytesize = 0;
	writeval = 0;
	writesize = 0;
	skipline = 0;
	line = 0;

	while( 0 == feof( fp ) )
	{
		ch = fgetc( fp );

		if( skipline == 0 )
		{
			if( ch >= '0' && ch <= '9' )
			{
				writeval = writeval * 16 + ch - '0';
				writesize += 4;
			}
			else if( ch >= 'a' && ch <= 'f' )
			{
				writeval = writeval * 16 + ch - 'a' + 10;
				writesize += 4;
			}
			else if( ch >= 'A' && ch <= 'F' )
			{
				writeval = writeval * 16 + ch - 'A' + 10;
				writesize += 4;
			}
			else
			{
				if( writesize == 8 || writesize == 16 || writesize == 32 )
				{
					for( i=0 ; i<writesize/8 ; i++ )
					{
						pOutData[bytesize++] = (U8)(writeval & 0xFF);
						writeval >>= 8;
					}
				}
				else
				{
					if( writesize != 0 )
						printf("process_nsih : Error at %d line.\n", line+1 );
				}
				writesize = 0;
				skipline = 1;
			}
		}

		if( ch == '\n' )
		{
			line++;
			skipline = 0;
			writeval = 0;
		}
	}
#if 0
	NX_DEBUG( "process_nsih : %d line processed. \n", line+1 );
	NX_DEBUG( "process_nsih : %d bytes generated.\n", bytesize );
#endif
	fclose( fp );

	return bytesize;
}

static int nsih_parsing( S8* nsih_name )
{
	S8* buf 				= (S8*)malloc( NSIHSIZE );
	if( NSIHSIZE != process_nsih( nsih_name, buf ) )
	{
		NX_ERROR("Failed to process NSIH.\n" );
		return -1;
	}	
	
	pBootInfo = (struct NX_SecondBootInfo*)buf;

	return 0;
}

static void nsih_user_config( char* buffer )
{
	CRC uCrc;
	S32 i 		= 0;

	/*	CRC Check for SPI, SD, UART, ETC */
	
	/*	2ndboot & 3rdboot Header Modify Information. */
	//---------------------------------------------------------------------------------------
	//if( (0 == strcmp( g_option_name, "3rdboot" )) || (0 == strcmp( g_option_name, "2ndboot" )) )
	{	
		pBootInfo = (struct NX_SecondBootInfo*)buffer;

		// Device Dependency
		//------------------------------------------------------------------------//
		// Not Required Option (NSIH Default)
		//-------------------------------------------------------------------------
		if( g_device_portnum != CFAILED )
			pBootInfo->DBI.SDMMCBI.PortNumber = (U8)g_device_portnum; 	// Each Device Port 
		if( g_device_addr != CFAILED )
			pBootInfo->DEVICEADDR = g_device_addr;						// Each Device Address
		//------------------------------------------------------------------------
		// Output Size
		pBootInfo->LOADSIZE 		= g_InputSize;
		//------------------------------------------------------------------------//	
		// NSIH Default ( Load Address, Launch Address )
		if( g_load_addr != CFAILED )
			pBootInfo->LOADADDR 		= g_load_addr; 
		if( g_launch_addr != CFAILED )
			pBootInfo->LAUNCHADDR		= g_launch_addr;

		pBootInfo->SIGNATURE			= HEADER_ID;		// Signature (NSIH)
		//------------------------------------------------------------------------//
		pBootInfo->DBI.SDMMCBI.CRC32 = __calc_crc((void*)(buffer + NSIHSIZE), (g_InputSize) );
		/* CRC - 2ndboot (16KB-16  ) */
		// NXP4330 && Secondboot
		if( (g_InputSize + NSIHSIZE) <= (NXP4330_SRAM_SIZE-16) )
		{
			uCrc.iCrc = __calc_crc((void*)(buffer), (NXP4330_SRAM_SIZE-16) );		
			for(i = 0; i < CRCSIZE; i++)
			{	
				buffer[NXP4330_SRAM_SIZE-16+i] = uCrc.chCrc[i];
    		#if 0
				printf("(BASEADDR + 0x%X) - CRC[%d]: %X(%X) \r\n", 
				(g_OutputSize-16+i), i, buffer[g_OutputSize-16+i], uCrc.chCrc[i]);
    		#endif
			}
		}		
		//------------------------------------------------------------------------//

#ifdef BOOT_DEBUG
		NX_DEBUG("LOADSIZE		: %8X \r\n", pBootInfo->LOADSIZE   );
		NX_DEBUG("LOADADDR		: %8X \r\n", pBootInfo->LOADADDR   );
		NX_DEBUG("LAUNCHADDR	: %8X \r\n", pBootInfo->LAUNCHADDR );
		NX_DEBUG("SIGNATURE 	: %8X \r\n", pBootInfo->SIGNATURE  );
		NX_DEBUG("CRC32 		: %8X \r\n", pBootInfo->DBI.SDMMCBI.CRC32 );
#endif
	}

}

static void nsih_get_bootmode( void )
{
	//-------------------------------------------------
	// Boot Mode Information
	//-------------------------------------------------
	U8  BoodMode = (U8)(pBootInfo->DBI.SPIBI.LoadDevice);
	if( BoodMode == BOOT_FROM_USB )
		g_boot_mode = "USB";
	else if( BoodMode == BOOT_FROM_SPI )
		g_boot_mode = "SPI";
	else if( BoodMode == BOOT_FROM_NAND )
		g_boot_mode = "NAND";
	else if( BoodMode == BOOT_FROM_SDMMC )
		g_boot_mode = "SDMMC";
	else if( BoodMode == BOOT_FROM_SDFS )
		g_boot_mode = "SDFS";
	else if( BoodMode == BOOT_FROM_UART )
		g_boot_mode = "UART";	
	//-------------------------------------------------
}

/* A First step for generating a NAND function Bingen image. */
#define NAND_SECTORSIZE		512
void update_nand_bootinfo( struct NX_SecondBootInfo* bootinfo, struct nand_info* info, int bin_size, int crc )
{
	bootinfo->DEVICEADDR			= info->dev_addr;
	bootinfo->LOADSIZE				= bin_size;
	bootinfo->LOADADDR				= info->load_addr;
	bootinfo->LAUNCHADDR			= info->launch_addr;
	bootinfo->SIGNATURE				= HEADER_ID;
	
	bootinfo->DBI.NANDBI.PageSize	= g_nand_pagesize / NAND_SECTORSIZE;
	bootinfo->DBI.NANDBI.TIOffset	= g_nand_offset;
	bootinfo->DBI.NANDBI.CopyCount	= g_nand_repeat;
	bootinfo->DBI.NANDBI.LoadDevice	= BOOT_FROM_NAND;
	bootinfo->DBI.NANDBI.CRC32 		= crc;

	// Global 
	pBootInfo = ((struct NX_SecondBootInfo*)bootinfo);

	printf("NAND Specific Information. 		\n");
	printf("\t- PageSize : %u				\n", g_nand_pagesize); 

	printf("\t- 3rdboot from     : 0x%X		\n", bootinfo->DBI.NANDBI.LoadDevice);
	printf("\t- Boot-Copy Offset : 0x%X		\n", g_nand_offset * (1024 * 1024));
	printf("\t- Boot-Copy Counts : %u		\n", g_nand_repeat);
}


/* A First step for generating a NAND function Bingen image. */
static S32 nand_first_stage( FILE *eccgen_fp, const char *in_file, struct nand_info *info )
{
	FILE	*bin_fp		= NULL;
	FILE	*merge_fp	= NULL;

	U32 BinFileSize, MergeFileSize, SrcLeft, SrcReadSize, DstSize, DstTotalSize;
	U32 BinFileSize_align;
	
	static U32 pdwSrcData[7*NX_BCH_SECTOR_MAX/4];
	static U32 pdwDstData[8*NX_BCH_SECTOR_MAX/4];

	U8 *merge_buffer	= NULL;
	
	S32 is_2ndboot		= info->image_type;
	U32 max_2ndboot		= info->max_2ndboot;
	U32 align;
	S32 crc = 0;
	
	size_t read_size = 0, sz;
	
	S32 ret = 0;

	//--------------------------------------------------------------------------
	// BIN processing
	bin_fp = fopen(in_file, "rb");
	if (!bin_fp)
	{
		NX_ERROR("ERROR : Failed to open %s file.\n", in_file);
		ret = -1;
		goto out;
	}

	BinFileSize = get_file_size (bin_fp);
	NX_DEBUG("[Merge Stage] BinFileSize : %d\n", BinFileSize);

	/* just for convenience, with 2ndboot */
	sz = (is_2ndboot) ? NSIHSIZE : iSectorSize;

	/* get MergeFileSize */
	align = (is_2ndboot) ? max_2ndboot : info->page_size;

	/* prepare merge buffer */
	MergeFileSize		= ALIGN(BinFileSize + sz, align);
	BinFileSize_align	= ALIGN(BinFileSize, sz);
	NX_DEBUG("MergeFileSize: %u, BinFileSize aligned: %u\n", MergeFileSize, BinFileSize_align);

	merge_buffer = malloc(MergeFileSize);
	if (!merge_buffer) 
	{
		NX_ERROR("Not enough memory. (merge)\n");
		ret = -1;
		goto out;
	}
	memset (merge_buffer, 0xff, MergeFileSize);

	/* Parsing NSIH */
	ret = process_nsih (info->nsih_name, merge_buffer);
	if ( ret != NSIHSIZE )
	{
		NX_ERROR("NSIH Parsing failed.\n");
		ret = -1;
		goto out;
	}

	/* read input binary */
	read_size = fread ( merge_buffer + sz, 1, BinFileSize, bin_fp);
	if( read_size != BinFileSize )
	{
		NX_ERROR("File read error.\n");
		ret = -1;
		goto out;
	}


	/* CRC */
	do {
		S32 i;

		for(i = 0; i < BinFileSize_align>>2; i++)
			crc = iget_fcs(crc, *(U32*)(merge_buffer + sz + (i*4)));
	} while(0);

	/* update bootinfo after parsing NSIH */
	update_nand_bootinfo( (struct NX_SecondBootInfo*)merge_buffer, info, BinFileSize_align, crc );
	

	/* write merge buffer */
    /* merge : ParsedNSIH + BinFile */
    merge_fp = tmpfile ();
    if (!merge_fp)
	{
		printf ("Cannot open tmpfile.\n");
		ret = -1;
		goto out;
	}
	fwrite (merge_buffer, 1, MergeFileSize, merge_fp);

	free (merge_buffer);
	merge_buffer = NULL;

	//--------------------------------------------------------------------------
	// EccGen Processing
	rewind ( merge_fp );
	if (is_2ndboot && MergeFileSize > max_2ndboot)
	{
		printf ("WARNING : %s is too big for Second boot image, only will used first %d bytes.\n", in_file, max_2ndboot);
		MergeFileSize = max_2ndboot;
	}

	NX_DEBUG("iSectorSize: %d \n", iSectorSize);

	iNX_BCH_VAR_K		=	(iSectorSize * 8);

	iNX_BCH_VAR_N		=	(((1<<iNX_BCH_VAR_M)-1));
	iNX_BCH_VAR_R		=	(iNX_BCH_VAR_M * iNX_BCH_VAR_T);

	iNX_BCH_VAR_TMAX	=	(60);
	iNX_BCH_VAR_RMAX	=	(iNX_BCH_VAR_M * iNX_BCH_VAR_TMAX);

	iNX_BCH_VAR_R32		=	((iNX_BCH_VAR_R   +31)/32);
	iNX_BCH_VAR_RMAX32	=	((iNX_BCH_VAR_RMAX+31)/32);

	iNX_BCH_OFFSET		= 8;

	SrcLeft = MergeFileSize;

	//--------------------------------------------------------------------------
	// Encoding - ECC Generation
	printf ("\n");

	DstTotalSize = 0;

	// generate the Galois Field GF(2**mm)
	generate_gf ();
	// Compute the generator polynomial and lookahead matrix for BCH code
	gen_poly ();

	while (SrcLeft > 0)
	{
		size_t sz;
		SrcReadSize = (SrcLeft > (iSectorSize*7)) ? (iSectorSize*7) : SrcLeft;
		NX_DEBUG("SrcLeft: 0x%x, SrcReadSize: 0x%x\n", SrcLeft, SrcReadSize);
		sz = fread (pdwSrcData, 1, SrcReadSize, merge_fp);
		SrcLeft -= SrcReadSize;
		DstSize = MakeECCChunk (pdwSrcData, pdwDstData, SrcReadSize);
		fwrite (pdwDstData, 1, DstSize, eccgen_fp);
		
		DstTotalSize += DstSize;
	}

	//--------------------------------------------------------------------------
	printf ("\n");
	printf ("%d bytes(%d sector) generated.\n", DstTotalSize, (DstTotalSize+iSectorSize-1)/iSectorSize);

out:
	if (merge_buffer)
		free (merge_buffer);
	if (bin_fp)
		fclose (bin_fp);
	if (merge_fp)
		fclose (merge_fp);


	return ret;
}

/* A Second step for generating a NAND function Bingen image. */
static S32 nand_second_stage(const char *out_file, FILE *eccgen_fp, const struct nand_info *info)
{
	FILE *out_fp;
	U8 *copy_buffer					= NULL;

	U32 nand_pagesize				= 0;
	S32 payload_in_page				= 0;
	S32 bin_filesize					= 0;
	S32 copy_count					= 0;
	
	S32 ret = 0 , i;

	/* buffer allocation */
	nand_pagesize = payload_in_page = info->page_size;
	copy_buffer = malloc(nand_pagesize);
	if (!copy_buffer)
	{
		ret = -10;
		goto err;
	}

	out_fp = fopen(out_file, "wb");
	if (!out_fp) {
		ret = -20;
		goto err_out_fp;
	}


	/* size calc. */
	rewind (eccgen_fp);
	bin_filesize = get_file_size (eccgen_fp);
	NX_DEBUG("[ECCGEN Stage] Binary File Size: %d \n", bin_filesize);

	if (info->image_type == IS_2NDBOOT)
		payload_in_page = MIN (payload_in_page, ROMBOOT_MAXPAGE);
	copy_count = DIV_ROUND_UP (bin_filesize, payload_in_page);
	NX_DEBUG("PayLoad: %d, Page Size: %d, Copy Count: %d \n", 
		payload_in_page, nand_pagesize, copy_count );

	/* file writing */
	for (i = 0; i < copy_count; i++)
	{
		size_t sz;
		memset (copy_buffer, 0xff, nand_pagesize);

		sz = fread (copy_buffer, 1, payload_in_page, eccgen_fp);
		fwrite (copy_buffer, 1, nand_pagesize, out_fp);
		g_OutputSize += nand_pagesize;
	}

	fclose (out_fp);
err_out_fp:
	free (copy_buffer);
err:
	return ret;
}

/* A function for creating a NAND Bingen image.  */
static S32 nand_make_image(const char *out_file, const char *in_file, struct nand_info *info)
{
	S32 ret = 0;
	FILE *eccgen_fp;

	eccgen_fp = tmpfile();
	if (!eccgen_fp) {
		NX_ERROR("Cannot generate temporary file. Abort.\n");
		goto done;
	}

	ret = nand_first_stage(eccgen_fp, in_file, info);
	if (ret < 0)
		goto done;
	
	ret = nand_second_stage(out_file, eccgen_fp, info);
	if (ret < 0)
		return -1;

done:
	fclose(eccgen_fp);
	
	return ret;
}

/*-------------------------------------------------------------------------------------*/

/* Chip by, calculating an output size based on the size limit on the SRAM.  */
static S32 output_size_calcurate( void )
{
	/* Ourput Size Calcurate */
	//--------------------------------------------------------------------------
	if( 0 == strcmp( g_option_name, "2ndboot" ) )
	{
		if( (0 == strcmp( g_cpu_name, "NXP4330" )) )
		{
		    if( g_InputSize > (NXP4330_SRAM_SIZE - NSIHSIZE) )
            {
                printf("Enter the binary size exceeded 16KB-512Byte.\n");
                printf("Check Filesize!! (%d) \n", g_InputSize );
                return -1;
            }
		    g_OutputSize = NXP4330_SRAM_SIZE;
		}
		else if( (0 == strcmp( g_cpu_name, "NXP5430" )) || (0 == strcmp( g_cpu_name, "S5P6818" )) )
		{
			g_OutputSize = g_InputSize + NSIHSIZE;
			if( g_OutputSize > (NXP5430_SRAM_SIZE - ROMBOOT_STACK_SIZE) )
			{
				printf("The image is Generated exceeds 64KB. The Creation failed! \r\n");
				printf("Calcurate image Size : %d \r\n", g_OutputSize );
				printf("Return Error End!!\r\n");
				return -1;
			}		
		}		
		else if(0 == strcmp( g_cpu_name, "S5P4418" ))
		{
			g_OutputSize = g_InputSize + NSIHSIZE;	
			if( g_OutputSize > (S5P4418_SRAM_SIZE - ROMBOOT_STACK_SIZE) )
			{
				printf("The image is Generated exceeds 28KB. The Creation failed! \r\n");
				printf("Calcurate image Size : %d \r\n", g_OutputSize );
				printf("Return Error End!!\r\n");
                //g_OutputSize = 28*1024;
				return -1;
			}		
			else if( g_OutputSize < NXP4330_SRAM_SIZE ) // S5P 4418 BinGen Size < 16*1024
			{
                g_OutputSize = NXP4330_SRAM_SIZE;
			}
		}
		else
			printf("CPU(Chip) name is unknown. \r\n");     

	}
	else if( 0 == strcmp( g_option_name, "3rdboot" ) )
		g_OutputSize = g_InputSize + NSIHSIZE;
	//--------------------------------------------------------------------------

	/* File Descript Check & Maximum Size */
	//--------------------------------------------------------------------------
	if( g_OutputSize == 0 )	
	{
		printf("Did not enter the Filesize files.\r\n");
		return -1;
	}
    else // 
    {
        if( (g_OutputSize <= (NXP4330_SRAM_SIZE-16)) )
            g_OutputSize = NXP4330_SRAM_SIZE;
    }
	//--------------------------------------------------------------------------
	return 0;
	
}

/* ------------------------------------------------------------------------------ */
/*	  @ Function : Other Boot Bingen (SPI, SD, UART, ETC )			
  *	  @ Param    : None.
  *	  @ Remak   : (SD, SPI, UART, etc.) Except the boot mode, making BINGEN NAND.
  */
static S32 other_boot_bingen( void )
{
	FILE *InFile_fd		= NULL;
	FILE *OutFile_fd	= NULL;

	U8	*Out_Buffer		= NULL;
	U8	*pdwBuffer		= NULL;

	U32	malloc_size		= NULL;

	U32 i = NULL, ret	= TRUE;

	InFile_fd	= fopen( g_input_name , "r" ); 
	OutFile_fd	= fopen( g_output_name, "wb");

	if((!InFile_fd)) 
	{
		NX_ERROR("Input File open failed!! Check filename!!\n");
		ret = FALSE;
		goto ERR_END;
	}

	if((!OutFile_fd))
	{
		NX_ERROR("Output File open failed!! Check filename!!\n");
		ret = FALSE;
		goto ERR_END;
	}

	// File Size Confirm.
	fseek( InFile_fd, 0, SEEK_END );
	g_InputSize = ftell ( InFile_fd );
	fseek( InFile_fd, 0, SEEK_SET );

	/* Output Size Calcurate. */
	if ( 0 > output_size_calcurate() )
		goto ERR_END;

#if 0
	if( (g_OutputSize % 512) != 0 )
		malloc_size = ((g_OutputSize / BLOCKSIZE) + 1) * BLOCKSIZE;
	else
#endif
		malloc_size = g_OutputSize;
		
	//--------------------------------------------------------------------------
	Out_Buffer	= (U8*)malloc( malloc_size );
	memset(Out_Buffer , 0xFF, malloc_size);		// Set 0 to Memory 
	
	/* Read to Process NSIH */
	//--------------------------------------------------------------------------
	if( NSIHSIZE != process_nsih( g_nsih_name, Out_Buffer ) )
	// translate text file to 512B binary
	/* | 512 NSIH | (16KB - 512 - 16) second boot | 4 CRC | 12 dummy |	==> total size is 16KB */
	{
		NX_ERROR("Failed to process NSIH.\n" );
		ret = FALSE;
		goto ERR_END;
	}
	if( 0 > fread(Out_Buffer + NSIHSIZE, 1, g_InputSize, InFile_fd) )
	{
		NX_ERROR("File Read Failed. \r\n");
		return CFAILED;
	}
	//--------------------------------------------------------------------------
	
#if 0
	NX_SHELLU_HexDump( (U32*)Out_Buffer + (NSIHSIZE/4), g_InputSize, 8 );
#endif

	
#if SECURE_BOOT
	// Secure Boot <-- Decript Issue ( 16Byte Convert )
	if( ((g_InputSize % 16) != 0) )	
		g_InputSize = ((g_InputSize / 16) * 16);
#endif
	// NSIH, Set the user you want.
	nsih_user_config( (char*)Out_Buffer );
	// Written information on the boot device.
	nsih_get_bootmode();

	// File image write.
	fwrite( Out_Buffer, 1, g_OutputSize, OutFile_fd );
	// Bingen Debug Mesage (Infomation desk)
	print_bingen_info();										

#if DUMP_DEBUG
	if( (0 == strcmp( g_view_option, "viewer" )) )
	{
		//print_hexdump( (U32*)Out_Buffer + (NSIHSIZE/4), g_InputSize );
		NX_SHELLU_HexDump( (U32*)Out_Buffer, g_OutputSize, 8 );	
	}
#endif
		
	fclose(InFile_fd);
	fclose(OutFile_fd);

	free(Out_Buffer);

	return 0;

ERR_END:
	fclose(InFile_fd);
	fclose(OutFile_fd);

	free(Out_Buffer);	

	return -1;

}

/* ------------------------------------------------------------------------------ */
/*	  @ Function : NAND Boot Bingen (SPI, SD, UART, ETC )			
  *	  @ Param    : None.
  *	  @ Remak   : Dedicated function for making NAND BINGEN.
  */
static S32 nand_boot_bingen( void )
{
	struct nand_info* info 
		= (struct nand_info*)malloc( sizeof(struct nand_info) );
	S32 ret 				= 0;
	
	/* Output Size Calcurate. */
	if ( 0 > output_size_calcurate() )
		return -1;
	
	// Get Sector Size.
	g_nand_sectorsize = (g_nand_pagesize < 1024) ? 512 : 1024;
	if (g_nand_sectorsize == 512) {
		iSectorSize 		=	512;
		iNX_BCH_VAR_T		=	24;
		iNX_BCH_VAR_M		=	13;
	}
	else if (g_nand_sectorsize == 1024) {
		iSectorSize 		=	1024;
		iNX_BCH_VAR_T		=	60;
		iNX_BCH_VAR_M		=	14;
	}
	NX_DEBUG("Page Size: %d, Sector Size: %d \n", g_nand_pagesize, g_nand_sectorsize);

	//--------------------------------------------------------------------
	info->nsih_name 	= g_nsih_name;
	info->load_addr		= g_load_addr;
	info->launch_addr	= g_launch_addr;
	// 2NDBOOT or 3RDBOOT
	//-------------------------------------
	if( !strcmp( g_option_name, "2ndboot") )
		info->image_type	= IS_2NDBOOT;
	else
		info->image_type	= IS_3RDBOOT;
	info->dev_addr		= g_device_addr;
	info->page_size 	= g_nand_pagesize;
	//-------------------------------------
	// Maximum 2ndboot Size.
	if( !strcmp( g_cpu_name, "S5P4418" ) )
		info->max_2ndboot	= NXP4330_SRAM_SIZE;	// 16KB 
	else if( !strcmp( g_cpu_name, "S5P6818" ) )
		info->max_2ndboot	= NXP5430_SRAM_SIZE;	// 64KB
	//-------------------------------------	
	if( 0 > nand_make_image( (const char*)g_output_name, (const char*)g_input_name, info ) )
	{
		NX_ERROR("NAND Image Make Failed! \n");
		return -1;
	} else 
	{
		// NSIH ->  Get Boot Mode.
		nsih_get_bootmode();
		// Bingen Debug Mesage (Infomation desk)
		print_bingen_info();
	}
	//--------------------------------------------------------------------
	return ret;
	
}

/* ------------------------------------------------------------------------------ */
/*	  @ Function : useage.		
  *	  @ Param    : None.
  *	  @ Remak   : Help on useage.
  */
void usage( void )
{
	printf("--------------------------------------------------------------------------\n");
	printf(" Release  Version         : Ver.%04d                                      \n", BOOT_BINGEN_VER );
	printf(" Author & Version Manager : Deoks (S/W 1Team)                             \n");
	printf("--------------------------------------------------------------------------\n");
	
	printf(" Usage : This will tell you How to Use Help.					          \n");
	printf("--------------------------------------------------------------------------\n" );
	printf("   -h [HELP]                     : show usage                             \n");
	printf("   -c [S5P6818/S5P4418]          : chip name         (mandatory)          \n");	
	printf("   -t [2nboot/3rdboot]           : What is the Boot? (mandatory)          \n");
	printf("   	->[2ndboot]                                                           \n");
	printf("   	->[3rdboot]              	   	    	                              \n");
	printf("   -n [file name]                : [NSIH] file name	 (mandatory)     	  \n");
	printf("   -i [file name]                : [INPUT]file name  (mandatory)		  \n");
	printf("   -o [file name]                : [OUTPUT]file name (mandatory)    	  \n");
	printf("   -l [load address]             : Binary Load  Address              	  \n");
	printf("   	-> Default Load	  Address : Default NSIH.txt   				          \n");			
	printf("   -e [launch address]           : Binary Launch Addres             	  \n");
	printf("   	-> Default Launch Address : Default NSIH.txt   				          \n");
	printf("   -p [nand pagesize]            : NAND - Page Size                       \n");
	printf("   	-> Default Page Size      : Default NSIH.txt (Zero)			          \n");
	printf("   -f [nand offset]              : NAND - Offset (MB)Size                 \n");
	printf("   	-> Default Offset (MB)Size: Default NSIH.txt (Zero)			          \n");
	printf("   -r [nand repeat count]        : Binary Launch Addres             	  \n");
	printf("   	-> Default Repeat Count   : Default NSIH.txt (Zero)			          \n");	
	printf("--------------------------------------------------------------------------\n");
    printf("   This Option is Not Required. (if you do not use NSIH.txt Default.)     \n");
    printf("--------------------------------------------------------------------------\n");
	printf("   -a [device address]           : What is the Device Address?   		  \n");
	printf("   -u [device port]              : device channel                         \n");
    printf("--------------------------------------------------------------------------\n");
    printf(" Remark & Reference Message                                               \n");
    printf(" The current version has not been applied to the NAND BINGEN version..    \n");
    printf("--------------------------------------------------------------------------\n");
	printf("\n");
	printf(" Usage: How to use the program? 			                              \n"); 
	printf(" Ubuntu  > How to use?                                                    \n");
	printf("  #>./BOOT_BINGEN -h 0 or ./BOOT_BINGEN \n");
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 2ndboot -n NXP4330_NSIH_V05_spi_800.txt -i pyrope_2ndboot_spi.bin -o 2ndboot_spi.bin               \n" );
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 2ndboot -n NXP4330_NSIH_V05_sd_800.txt  -i pyrope_2ndboot_sdmmc.bin -o 2ndboot_sdmmc.bin           \n" );
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 3rdboot -n NXP4330_NSIH_V05_spi_800.txt -i u-boot.bin -o 3rdboot_spi.bin -l 40100000 -e 40100000   \n" );
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 3rdboot -n NXP4330_NSIH_V05_sd_800.txt  -i u-boot.bin -o 3rdboot_sdmmc.bin -l 40100000 -e 40100000 \n" );
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 2ndboot -n nsih_svt_nand.txt -i 2ndboot.bin -o 2nddboot_nand.bin -l FFFF0000 -e FFFF0000           \n" );
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 3rdboot -n nsih_svt_nand.txt -i u-boot.bin  -o 3rdboot_nand.bin  -l FFFF0000 -e FFFF0000           \n" );	
    printf("\n");
	printf(" Windows > How to use?                                                    \n");
    printf("  #>BOOT_BINGEN.exe -h 0 or BOOT_BINGEN.exe \n");
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 2ndboot -n NXP4330_NSIH_V05_spi_800.txt -i pyrope_2ndboot_spi.bin -o 2ndboot_spi.bin               \n" );
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 2ndboot -n NXP4330_NSIH_V05_sd_800.txt  -i pyrope_2ndboot_sdmmc.bin -o 2ndboot_sdmmc.bin           \n" );
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 3rdboot -n NXP4330_NSIH_V05_spi_800.txt -i u-boot.bin -o 3rdboot_spi.bin -l 40100000 -e 40100000   \n" );
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 3rdboot -n NXP4330_NSIH_V05_sd_800.txt  -i u-boot.bin -o 3rdboot_sdmmc.bin -l 40100000 -e 40100000 \n" );	
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 2ndboot -n nsih_svt_nand.txt -i 2ndboot.bin -o 2nddboot_nand.bin -l FFFF0000 -e FFFF0000           \n" );
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 3rdboot -n nsih_svt_nand.txt -i u-boot.bin  -o 3rdboot_nand.bin  -l FFFF0000 -e FFFF0000           \n" );

	printf("\n");
}

/* ------------------------------------------------------------------------------ */
/*	  @ Function :	
  *	  @ Param    : None.
  *	  @ Remak   : Boot Bingen Generation Information.	
  */
void print_bingen_info( void )
{
	printf( "----------------------------------------------------\n" );
	printf( " %s %s Binary file Information.\n", g_cpu_name, g_option_name );
	printf( " %s Binary Boot Mode 	 		\n", g_boot_mode );
	printf( " NSIH   Text   File : %s		\n", g_nsih_name   ? (char*)g_nsih_name   : "NULL" );
	printf( " Input  Binary File : %s		\n", g_input_name  ? (char*)g_input_name  : "NULL" );
	printf( " Output Binary File : %s		\n", g_output_name ? (char*)g_output_name : "NULL" );
	printf( " Input  Binary Size : %d Byte (%dKB)	\n", g_InputSize   ? g_InputSize   : 0, (g_InputSize+1024-1)/1024 );
	printf( " Output Binary Size : %d Byte (%dKB)   \n", g_OutputSize  ? g_OutputSize  : 0, (g_OutputSize+1024-1)/1024 );
	printf( "----------------------------------------------------\n" );
	printf( " NSIH(Header) Information.  \n" );
	printf( " %s Binary Boot Mode 	\n", g_boot_mode );
	
	if( (0 == strcmp( g_boot_mode, "SPI" )) 
	|| (pBootInfo->DBI.SPIBI.LoadDevice == 0x01 ) )
		printf( "  -> Addr  Step : %8Xh \r\n", pBootInfo->DBI.SPIBI.AddrStep );
	else
		printf( "  -> DevicePort : %8Xh \r\n", pBootInfo->DBI.SDMMCBI.PortNumber );
	
	printf( "  -> DeviceAddr : %8Xh \r\n", pBootInfo->DEVICEADDR );
	printf( "  -> LoadSize   : %8Xh \r\n", pBootInfo->LOADSIZE   );
	printf( "  -> LoadAddr   : %8Xh \r\n", pBootInfo->LOADADDR   );
	printf( "  -> LauchAddr  : %8Xh \r\n", pBootInfo->LAUNCHADDR );
	printf( "  -> SigNature  : %8Xh \r\n", pBootInfo->SIGNATURE );
	printf( "  -> CRC32	: %8Xh \r\n", pBootInfo->DBI.SDMMCBI.CRC32 );
}


S32 main( S32 argc, char **argv )
{
	U32	param_opt 		= NULL;
	S8  boot_mode		= NULL;

	/* Pre-Fix  Default (Name & Value) */
	g_cpu_name		= "S5P4418";
	g_option_name	= "2ndboot";
	g_boot_mode		= ".";

	g_nsih_name		= "NSIH.txt"; 
	g_input_name	= "pyrope_2ndboot_spi.bin";
	g_output_name	= "2ndboot_spi.bin";

	g_OutputSize	= (16*1024);
	g_nand_pagesize	= ( 4*1024);
	g_nand_sectorsize = 512;

    if( argc == CTRUE )
    {
        usage();
        return CTRUE;
    }

	while( -1 !=(param_opt = getopt( argc, argv, "hc:t:n:i:o:l:e:a:u:v:p:f:r:")))
	{
      	switch(param_opt)
      	{
	      	case 'h':
	      		usage();
	      		return CTRUE;
	      	case 'c':
	      		g_cpu_name	  = strdup(optarg);
	      		to_upper(g_cpu_name);
	      		break;				
			case 't':
				g_option_name = strdup(optarg);	
				to_lower(g_option_name);
				break;			
	      	case 'n':  
	        	g_nsih_name  = strdup(optarg);
	        	break;
	      	case 'i':
	      		g_input_name = strdup(optarg);
	        	break;
	      	case 'o':
	      		g_output_name = strdup(optarg);
	        	break;      	
			case 'l':
				g_load_addr	= hexa_toint(optarg);
				break;
	      	case 'e':
	      		g_launch_addr	= hexa_toint(optarg);
	        	break;
            // Not Required Option			
	      	case 'u':
	      		g_device_portnum = hexa_toint(optarg);
	        	break;	  			
			case 'a':
				g_device_addr	 = hexa_toint(optarg);					
				break;		
			// NAND Option Parameter.
			case 'p':
				g_nand_pagesize = strtoul (optarg, NULL, 10);
				break;
			case 'f':
				g_nand_offset = strtoul (optarg, NULL, 10);
				break;
			case 'r':
				g_nand_repeat = strtoul (optarg, NULL, 10);	
				break;
            // Debug Option
			case 'v':
				g_view_option	= strdup(optarg);
				break;				
			// Unknown Option
			default:
	        	printf("unknown option_num parameter\r\n");
	        	break;
    	}
    }

    if( g_nsih_name == NULL )
    {
		g_nsih_name = "NSIH.txt";
		NX_DEBUG("Did not enter the NSIH files.\r\n");
		NX_DEBUG("This has been used as the default NSIH file.\r\n");
    }

	if( g_input_name == NULL )
	{
		g_input_name = "pyrope_secondboot.bin";
		NX_DEBUG("Did not enter the Binary files.\r\n");
		NX_DEBUG("This has been used as the default pyrope_secondboot.bin.\r\n");
	}

	// NSIH Parsing
	nsih_parsing( (S8*)g_nsih_name );
	// Booting Mode ( SD, SPI, UART, MMC, )
	boot_mode = (U8)(pBootInfo->DBI.SPIBI.LoadDevice);
	if( boot_mode != BOOT_FROM_NAND )
	{
		NX_DEBUG("BOOT BINGEN!! \n");
		if ( 0 > other_boot_bingen() )
		{
			NX_ERROR("BOOT BINGEN Failed \n");
			return -1;
		}	
	} else
	{	
		NX_DEBUG("NAND BOOT BINGEN!! \n");
		if( 0 > nand_boot_bingen() )
		{
			NX_ERROR("NAND BOOT BINGEN Failed \n");
			return -1;
		}		
	}

	return 0;

}



