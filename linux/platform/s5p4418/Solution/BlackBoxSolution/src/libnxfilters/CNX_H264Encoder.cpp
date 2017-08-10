//------------------------------------------------------------------------------
//
//	Copyright (C) 2013 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		:
//	File		:
//	Description	:
//	Author		: 
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>			// memcpy()
#include <stdlib.h>			// malloc()
#include <linux/sched.h> 	// SCHED_NORMAL, SCHED_FIFO, SCHED_RR, SCHED_BATCH
#include <asm/unistd.h> 	// __NR_gettid
#include <unistd.h> 		// getpid(), syscall()

#include "CNX_H264Encoder.h"
#define	NX_DTAG	"[CNX_H264Encoder] "
#include "NX_DbgMsg.h"

#define	NEW_SCHED_POLICY	SCHED_RR
#define	NEW_SCHED_PRIORITY	25
#define MAX_VID_QCOUNT		64

#define	DUMP_H264			0

#if( DUMP_H264 )
#define		H264_DUMP_FILENAME	"/mnt/mmc/dump_video01.h264"
static FILE *outFp = NULL;
static int32_t bFileOpen = false;
#endif

#if(0)
static void dumpdata( void *data, int len, const char *msg )
{
	int i=0;
	unsigned char *byte = (unsigned char *)data;
	printf("Dump Data : %s", msg);
	for( i=0 ; i<len ; i ++ )
	{
		if( i!=0 && i%32 == 0 ) printf("\n\t");
		printf("%.2x", byte[i] );
		if( i%4 == 3 ) printf(" ");
	}
	printf("\n");
}
#else
static void dumpdata( void *data, int len, const char *msg )
{
}
#endif

//------------------------------------------------------------------------------
CNX_H264Encoder::CNX_H264Encoder()
	: m_bInit( false )
	, m_bRun( false )
	, m_bEnable( true )
	, m_bThreadExit( true )
	, m_hThread( 0x00 )
	, m_PacketID( 0 )
	, m_hEnc( NULL )
	, m_iNumOfBuffer( 0 )
{
	m_pSemIn			= new CNX_Semaphore( MAX_VID_QCOUNT, 0 );
	m_pSemOut			= new CNX_Semaphore( MAX_VID_QCOUNT, 0 );
	m_pInStatistics		= new CNX_Statistics();
	m_pOutStatistics 	= new CNX_Statistics();

	NX_ASSERT( m_pSemIn );
	NX_ASSERT( m_pSemOut );

	pthread_mutex_init( &m_hLock, NULL );
}

//------------------------------------------------------------------------------
CNX_H264Encoder::~CNX_H264Encoder()
{
	if( true == m_bInit )
		Deinit();

	pthread_mutex_destroy( &m_hLock );
	
	delete m_pSemIn;
	delete m_pSemOut;
	delete m_pInStatistics;
	delete m_pOutStatistics;
}

//------------------------------------------------------------------------------
void CNX_H264Encoder::Init( NX_VIDENC_CONFIG *pConfig )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( false == m_bInit );
	NX_ASSERT( NULL != pConfig );

	if( false == m_bInit )
	{
		memset( &m_EncInfo, 0, sizeof(m_EncInfo) );

		m_EncInfo.width				= pConfig->width;
		m_EncInfo.height			= pConfig->height;
		m_EncInfo.gopSize			= pConfig->fps / 2;		// Group of pictures (key frame interval)
		m_EncInfo.fpsNum			= pConfig->fps;
		m_EncInfo.fpsDen			= 1;
		
		// rate control parameter
		m_EncInfo.enableRC			= 1;
		m_EncInfo.RCAlgorithm		= 1;					// 0:Hardware / 1:Nexell
		m_EncInfo.bitrate			= pConfig->bitrate;
		m_EncInfo.initialQp			= 0;
		m_EncInfo.maximumQp			= 51;
		m_EncInfo.disableSkip		= true;
		
		m_EncInfo.rcVbvSize			= 0;
		
		m_EncInfo.numIntraRefreshMbs= 0;
		m_EncInfo.searchRange		= 2;	// 0 : 128 x 64, 1 : 64 x 32, 2 : 32 x 16, 3 : 16 x 16
		m_EncInfo.chromaInterleave	= 0;
		m_EncInfo.enableAUDelimiter	= false;
		// m_EncInfo.RCDelay		=;	// Hardware Rate Control Algorithm
		// m_EncInfo.gammaFactor	=;	// Hardware Rate Control Algorithm
		// m_EncInfo.RcMode			=;	// N/A

		// m_EncInfo.rotAngle		=;
		// m_EncInfo.mirDirection	=;
		// m_EncInfo.jpgQuality		=;

		NxDbgMsg( NX_DBG_INFO, (TEXT("[Video|Encoder] Width=%d, Height=%d, Bitrate=%d, FPS=%d\n"), m_EncInfo.width, m_EncInfo.height, m_EncInfo.bitrate, m_EncInfo.fpsNum / m_EncInfo.fpsDen) );

		AllocateBuffer( NUM_ALLOC_BUFFER );
		m_bInit = true;

#if( DUMP_H264 )
		bFileOpen = true;
		if( bFileOpen )
			outFp = fopen(H264_DUMP_FILENAME, "wb+");
#endif
	}
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void CNX_H264Encoder::Deinit( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( true == m_bInit );

	if( true == m_bInit )
	{
		if( m_bRun )	Stop();
		m_bInit = false;
	}

#if( DUMP_H264 )
	if( outFp ) {
		fclose(outFp);
		outFp = NULL;
	}		
#endif

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t	CNX_H264Encoder::Receive( CNX_Sample *pSample )
{
	CNX_AutoLock lock( &m_hLock );
	NX_ASSERT( NULL != pSample );

	if( !m_bEnable ) 
		return true;

#if( DUMP_H264 )
#else	
	if( !m_pOutFilter )
		return true;
#endif

	pSample->Lock();
	m_SampleInQueue.PushSample( pSample );
	m_pSemIn->Post();

	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_H264Encoder::ReleaseSample( CNX_Sample *pSample )
{
	m_SampleOutQueue.PushSample( pSample );
	m_pSemOut->Post();
	
	unsigned char *pBuf;
	int32_t bufSize;

	((CNX_MuxerSample*)pSample)->GetBuffer(&pBuf, &bufSize);
	if( pBuf ) {
		free( pBuf );
		pBuf = NULL;
	}

	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_H264Encoder::Run( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( m_bRun == false ) {
		m_bThreadExit 	= false;
		NX_ASSERT( !m_hThread );

		if( !m_hEnc ) {
			m_hEnc = NX_VidEncOpen( NX_AVC_ENC, NULL );
			NX_VidEncInit( m_hEnc, &m_EncInfo );
		}

		if( 0 > pthread_create( &this->m_hThread, NULL, this->ThreadMain, this ) ) {
			NxDbgMsg( NX_DBG_ERR, (TEXT("%s(): Fail, Create Thread\n"), __FUNCTION__) );
			return false;
		}

		m_bRun = true;
	}
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_H264Encoder::Stop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( true == m_bRun ) {
		m_bThreadExit = true;
		m_pSemIn->Post();
		m_pSemOut->Post();
		pthread_join( m_hThread, NULL );
		m_hThread = 0x00;
		m_bRun = false;

		if( NULL != m_hEnc ) {
			NX_VidEncClose( m_hEnc );
			m_hEnc = NULL;
		}
	}
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
void CNX_H264Encoder::AllocateBuffer( int32_t numOfBuffer )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( numOfBuffer <= MAX_BUFFER );

	m_SampleOutQueue.Reset();
	m_SampleOutQueue.SetQueueDepth( numOfBuffer );

	m_pSemOut->Init();
	for( int32_t i = 0; i < numOfBuffer; i++) {
		m_OutSample[i].SetOwner(this);
		m_OutSample[i].SetBuffer( (uint8_t*)&m_OutBuf[i], sizeof(NX_VID_ENC_OUT) );

		m_SampleOutQueue.PushSample( &m_OutSample[i] );
		m_pSemOut->Post();
	}
	m_iNumOfBuffer = numOfBuffer;
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()-- (m_iNumOfBuffer=%d)\n"), __FUNCTION__, m_iNumOfBuffer) );
}

//------------------------------------------------------------------------------
void CNX_H264Encoder::FreeBuffer( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	for( int32_t i = 0; i < m_iNumOfBuffer; i++ )
	{
		//NX_ASSERT(NULL != m_OutBuf[i]);
	}
	m_pSemIn->Post();
	m_pSemOut->Post();
	m_iNumOfBuffer = 0;
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
//	GetSample�� input���� ���� ���۸� queue�� �־� �ξ��� ��� queue�κ���
//	sample�� �������� ��ƾ�̴�.
//	Locking�� unlocking�� sample push �� (Receive) �� pop �Ŀ� �Ͽ��� �Ѵ�.
int32_t	CNX_H264Encoder::GetSample( CNX_Sample **ppSample )
{
	m_pSemIn->Pend();
	if( true == m_SampleInQueue.IsReady() ){
		m_SampleInQueue.PopSample( ppSample );
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
int32_t	CNX_H264Encoder::GetDeliverySample( CNX_Sample **ppSample )
{
	m_pSemOut->Pend();
	if( true == m_SampleOutQueue.IsReady() ) {
		m_SampleOutQueue.PopSample( ppSample );
		(*ppSample)->Lock();
		return true;
	}
	NxDbgMsg( NX_DBG_WARN, (TEXT("Sample is not ready\n")) );
	return false;
}

//------------------------------------------------------------------------------
void CNX_H264Encoder::ThreadLoop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	int32_t	ret = 0;
	CNX_VideoSample		*pSample = NULL;
	CNX_MuxerSample		*pOutSample = NULL;

#if (0)
	{
		pid_t pid = getpid();
		pid_t tid = (pid_t)syscall(__NR_gettid);
		struct sched_param param;
		memset( &param, 0, sizeof(param) );
		NxDbgMsg( NX_DBG_DEBUG, (TEXT("Thread info ( pid:%4d, tid:%4d )\n"), pid, tid) );
		param.sched_priority = NEW_SCHED_PRIORITY;
		if( 0 != sched_setscheduler( tid, NEW_SCHED_POLICY, &param ) ){
			NxDbgMsg( NX_DBG_ERR, (TEXT("Failed sched_setscheduler!!!(pid=%d, tid=%d)\n"), pid, tid) );
		}
	}
#endif

	while( !m_bThreadExit )
	{
		if( false == GetSample((CNX_Sample **)&pSample) )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("GetSample() Failed\n")) );
			continue;
		}
		if( NULL == pSample )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("Sample is NULL\n")) );
			continue;
		}
		if( false == GetDeliverySample( (CNX_Sample **)&pOutSample) )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("GetDeliverySample() Failed\n")) );
			pSample->Unlock();
			continue;
		}
		if( NULL == pOutSample )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("Encode buffer is NULL\n")) );
			pSample->Unlock();
			continue;
		}

		m_pOutStatistics->CalculateBpsStart();
		// uint64_t interval = NX_GetTickCount();
		ret = EncodeVideo( pSample, pOutSample );
		// interval = NX_GetTickCount() - interval;
		// NxDbgMsg( NX_DBG_VBS, (TEXT("Encoding Interval : %lld mSec\n"), interval) );

		m_pOutStatistics->CalculateBpsEnd( pOutSample->GetActualDataLength() );

		if( pSample )
			pSample->Unlock();

		// m_pInStatistics->CalculateFps();
		// m_pInStatistics->CalculateBufNumber( m_SampleInQueue.GetSampleCount() );
		// m_pOutStatistics->CalculateFps();
		// m_pOutStatistics->CalculateBufNumber( m_iNumOfBuffer - m_SampleOutQueue.GetSampleCount() );

		if( ret > 0 ) {
			// interval = NX_GetTickCount();
			Deliver( pOutSample );
			// interval = NX_GetTickCount() - interval;
			// NxDbgMsg( NX_DBG_VBS, (TEXT("Encoding Data deliver Interval : %lld mSec\n"), interval) );
		}
		else if( ret < 0 ) {
			if( m_pNotify ) {
				m_pNotify->EventNotify( 0xF002, NULL, 0 );
			}
		}
		else {
			NxDbgMsg( NX_DBG_DEBUG, (TEXT("Have no MPEG encode result.\n")) );
		}

		if( pOutSample )
			pOutSample->Unlock();

	}

	while( m_SampleInQueue.IsReady() )
	{
		m_SampleInQueue.PopSample((CNX_Sample**)&pSample);
		if( pSample )
			pSample->Unlock();
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void* CNX_H264Encoder::ThreadMain(void*arg)
{
	CNX_H264Encoder *pClass = (CNX_H264Encoder *)arg;

	pClass->ThreadLoop();

	return (void*)0xDEADDEAD;
}

//------------------------------------------------------------------------------
//	Ret
//	0 <  Ret : Encode success & have acutual data length
//	0 == Ret : No error but have no output stream length
//	0 >  Ret : Error ( Ret = error code )
int32_t CNX_H264Encoder::EncodeVideo( CNX_VideoSample *pInSample, CNX_MuxerSample *pOutSample )
{
	NX_VID_ENC_IN	encIn;
	NX_VID_ENC_OUT	encOut;
	uint64_t timeStamp = pInSample->GetTimeStamp();

	memset( &encIn, 0x00, sizeof(encIn) );
	encIn.pImage		= pInSample->GetVideoMemory();
	// encIn.timeStamp		= ;
	// encIn.forcedIFrame	= ;
	// encIn.quantParam		= ;

	if( !NX_VidEncEncodeFrame( m_hEnc, &encIn, &encOut) ) // encoding successful.
	{
		if( encOut.bufSize > 0 ) 
		{
			unsigned char *encBuffer = NULL;
			unsigned char *seqBuffer = NULL;
			int32_t encSize = 0, seqSize = 0;

#if 1		
			// move to "MP4 Muxer" / "TS Muxer"
			if(encOut.frameType) {
				seqBuffer = (unsigned char *)malloc( MAX_SEQ_BUF_SIZE );
				NX_VidEncGetSeqInfo( m_hEnc, seqBuffer, &seqSize );
			}
#endif			

			encSize = encOut.bufSize + seqSize;
			encBuffer = (unsigned char*) malloc( encSize );
			
			if( NULL != seqBuffer ) {
				memcpy( encBuffer, seqBuffer, seqSize );
				free(seqBuffer);
				seqBuffer = NULL;
				
				dumpdata( encBuffer, seqSize, "SPS/PPS\n\t");
			}
			memcpy( encBuffer + seqSize, encOut.outBuf, encOut.bufSize );

			pOutSample->SetOwner( this );
			pOutSample->SetBuffer( encBuffer, encSize );
			pOutSample->SetDataType( m_PacketID );
			pOutSample->SetTimeStamp( timeStamp );
			pOutSample->SetActualDataLength( encSize );
			pOutSample->SetSyncPoint( encOut.frameType );
			pOutSample->SetFlags( false );
			
#if( DUMP_H264 )
			if( outFp ) {
				fwrite( encBuffer, 1, encSize, outFp );
			}
#endif

			return encSize;
		}
		return 0;
	}

	return -1;
}

//----------------------------------------------------------------------------
int32_t	CNX_H264Encoder::EnableFilter( uint32_t enable )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	NxDbgMsg( NX_DBG_DEBUG, (TEXT("%s : %s -- > %s\n"), __FUNCTION__, (m_bEnable)?"Enable":"Disable", (enable)?"Enable":"Disable") );

	m_bEnable = enable;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_H264Encoder::SetPacketID( uint32_t PacketID )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	
	NxDbgMsg( NX_DBG_DEBUG, (TEXT("Packet ID = %d\n"), PacketID) );
	m_PacketID = PacketID;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_H264Encoder::GetDsiInfo( uint8_t *dsiInfo, int32_t *dsiSize )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( m_bInit ) {
		if( !m_hEnc ) {
			m_hEnc = NX_VidEncOpen( NX_AVC_ENC, NULL );
			NX_VidEncInit( m_hEnc, &m_EncInfo );
		}

		NX_VidEncGetSeqInfo( m_hEnc, dsiInfo, dsiSize );
		dumpdata( dsiInfo, *dsiSize, "H264 Encoder DSI\n\t" );

		if( m_hEnc ) {
			NX_VidEncClose( m_hEnc );
			m_hEnc = NULL;
		}
	}
	else {
		NxDbgMsg( NX_DBG_ERR, (TEXT("Fail, Get DSI infomation.\n")) );
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t  CNX_H264Encoder::GetStatistics( NX_FILTER_STATISTICS *pStatistics )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );	
	return true;
}

