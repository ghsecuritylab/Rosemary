#include <stdio.h>
#include <string.h>
#include <unistd.h>		    //	getopt & optarg
#include <stdlib.h>         //	atoi
#include <sys/time.h>		//	gettimeofday
#include <math.h>

#include <nx_fourcc.h>
#include <nx_vip.h>			//	VIP
#ifndef ANDROID
#include <nx_dsp.h>			//	Display
#endif
#include <nx_video_api.h>	//	Video En/Decoder

#include "NX_Queue.h"
#include "Util.h"

#define	MAX_SEQ_BUF_SIZE		(4*1024)
#define	MAX_ENC_BUFFER			8
#define	ENABLE_NV12				1
#define VIP_PORT				0		// s5p4418 = 0

#define	ENABLE_DISPLAY			0

//#define TEST_CHG_PARA


//
//	Display Position
//
static int32_t giX = 0, giY = 0, giWidth = 1024, giHeight = 600;		//	Drone Board.

static float GetPSNR (uint8_t *pbyOrg, uint8_t *pbyRecon, int32_t iWidth, int32_t iHeight, int32_t iStride)
{
	int32_t  i, j;
	float    fPSNR_L = 0;

	for (i = 0; i < iHeight ; i++) {
		for (j = 0; j < iWidth ; j++) {
			fPSNR_L += (*(pbyOrg + j) - *(pbyRecon + j)) * (*(pbyOrg + j) - *(pbyRecon + j));
		}
		pbyOrg   += iStride;
		pbyRecon += iWidth;
	}

	// L
	fPSNR_L = (float) fPSNR_L / (float) (iWidth * iHeight);
	fPSNR_L = (fPSNR_L)? 10 * (float) log10 ((float) (255 * 255) / fPSNR_L): (float) 99.99;

	return fPSNR_L;
}

//
//	pSrc : Y + U(Cb) + V(Cr) (IYUV format)
//
static int32_t LoadImage( uint8_t *pSrc, int32_t w, int32_t h, NX_VID_MEMORY_INFO *pImg )
{
	int32_t i, j;
	uint8_t *pDst, *pCb, *pCr;

	//	Copy Lu
	pDst = (uint8_t*)pImg->luVirAddr;
	for( i=0 ; i<h ; i++ )
	{
		memcpy(pDst, pSrc, w);
		pDst += pImg->luStride;
		pSrc += w;
	}

	pCb = pSrc;
	pCr = pSrc + w*h/4;


	switch( pImg->fourCC )
	{
		case FOURCC_NV12:
		{
			uint8_t *pCbCr;
			pDst = (uint8_t*)pImg->cbVirAddr;
			for( i=0 ; i<h/2 ; i++ )
			{
				pCbCr = pDst + pImg->cbStride*i;
				for( j=0 ; j<w/2 ; j++ )
				{
					*pCbCr++ = *pCb++;
					*pCbCr++ = *pCr++;
				}
			}
			break;
		}
		case FOURCC_NV21:
		{
			uint8_t *pCrCb;
			pDst = (uint8_t*)pImg->cbVirAddr;
			for( i=0 ; i<h/2 ; i++ )
			{
				pCrCb = pDst + pImg->cbStride*i;
				for( j=0 ; j<w/2 ; j++ )
				{
					*pCrCb++ = *pCr++;
					*pCrCb++ = *pCb++;
				}
			}
			break;
		}
		case FOURCC_MVS0:
		case FOURCC_YV12:
		case FOURCC_IYUV:
		{
			//	Cb
			pDst = (uint8_t*)pImg->cbVirAddr;
			for( i=0 ; i<h/2 ; i++ )
			{
				memcpy(pDst, pCb, w/2);
				pDst += pImg->cbStride;
				pCb += w/2;
			}

			//	Cr
			pDst = (uint8_t*)pImg->crVirAddr;
			for( i=0 ; i<h/2 ; i++ )
			{
				memcpy(pDst, pCr, w/2);
				pDst += pImg->crStride;
				pCr += w/2;
			}
			break;
		}
	}
	return 0;
}

#ifdef TEST_CHG_PARA
static void TestChangeParameter( ENC_APP_DATA *pAppData, NX_VID_ENC_HANDLE hEnc, int32_t frameCnt )
{
	NX_VID_ENC_CHG_PARAM stChgParam = {0,};

	if (frameCnt == 0)
	{
		printf(" <<< Test Change Parameter >>> \n");
	}
	else if (frameCnt == 200)
	{
		stChgParam.chgFlg = VID_CHG_GOP;
		stChgParam.gopSize = pAppData->gop >> 1;
		printf("Change From 200Frm : GOP Size is half (%d -> %d) \n", pAppData->gop, stChgParam.gopSize );
		NX_VidEncChangeParameter( hEnc, &stChgParam );
	}
	else if (frameCnt == 400)
	{
		stChgParam.chgFlg = VID_CHG_BITRATE | VID_CHG_GOP | VID_CHG_VBV;
		stChgParam.bitrate = ( pAppData->kbitrate >> 1 ) * 1024;
		stChgParam.gopSize = pAppData->gop;
		stChgParam.rcVbvSize = 0;
		printf("Change From 400Frm : BPS is half (%d -> %d) \n", pAppData->kbitrate, stChgParam.bitrate );
		NX_VidEncChangeParameter( hEnc, &stChgParam );
	}
	else if (frameCnt == 600)
	{
		stChgParam.chgFlg = VID_CHG_FRAMERATE | VID_CHG_BITRATE | VID_CHG_VBV;
		stChgParam.bitrate = pAppData->kbitrate * 1024;
		stChgParam.fpsNum = pAppData->fpsNum >> 1;
		stChgParam.fpsDen = pAppData->fpsDen;
		stChgParam.rcVbvSize = 0;
		printf("Change From 600Frm : FPS is half (%d, %d) \n", pAppData->fpsNum, stChgParam.fpsNum );
		NX_VidEncChangeParameter( hEnc, &stChgParam );
	}
	else if (frameCnt == 800)
	{
		stChgParam.chgFlg = VID_CHG_BITRATE | VID_CHG_GOP | VID_CHG_FRAMERATE | VID_CHG_VBV;
		stChgParam.bitrate = ( pAppData->kbitrate << 2 ) * 1024;
		stChgParam.gopSize = pAppData->gop >> 2;
		stChgParam.fpsNum = pAppData->fpsNum;
		stChgParam.fpsDen = pAppData->fpsDen;
		stChgParam.rcVbvSize = 0;
		printf("Change From 800Frm : BPS is quadruple & gop is quarter (%d -> %d, %d -> %d) \n", pAppData->kbitrate, stChgParam.bitrate, pAppData->gop, stChgParam.gopSize );
		NX_VidEncChangeParameter( hEnc, &stChgParam );
	}
}
#endif

//	Camera Encoder Main
static int32_t VpuCamEncMain( CODEC_APP_DATA *pAppData )
{
	unsigned char *seqBuffer = (unsigned char *)malloc( MAX_SEQ_BUF_SIZE );			// SPS/PPS or Jpeg Header
	int seqSize = 0;

	//	Display
#if (ENABLE_DISPLAY)
	DISPLAY_HANDLE hDsp;
#endif
	NX_VID_ENC_HANDLE hEnc;
	uint64_t StrmTotalSize = 0;

	//	Set Image & Clipper Information
	int32_t cropX = 0;
	int32_t cropY = 0;
	int32_t cropW = pAppData->width;
	int32_t cropH = pAppData->height;

	// out/Log File Open
	FILE *fdOut = fopen( pAppData->outFileName, "wb" );
	FILE *fdLog = fopen( pAppData->outLogFileName, "wb" );
	FILE *fdRecon = ( pAppData->outImgName ) ? fopen( pAppData->outImgName, "wb" ) : NULL;

	if ( fdOut == NULL )
	{
		printf("input file or output file open error!!\n");
		exit(-1);
	}

	//==============================================================================
	// INITIALIZATION
	//==============================================================================
	{
		NX_VID_ENC_INIT_PARAM encInitParam;											// Encoder Parameters

		// Initialize Encoder
		if ( pAppData->codec == 0 ) pAppData->codec = NX_AVC_ENC;
		else if ( pAppData->codec == 1 ) pAppData->codec = NX_MP4_ENC;
		else if ( pAppData->codec == 2 ) pAppData->codec = NX_H263_ENC;
		else if ( pAppData->codec == 3 ) pAppData->codec = NX_JPEG_ENC;
		hEnc = NX_VidEncOpen( (VID_TYPE_E)pAppData->codec,  NULL );

		pAppData->fpsNum = ( pAppData->fpsNum ) ? ( pAppData->fpsNum ) : ( 30 );
		pAppData->fpsDen = ( pAppData->fpsDen ) ? ( pAppData->fpsDen ) : ( 1 );
		pAppData->gop = ( pAppData->gop ) ? ( pAppData->gop ) : ( pAppData->fpsNum / pAppData->fpsDen );

		//	Initialize Encoder
		memset( &encInitParam, 0, sizeof(encInitParam) );
		encInitParam.width = cropW;
		encInitParam.height = cropH;
		encInitParam.fpsNum = pAppData->fpsNum;
		encInitParam.fpsDen = pAppData->fpsDen;
		encInitParam.gopSize = pAppData->gop;
		encInitParam.bitrate = pAppData->kbitrate * 1024;
		encInitParam.chromaInterleave = ENABLE_NV12;
		encInitParam.enableAUDelimiter = 0;	//	Enable / Disable AU Delimiter
		encInitParam.searchRange = 0;
		if ( pAppData->codec == NX_JPEG_ENC )
		{
			encInitParam.jpgQuality = ( pAppData->qp == 0 ) ? ( 90 ) : ( pAppData->qp );
		}

		//	Rate Control
		encInitParam.maximumQp = pAppData->maxQp;
		encInitParam.disableSkip = 0;
		encInitParam.initialQp = pAppData->qp;
		encInitParam.enableRC = ( encInitParam.bitrate ) ? ( 1 ) : ( 0 );
		encInitParam.RCAlgorithm = ( pAppData->RCAlgorithm == 0 ) ? ( 1 ) : ( 0 );
		encInitParam.rcVbvSize = ( pAppData->vbv ) ? ( pAppData->vbv ) : ( encInitParam.bitrate * 2 / 8 );

		if (NX_VidEncInit( hEnc, &encInitParam ) != VID_ERR_NONE )
		{
			printf("NX_VidEncInit() failed \n");
			exit(-1);
		}
		printf("NX_VidEncInit() Success \n");

		// Get Sequence Data or jpeg Header
		if( fdOut )
		{
			//	Write Sequence Data
			if ( pAppData->codec != NX_JPEG_ENC )
			{
				NX_VidEncGetSeqInfo( hEnc, seqBuffer, &seqSize );
				fwrite( seqBuffer, 1, seqSize, fdOut );
			}
			else
			{
				NX_VidEncJpegGetHeader( hEnc, seqBuffer, &seqSize );
			}

			//dumpdata( seqBuffer, seqSize, "sps pps" );
			StrmTotalSize += seqSize;
			printf("Encoder Header Size = %d\n", seqSize);
		}

		if ( fdLog )
		{
			fprintf(fdLog, "Frame Count\tFrame Size\tEncoding Time\tIs Ley\n");
		}
	}

	//==============================================================================
	// ENCODE PROCESS UNIT
	//==============================================================================
	{
#if ENABLE_NV12
		NX_VID_MEMORY_INFO *pNV12Mem = NULL;
#endif
		NX_VID_MEMORY_HANDLE hMem[MAX_ENC_BUFFER];
		NX_VID_MEMORY_INFO *pPrevDsp = NULL;			//	Previous Displayed Memory

		NX_VID_ENC_IN encIn;
		NX_VID_ENC_OUT encOut;

		long long vipTimeStamp;
		long long totalSize = 0;
		double bitRate = 0.;
		int32_t frameCnt = 0, i, readSize;
		uint64_t startTime, endTime, totalTime = 0;

		NX_QUEUE memQueue;

		//	Current Vip Buffer
		NX_VID_MEMORY_INFO *pCurCapturedBuf = NULL;
		NX_VID_MEMORY_INFO *pTmpMem = NULL;

		//	VIP
		VIP_HANDLE hVip;
		VIP_INFO vipInfo;

#ifndef ANDROID
#if (ENABLE_DISPLAY)
		DISPLAY_INFO dspInfo;
		memset( &dspInfo, 0, sizeof(dspInfo) );
		// Initailize Display
		dspInfo.port = 0;
		dspInfo.module = 0;
		dspInfo.width = cropW;
		dspInfo.height = cropH;
		dspInfo.numPlane = 1;
		// Source Crop
		dspInfo.dspSrcRect.left = 0;
		dspInfo.dspSrcRect.top = 0;
		dspInfo.dspSrcRect.right = cropW;
		dspInfo.dspSrcRect.bottom = cropH;
		// Display Scaling
		dspInfo.dspDstRect.left = 0;
		dspInfo.dspDstRect.top = 0;
		dspInfo.dspDstRect.right = cropW;
		dspInfo.dspDstRect.bottom = cropH;
		hDsp = NX_DspInit( &dspInfo );
		NX_DspVideoSetPriority(dspInfo.module, 0);
#endif
#endif

		memset( &vipInfo, 0, sizeof(vipInfo) );
		vipInfo.port = VIP_PORT;
		vipInfo.mode = VIP_MODE_CLIPPER;
		//	Sensor Input Size
		vipInfo.width = pAppData->width;
		vipInfo.height = pAppData->height;
		vipInfo.numPlane = 1;
		//	Clipper Setting
		vipInfo.cropX = cropX;
		vipInfo.cropY = cropY;
		vipInfo.cropWidth = cropW;
		vipInfo.cropHeight = cropH;
		//	Fps
		vipInfo.fpsNum = pAppData->fpsNum;
		vipInfo.fpsDen = 1;
		// Initailize VIP
		//hVip = NX_VipInit(&vipInfo);

		// Initialze Memory Queue
		NX_InitQueue( &memQueue, MAX_ENC_BUFFER );

		//	Allocate Memory
		for( i=0; i<MAX_ENC_BUFFER ; i++ )
		{
			hMem[i] = NX_VideoAllocateMemory( 4096, cropW, cropH, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
			NX_PushQueue( &memQueue, hMem[i] );
		}

#if ENABLE_NV12
		pNV12Mem = NX_VideoAllocateMemory( 4096, cropW, cropH, NX_MEM_MAP_LINEAR, FOURCC_NV12 );
#endif

#ifndef ANDROID
		//	PopQueue
		NX_PopQueue( &memQueue, (void**)&pTmpMem );
		NX_VipQueueBuffer( hVip, pTmpMem );
#endif

		while(1)
		{
			NX_PopQueue( &memQueue, (void**)&pTmpMem );
			//NX_VipQueueBuffer( hVip, pTmpMem );

			//NX_VipDequeueBuffer( hVip, &pCurCapturedBuf, &vipTimeStamp );
#if (ENABLE_DISPLAY)
			NX_DspQueueBuffer( hDsp, pCurCapturedBuf );
#endif
			if( pPrevDsp )
			{
#if (ENABLE_DISPLAY)
				NX_DspDequeueBuffer( hDsp );
#endif

#if ENABLE_NV12
				if( pNV12Mem )
				{
					int j;
					unsigned char *cbcr =(unsigned char*)pNV12Mem->cbVirAddr;
					unsigned char *cb   =(unsigned char*)pPrevDsp->cbVirAddr;
					unsigned char *cr   =(unsigned char*)pPrevDsp->crVirAddr;

					//	Copy
					memcpy( (unsigned char*)pNV12Mem->luVirAddr, (unsigned char*)pPrevDsp->luVirAddr, cropW*cropH );
					for( i=0 ; i<cropH/2 ; i++ )
					{
						for( j=0 ; j<cropW/2 ; j++ )
						{
							*cbcr++ = *cb++;
							*cbcr++ = *cr++;
						}
					}

					encIn.pImage = pNV12Mem;
				}
				else
#endif
				{
					encIn.pImage = pPrevDsp;
				}

				if( pAppData->codec != NX_JPEG_ENC )
				{
					encIn.forcedIFrame = 0;
					encIn.forcedSkipFrame = 0;
					encIn.quantParam = pAppData->qp;
					encIn.timeStamp = 0;

					// Encode Image
					startTime = NX_GetTickCount();
					NX_VidEncEncodeFrame( hEnc, &encIn, &encOut );
				}
				else
				{
					startTime = NX_GetTickCount();
					NX_VidEncJpegRunFrame( hEnc, encIn.pImage, &encOut );
				}

				endTime = NX_GetTickCount();
				totalTime += (endTime - startTime);

				NX_PushQueue( &memQueue, pPrevDsp );

				if( fdOut && encOut.bufSize>0 )
				{
					totalSize += encOut.bufSize;
					bitRate = (double)totalSize*8/(double)frameCnt;

					if( pAppData->codec == NX_JPEG_ENC )
						fwrite( seqBuffer, 1, seqSize, fdOut );
					fwrite( encOut.outBuf, 1, encOut.bufSize, fdOut );

					//dumpdata( encOut.outBuf, 16, "" );
					printf("[%4d]Size = %6d, bitRate = %6.3f kbps, Qp = %2d, time = %6lld\n", frameCnt, encOut.bufSize, bitRate*pAppData->fpsNum/pAppData->fpsDen/1000., encIn.quantParam, (endTime-startTime) );
					StrmTotalSize += encOut.bufSize;
				}

				if ( fdLog )
				{
				}

				if ( fdRecon )
				{
				}

			}

			pPrevDsp = pCurCapturedBuf;
			frameCnt ++;
		}

#ifndef ANDROID
#if (ENABLE_DISPLAY)
		NX_DspClose( hDsp );
#endif
#endif
		NX_VipClose( hVip );

		{
			float TotalBps = (float)((StrmTotalSize * 8 * pAppData->fpsNum / pAppData->fpsDen) / (frameCnt * 1024));
			printf("[Summary]Bitrate = %3fKBps(%2.f %%), Frame Count = %d \n", TotalBps, TotalBps * 100.f / pAppData->kbitrate, frameCnt );
		}
	}

	//==============================================================================
	// TERMINATION
	//==============================================================================
	if( fdLog )
	{
		fclose( fdLog );
	}

	if( fdOut )
	{
		fclose( fdOut );
	}

	if( hEnc )
	{
		NX_VidEncClose( hEnc );
	}

	return 0;
}

//
//	Coda960 Performance Test Application
//
//	Application Sequence :
//
//	Step 1. Prepare Parameter
//	Step 2. Load YUV Image & Copy to Encoding Buffer
//	Step 3. Write Encoded Bitstream
//
static int32_t VpuEncPerfMain( CODEC_APP_DATA *pAppData )
{
	uint8_t *seqBuffer = (uint8_t *)malloc( MAX_SEQ_BUF_SIZE );  // SPS/PPS or JPEG Header
	int32_t seqSize = 0;

#if (ENABLE_DISPLAY)
	DISPLAY_HANDLE hDsp;                       // Display Handle
#endif
	NX_VID_ENC_HANDLE hEnc;                    // Encoder Handle
	uint64_t StrmTotalSize = 0;
	float    PSNRSum = 0;

	//	Input Image
	int32_t inWidth = pAppData->width;
	int32_t inHeight = pAppData->height;

	//
	//	In/Out/Log File Open
	//
	FILE *fdIn = fopen( pAppData->inFileName, "rb" );
	FILE *fdOut = fopen( pAppData->outFileName, "wb" );
	FILE *fdLog = fopen( pAppData->outLogFileName, "w" );
	FILE *fdRecon = ( pAppData->outImgName ) ? fopen( pAppData->outImgName, "wb" ) : NULL;

	if ( fdIn == NULL || fdOut == NULL )
	{
		printf("input file or output file open error!!\n");
		exit(-1);
	}

	//==============================================================================
	// INITIALIZATION
	//==============================================================================
	{
		NX_VID_ENC_INIT_PARAM encInitParam;									// Encoder Parameters
#ifndef ANDROID
#if (ENABLE_DISPLAY)
		DISPLAY_INFO dspInfo;
		memset( &dspInfo, 0, sizeof(dspInfo) );

		// Initailize Display
		dspInfo.port = 0;
		dspInfo.module = 0;
		dspInfo.width = inWidth;
		dspInfo.height = inHeight;
		dspInfo.numPlane = 1;
		//	Source Crop
		dspInfo.dspSrcRect.left = 0;
		dspInfo.dspSrcRect.top = 0;
		dspInfo.dspSrcRect.right = inWidth;
		dspInfo.dspSrcRect.bottom = inHeight;
		//	Display Scaling
		dspInfo.dspDstRect.left = pAppData->dspX;
		dspInfo.dspDstRect.top = pAppData->dspY;
		dspInfo.dspDstRect.right = pAppData->dspX + pAppData->dspWidth;
		dspInfo.dspDstRect.bottom = pAppData->dspY + pAppData->dspHeight;
		hDsp = NX_DspInit( &dspInfo );
		NX_DspVideoSetPriority(dspInfo.module, 0);
#endif
#endif

		// Initialize Encoder
		if ( pAppData->codec == 0) pAppData->codec = NX_AVC_ENC;
		else if (pAppData->codec == 1) pAppData->codec = NX_MP4_ENC;
		else if (pAppData->codec == 2) pAppData->codec = NX_H263_ENC;
		else if (pAppData->codec == 3) pAppData->codec = NX_JPEG_ENC;
		hEnc = NX_VidEncOpen( (VID_TYPE_E)pAppData->codec, NULL );

		pAppData->fpsNum = ( pAppData->fpsNum ) ? ( pAppData->fpsNum ) : ( 30 );
		pAppData->fpsDen = ( pAppData->fpsDen ) ? ( pAppData->fpsDen ) : ( 1 );
		pAppData->gop = ( pAppData->gop ) ? ( pAppData->gop ) : ( pAppData->fpsNum / pAppData->fpsDen );

		memset( &encInitParam, 0, sizeof(encInitParam) );
		encInitParam.width = inWidth;
		encInitParam.height = inHeight;
		encInitParam.fpsNum = pAppData->fpsNum;
		encInitParam.fpsDen = pAppData->fpsDen;
		encInitParam.gopSize = pAppData->gop;
		encInitParam.bitrate = pAppData->kbitrate * 1024;
		encInitParam.chromaInterleave = ENABLE_NV12;
		encInitParam.enableAUDelimiter = 0;			//	Enable / Disable AU Delimiter
		encInitParam.searchRange = 0;
		if ( pAppData->codec == NX_JPEG_ENC )
		{
			encInitParam.chromaInterleave = 0;
			encInitParam.jpgQuality = (pAppData->qp == 0) ? (90) : (pAppData->qp);
		}

		//	Rate Control
		encInitParam.maximumQp= pAppData->maxQp;
		encInitParam.disableSkip = 0;
		encInitParam.initialQp = pAppData->qp;
		encInitParam.enableRC = ( encInitParam.bitrate ) ? ( 1 ) : ( 0 );
		encInitParam.RCAlgorithm = ( pAppData->RCAlgorithm == 0 ) ? ( 1 ) : ( 0 );
		encInitParam.rcVbvSize = ( pAppData->vbv ) ? (pAppData->vbv) : (encInitParam.bitrate * 2 / 8);

		if (NX_VidEncInit( hEnc, &encInitParam ) != VID_ERR_NONE)
		{
			printf("NX_VidEncInit() failed \n");
			exit(-1);
		}
		printf("NX_VidEncInit() success \n");

		//	Get Sequence Data or Jpeg Header
		if( fdOut )
		{
			//	Write Sequence Data
			if ( pAppData->codec != NX_JPEG_ENC )
			{
				NX_VidEncGetSeqInfo( hEnc, seqBuffer, &seqSize );
				fwrite( seqBuffer, 1, seqSize, fdOut );
			}
			else
			{
				NX_VidEncJpegGetHeader( hEnc, seqBuffer, &seqSize );
			}

			//dumpdata( seqBuffer, size, "sps pps" );
			StrmTotalSize += seqSize;
			printf("Encoder Header Size = %d\n", seqSize);
		}

		if( fdLog )
		{
			fprintf(fdLog, "Frame Count\tFrame Size\tEncoding Time\tIs Key\n");
		}
	}

	//==============================================================================
	// ENCODE PROCESS UNIT
	//==============================================================================
	{
		NX_VID_MEMORY_HANDLE hMem[MAX_ENC_BUFFER];      // Allocate Memory for Encoder Input
		NX_VID_MEMORY_INFO *pPrevDsp = NULL;            // Previous Displayed Memory

		NX_VID_ENC_IN encIn;
		NX_VID_ENC_OUT encOut;

		long long totalSize = 0;
		double bitRate = 0.;
		int32_t frameCnt = 0, i, readSize;
		uint64_t startTime, endTime, totalTime = 0;

		uint8_t *pSrcBuf = (uint8_t*)malloc(inWidth*inHeight*3/2);

		for( i=0; i<MAX_ENC_BUFFER ; i++ )
		{
			if ( pAppData->codec != NX_JPEG_ENC )
			{
#if ENABLE_NV12
				hMem[i] = NX_VideoAllocateMemory( 4096, inWidth, inHeight, NX_MEM_MAP_LINEAR, FOURCC_NV12 );
#else
				hMem[i] = NX_VideoAllocateMemory( 4096, inWidth, inHeight, NX_MEM_MAP_LINEAR, /*FOURCC_NV12*/FOURCC_MVS0 );
#endif
			}
			else
				hMem[i] = NX_VideoAllocateMemory( 4096, inWidth, inHeight, NX_MEM_MAP_LINEAR, /*FOURCC_NV12*/FOURCC_MVS0 );
		}

		while(1)
		{
#ifdef TEST_CHG_PARA
			TestChangeParameter( pAppData, hEnc, frameCnt );
#endif

			//if (frameCnt % 35 == 7)
			//	encIn.forcedIFrame = 1;
			//else if (frameCnt % 35 == 20)
			//	encIn.forcedSkipFrame = 1;

			encIn.pImage = hMem[frameCnt%MAX_ENC_BUFFER];
			if( fdIn )
			{
				readSize = fread(pSrcBuf, 1, inWidth*inHeight*3/2, fdIn);
				if( readSize != inWidth*inHeight*3/2 || readSize == 0 )
				{
					printf("End of Stream!!!\n");
					break;
				}
			}

			LoadImage( pSrcBuf, inWidth, inHeight, encIn.pImage );

			if ( pAppData->codec != NX_JPEG_ENC )
			{
				encIn.forcedIFrame = 0;
				encIn.forcedSkipFrame = 0;
				encIn.quantParam = pAppData->qp;
				encIn.timeStamp = 0;

				//	Encode Image
				startTime = NX_GetTickCount();
				NX_VidEncEncodeFrame( hEnc, &encIn, &encOut );
			}
			else
			{
				startTime = NX_GetTickCount();
				NX_VidEncJpegRunFrame( hEnc, encIn.pImage, &encOut );
			}

			endTime = NX_GetTickCount();
			totalTime += (endTime-startTime);

#ifndef ANDROID
#if (ENABLE_DISPLAY)
			// Display Image
			NX_DspQueueBuffer( hDsp, encIn.pImage );

			if( pPrevDsp )
			{
				NX_DspDequeueBuffer( hDsp );
			}
#endif
			pPrevDsp = encIn.pImage;
#endif

			if( fdOut && encOut.bufSize>0 )
			{
				float PSNR = ( pAppData->codec != NX_JPEG_ENC ) ? (GetPSNR((uint8_t *)encIn.pImage->luVirAddr, (uint8_t *)encOut.ReconImg.luVirAddr, encOut.width, encOut.height, encIn.pImage->luStride)) : ( 0 );

				totalSize += encOut.bufSize;
				bitRate = (double)totalSize*8/(double)frameCnt;

				// Write Sequence Data
				if (  pAppData->codec == NX_JPEG_ENC )
					fwrite( seqBuffer, 1, seqSize, fdOut );
				fwrite( encOut.outBuf, 1, encOut.bufSize, fdOut );

				//dumpdata( encOut.outBuf, 16, "" );
				printf("[%4d]FrameType = %d, size = %8d, ", frameCnt, encOut.frameType, encOut.bufSize);
				printf("bitRate = %6.3f kbps, Qp = %2d, PSNR = %f, time=%6lld\n", bitRate*pAppData->fpsNum/pAppData->fpsDen/1000., encIn.quantParam, PSNR, (endTime-startTime) );
				StrmTotalSize += encOut.bufSize;
				PSNRSum += PSNR;

				//	Frame Size, Encoding Time, Is Key
				if( fdLog )
				{
					fprintf(fdLog, "%5d\t%7d\t%2d\t%lld\t%d\n", frameCnt, encOut.bufSize, encIn.quantParam, (endTime-startTime), encOut.frameType);
					fflush(fdLog);
				}

				if ( fdRecon )
				{
					if ( encOut.width == (int32_t)encOut.ReconImg.luStride )
					{
						fwrite( (void *)encOut.ReconImg.luVirAddr, 1, encOut.width * encOut.height, fdRecon );
						fwrite( (void *)encOut.ReconImg.cbVirAddr, 1, encOut.width * encOut.height / 4, fdRecon );
						fwrite( (void *)encOut.ReconImg.crVirAddr, 1, encOut.width * encOut.height / 4, fdRecon );
					}
					else
					{
						int32_t y;
						uint8_t *pbyTmp = (uint8_t *)encOut.ReconImg.luVirAddr;
						for (y=0 ; y<encOut.height ; y++)
						{
							fwrite( (void *)pbyTmp, 1, encOut.width, fdRecon );
							pbyTmp += encOut.ReconImg.luStride;
						}

						pbyTmp = (uint8_t *)encOut.ReconImg.cbVirAddr;
						for (y=0 ; y<encOut.height/2 ; y++)
						{
							fwrite( (void *)pbyTmp, 1, encOut.width/2, fdRecon );
							pbyTmp += encOut.ReconImg.cbStride;
						}

						pbyTmp = (uint8_t *)encOut.ReconImg.crVirAddr;
						for (y=0 ; y<encOut.height/2 ; y++)
						{
							fwrite( (void *)pbyTmp, 1, encOut.width/2, fdRecon );
							pbyTmp += encOut.ReconImg.crStride;
						}
					}
				}
			}
			// if (frameCnt > 5) break;
			frameCnt ++;
		}

		{
			float TotalBps = (float)((StrmTotalSize * 8 * pAppData->fpsNum / pAppData->fpsDen) / (frameCnt * 1024));
			printf("[Summary]Bitrate = %.3fKBps(%.2f %%), PSNR = %.3fdB, Frame Count = %d \n", TotalBps, TotalBps * 100 / pAppData->kbitrate, (PSNRSum / frameCnt), frameCnt );
		}
	}

	//==============================================================================
	// TERMINATION
	//==============================================================================
	if( fdLog )
	{
		fclose(fdLog);
	}

	if( fdIn )
	{
		fclose( fdIn );
	}

	if( fdOut )
	{
		fclose( fdOut );
	}

	if( hEnc )
	{
		NX_VidEncClose( hEnc );
	}

#ifndef ANDROID
#if (ENABLE_DISPLAY)
	NX_DspClose( hDsp );
#endif
#endif

	return 0;
}

int32_t VpuEncMain( CODEC_APP_DATA *pAppData )
{
	//	Performance Test
	if( pAppData->inFileName )
	{
		if( pAppData->outLogFileName == NULL )
		{
			pAppData->outLogFileName = (char*)malloc(strlen(pAppData->outFileName) + 5);
			strcpy(pAppData->outLogFileName, pAppData->outFileName);
			strcat(pAppData->outLogFileName, ".log");
		}
		return VpuEncPerfMain( pAppData );
	}
	else
	{
		return VpuCamEncMain( pAppData );
	}
	return 0;
}
