//------------------------------------------------------------------------------
//
//  Copyright (C) 2014 Nexell Co. All Rights Reserved
//  Nexell Co. Proprietary & Confidential
//
//  NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//  Module      :
//  File        :
//  Description :
//  Author      : 
//  Export      :
//  History     :
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <nx_fourcc.h>
#include <nx_dsp.h>
#include <nx_jpeg.h>

#include "NX_JpegDecode.h"

#define JPEG_FORMAT_STRING(A) 							\
	(A == NX_JPEG_FORMAT_YUV444)  		? "YUV444"  :	\
	(A == NX_JPEG_FORMAT_YUV422)  		? "YUV422"  :	\
	(A == NX_JPEG_FORMAT_YUV420)  		? "YUV420"  :	\
	(A == NX_JPEG_FORMAT_YUVGRAY) 		? "YUVGRAY" :	\
	(A == NX_JPEG_FORMAT_YUV440)  		? "YUV440"  :	\
	"Unknown"

NX_JPEG_HANDLE 			g_hJpeg = NULL;
NX_VID_MEMORY_HANDLE 	g_hVidMem = NULL;
DISPLAY_HANDLE			g_hDsp = NULL;

void JpegDecodeStart( uint8_t *pFileName )
{
	NX_JPEG_HEADER_INFO 	headerInfo;
	NX_JPEG_BUFFER_ALIGN 	alignInfo;

	g_hJpeg = NX_JpegOpen( pFileName );
	if( NULL == g_hJpeg )	return ;
	
	memset( &headerInfo, 0x00, sizeof(NX_JPEG_HEADER_INFO) );
	memset( &alignInfo, 0x00, sizeof(NX_JPEG_BUFFER_ALIGN) );

	NX_JpegGetHeaderInfo( g_hJpeg, &headerInfo );
	NX_JpegGetYuvBufferAlign( g_hJpeg, &alignInfo );

	printf("header info : width( %d ), height( %d ), format( %s )\n", headerInfo.width, headerInfo.height, JPEG_FORMAT_STRING(headerInfo.format));

	uint8_t *outBuf;
	int64_t outSize;
	outSize = NX_JpegRequireBufSize( g_hJpeg, NX_JPEG_DECODE_FORMAT_YUV );
	outBuf 	= (uint8_t*)malloc( outSize );

	NX_JpegDecode( g_hJpeg, outBuf, NX_JPEG_DECODE_FORMAT_YUV );

	int64_t bufPos = 0, memPos = 0;
	int32_t i = 0;

	g_hVidMem = NX_VideoAllocateMemory( 4096, headerInfo.width, headerInfo.height, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );

	memPos = 0;
	for( i = 0; i < alignInfo.luHeight; i++ ) {
		memcpy( (char*)g_hVidMem->luVirAddr + memPos, outBuf + bufPos, alignInfo.luWidth );
		memPos += g_hVidMem->luStride;
		bufPos += alignInfo.luWidth;
	}
	memPos = 0;
	for( i = 0; i < alignInfo.cbHeight; i++ ) {
		memcpy( (char*)g_hVidMem->cbVirAddr + memPos, outBuf + bufPos, alignInfo.cbWidth );
		memPos += g_hVidMem->cbStride;
		bufPos += alignInfo.cbWidth;
	}
	memPos = 0;
	for( i = 0; i < alignInfo.crHeight; i++ ) {
		memcpy( (char*)g_hVidMem->crVirAddr + memPos, outBuf + bufPos, alignInfo.crWidth );
		memPos += g_hVidMem->crStride;
		bufPos += alignInfo.crWidth;
	}

	DISPLAY_INFO	dspInfo;
	DSP_IMG_RECT	srcRect;
	DSP_IMG_RECT	dstRect;

	memset( &dspInfo, 0x00, sizeof(dspInfo) );
	srcRect.left 		= 0;
	srcRect.top 		= 0;
	srcRect.right		= headerInfo.width;
	srcRect.bottom		= headerInfo.height;

	dstRect.left 		= 0;
	dstRect.top 		= 0;
	dstRect.right		= 800;
	dstRect.bottom		= 600;

	dspInfo.port		= 0;
	dspInfo.module		= 0;

	dspInfo.width		= headerInfo.width;
	dspInfo.height		= headerInfo.height;
	dspInfo.dspSrcRect	= srcRect;
	dspInfo.dspDstRect	= dstRect;

	dspInfo.numPlane = 1;
	
	g_hDsp = NX_DspInit( &dspInfo );
	NX_DspQueueBuffer( g_hDsp, g_hVidMem );

	if( outBuf ) free(outBuf);
}

void JpegDecodeStop( void )
{
	if( g_hDsp ) 	NX_DspClose( g_hDsp );
	if( g_hVidMem ) NX_FreeVideoMemory( g_hVidMem );
	if( g_hJpeg ) 	NX_JpegClose( g_hJpeg );
}
