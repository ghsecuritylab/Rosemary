//------------------------------------------------------------------------------
//
//	Copyright (C) 2014 Nexell Co. All Rights Reserved
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

#include <string.h>			// memset
#include <stdlib.h>
#include <unistd.h>			// getpid(), syscall()
#include <linux/sched.h> 	// SCHED_NORMAL, SCHED_FIFO, SCHED_RR, SCHED_BATCH
#include <asm/unistd.h> 	// __NR_gettid

#include "CNX_VIPCaptureFilter.h"

#define	NX_DTAG	"[CNX_VIPCaptureFilter] "
#include "NX_DbgMsg.h"

#define	NEW_SCHED_POLICY	SCHED_RR
#define	NEW_SCHED_PRIORITY	25

//------------------------------------------------------------------------------
CNX_VIPCaptureFilter::CNX_VIPCaptureFilter()
	: JpegFileNameFunc( NULL )
	, m_bInit( false )
	, m_bRun( false )
	, m_bThreadExit( true )
	, m_hThread( 0 )
	, m_hVip( NULL )
	, m_FourCC( FOURCC_MVS0 )
	, m_iNumOfBuffer( 0 )
	, m_bCaptured( false )
{
	for( int32_t i = 0; i < MAX_BUFFER; i++) {
		m_DeciMemory[i] = NULL;
		m_ClipMemory[i] = NULL;
	}

	m_pRefClock			= CNX_RefClock::GetSingletonPtr();
	
	m_pSemCap			= new CNX_Semaphore(MAX_BUFFER, 0);
	m_pSemOut			= new CNX_Semaphore(MAX_BUFFER, 0);
	
	m_pJpegCapture		= new INX_JpegCapture();

	memset( m_JpegFileName, 0x00, sizeof(m_JpegFileName) );

	NX_ASSERT( m_pSemOut );
	pthread_mutex_init( &m_hCaptureLock, NULL );
}

//------------------------------------------------------------------------------
CNX_VIPCaptureFilter::~CNX_VIPCaptureFilter()
{
	if( true == m_bInit )
		Deinit();

	pthread_mutex_destroy( &m_hCaptureLock );

	delete m_pSemOut;
	delete m_pSemCap;
	delete m_pJpegCapture;
}

//------------------------------------------------------------------------------
void CNX_VIPCaptureFilter::Init( NX_VIPCAPTURE_CONFIG *pConfig )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( false == m_bInit );
	NX_ASSERT( NULL != pConfig );
	
	if( false == m_bInit ) {
		m_VipInfo.port			= pConfig->port;
		m_VipInfo.mode			= VIP_MODE_CLIP_DEC2;
		m_VipInfo.width			= pConfig->width;
		m_VipInfo.height		= pConfig->height;
		m_VipInfo.numPlane		= 1;
		m_VipInfo.fpsNum		= pConfig->fps;
		m_VipInfo.fpsDen		= 1;
		m_VipInfo.cropX			= 0;
		m_VipInfo.cropY			= 0;
		m_VipInfo.cropWidth		= pConfig->width;
		m_VipInfo.cropHeight	= pConfig->height;
		m_VipInfo.outWidth		= pConfig->width;
		m_VipInfo.outHeight		= pConfig->height;

		AllocateBuffer( m_VipInfo.outWidth, m_VipInfo.outHeight, 256, 256, NUM_ALLOC_BUFFER, m_FourCC );
		m_bInit = true;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void CNX_VIPCaptureFilter::Deinit( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( true == m_bInit );

	if( true == m_bInit ) {
		if( m_bRun )	Stop();

		FreeBuffer();
		m_bInit = false;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPCaptureFilter::Receive( CNX_Sample *pSample )
{
	NX_ASSERT(false);
	return false;
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPCaptureFilter::ReleaseSample( CNX_Sample *pSample )
{
	for( int32_t i = 0; i < m_iNumOfBuffer; i++ )
	{
		if( m_DeciMemory[i] == ((CNX_VideoSample*)pSample)->GetVideoMemory() )
		{
			m_ReleaseDeciQueue.Push( (void*)((CNX_VideoSample*)pSample)->GetVideoMemory() );
			m_SampleOutQueue.PushSample( pSample );
			m_pSemOut->Post();
			break;
		}
		
		if( m_ClipMemory[i] == ((CNX_VideoSample*)pSample)->GetVideoMemory() )
		{
			m_ReleaseClipQueue.Push( (void*)((CNX_VideoSample*)pSample)->GetVideoMemory() );
			m_SampleCapQueue.PushSample( pSample );
			m_pSemCap->Post();
			break;
		}
	}

	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_VIPCaptureFilter::Run( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( m_bRun == false ) {
		m_bThreadExit 	= false;
		NX_ASSERT( !m_hThread );
		
		m_hVip = NX_VipInit( &m_VipInfo );

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
int32_t	CNX_VIPCaptureFilter::Stop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( true == m_bRun ) {
		m_bThreadExit = true;
		m_pSemOut->Post();
		m_pSemCap->Post();
		pthread_join( m_hThread, NULL );
		m_hThread = 0x00;
		m_bRun = false;
	}

	NX_VipStreamControl( m_hVip, false );
	NX_VipClose( m_hVip );

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
void CNX_VIPCaptureFilter::AllocateBuffer( int32_t width, int32_t height, int32_t alignx, int32_t aligny, int32_t numOfBuffer, uint32_t dwFourCC )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( numOfBuffer <= MAX_BUFFER );

	m_SampleCapQueue.Reset();
	m_SampleOutQueue.Reset();

	m_SampleCapQueue.SetQueueDepth( numOfBuffer );
	m_SampleOutQueue.SetQueueDepth( numOfBuffer );

	for( int32_t i = 0; i < numOfBuffer; i++)
	{
		NX_ASSERT(NULL == m_ClipMemory[i]);
		NX_ASSERT(NULL == m_DeciMemory[i]);
		
		m_ClipMemory[i] = NX_VideoAllocateMemory( 4096, m_VipInfo.cropWidth, m_VipInfo.cropHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
		m_DeciMemory[i] = NX_VideoAllocateMemory( 4096, m_VipInfo.outWidth, m_VipInfo.outHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
		
		NX_ASSERT( NULL != m_ClipMemory[i] );
		NX_ASSERT( NULL != m_DeciMemory[i] );
		
		m_ClipSample[i].SetOwner( this );
		m_DeciSample[i].SetOwner( this );

		m_SampleCapQueue.PushSample( &m_ClipSample[i] );
		m_SampleOutQueue.PushSample( &m_DeciSample[i] );
	}
	m_iNumOfBuffer = numOfBuffer;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()-- (m_iNumOfBuffer=%d)\n"), __FUNCTION__, m_iNumOfBuffer) );
}

//------------------------------------------------------------------------------
void CNX_VIPCaptureFilter::FreeBuffer( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	
	m_SampleCapQueue.Reset();
	m_SampleOutQueue.Reset();

	for( int32_t i = 0; i < m_iNumOfBuffer; i++ )
	{
		NX_ASSERT(NULL != m_ClipMemory[i]);
		NX_ASSERT(NULL != m_DeciMemory[i]);

		if( m_ClipMemory[i] ) NX_FreeVideoMemory( m_ClipMemory[i] );
		if( m_DeciMemory[i] ) NX_FreeVideoMemory( m_DeciMemory[i] );
	}
	m_pSemCap->Post();
	m_pSemOut->Post();
	m_iNumOfBuffer = 0;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPCaptureFilter::GetSample( CNX_Sample **ppSample )
{
	NX_ASSERT(false);
	return false;
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPCaptureFilter::GetDeliverySample( CNX_Sample **ppSample )
{
	m_pSemOut->Pend();
	if( true == m_SampleOutQueue.IsReady() ) {
		m_SampleOutQueue.PopSample( ppSample );
		(*ppSample)->Lock();
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPCaptureFilter::GetCaptureSample( CNX_Sample **ppSample )
{
	m_pSemCap->Pend();
	if( true == m_SampleCapQueue.IsReady() ) {
		m_SampleCapQueue.PopSample( ppSample );
		(*ppSample)->Lock();
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
void CNX_VIPCaptureFilter::ThreadLoop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	CNX_VideoSample		*pDeciSample = NULL;
	CNX_VideoSample		*pClipSample = NULL;
	
	NX_VID_MEMORY_INFO	*pDeciMemory = NULL;
	NX_VID_MEMORY_INFO	*pClipMemory = NULL;

	uint64_t			sampleTime = 0;
	int64_t				decimatorTime = 0;
	int64_t				clipperTime = 0;

#if(0)
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
	
	m_ReleaseDeciQueue.Reset();
	m_ReleaseClipQueue.Reset();

	for( int32_t i = 0; i < NUM_ALLOC_BUFFER; i++ )
	{
		if( 0 > NX_VipQueueBuffer2( m_hVip, m_ClipMemory[i], m_DeciMemory[i] ) ) {
			NxDbgMsg( NX_DBG_ERR, (TEXT("VipQueueBuffer() Failed.\n")) );
		}
	}

	for( int32_t i = 0; i < NUM_ALLOC_BUFFER - 1; i++ ) {
		m_pSemOut->Post();
		m_pSemCap->Post();
	}

	while( !m_bThreadExit )
	{
		if( false == GetDeliverySample( (CNX_Sample **)&pDeciSample) )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("GetDeliverySample() Failed\n")) );
			continue;
		}
		if( NULL == pDeciSample )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("Sample is NULL\n")) );
			continue;
		}

		if( false == GetCaptureSample( (CNX_Sample **)&pClipSample) )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("GetDeliverySample() Failed\n")) );
			pClipSample->Unlock();
			continue;
		}
		if( NULL == pClipSample )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("Sample is NULL\n")) );
			pClipSample->Unlock();
			continue;
		}

		while( m_ReleaseDeciQueue.IsReady() && m_ReleaseClipQueue.IsReady() )
		{
			m_ReleaseClipQueue.Pop( (void**)&pClipMemory );
			m_ReleaseDeciQueue.Pop( (void**)&pDeciMemory );

			if( 0 > NX_VipQueueBuffer2( m_hVip, pClipMemory, pDeciMemory ) ) {
				NxDbgMsg( NX_DBG_ERR, (TEXT("VipQueueBuffer() Failed.\n")) );
			}
		}

		if( 0 > NX_VipDequeueBuffer2( m_hVip, &pClipMemory, &pDeciMemory, &clipperTime, &decimatorTime ) ) {
			NxDbgMsg( NX_DBG_WARN, (TEXT("VipDequeueBuffer() Failed.\n")) );
			pDeciSample->Unlock();
			continue;
		}

		if( m_pRefClock ) 	sampleTime = m_pRefClock->GetCorrectTickCount( decimatorTime );
		else 				sampleTime = decimatorTime / 1000000;

		pDeciSample->SetTimeStamp( sampleTime );
		pDeciSample->SetVideoMemory( pDeciMemory );
		pClipSample->SetVideoMemory( pClipMemory );

		Deliver( pDeciSample );

		pthread_mutex_lock( &m_hCaptureLock );
		if( m_bCaptured ) {
			JpegEncode( pClipSample );
			m_bCaptured = false;
		}
		pthread_mutex_unlock( &m_hCaptureLock );
		
		if( pDeciSample ) pDeciSample->Unlock();
		if( pClipSample ) pClipSample->Unlock();
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void* CNX_VIPCaptureFilter::ThreadMain( void *arg )
{
	CNX_VIPCaptureFilter *pClass = (CNX_VIPCaptureFilter *)arg;
	NX_ASSERT(NULL != pClass);

	pClass->ThreadLoop();

	return (void*)0xDEADDEAD;
}

//------------------------------------------------------------------------------
void CNX_VIPCaptureFilter::SetJpegFileName( uint8_t *pFileName )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	if( pFileName )
		sprintf( (char*)m_JpegFileName, "%s", pFileName );
	
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void CNX_VIPCaptureFilter::JpegEncode( CNX_Sample *pSample )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	if( !m_JpegFileName[0] ) {
		if( JpegFileNameFunc ) {
			uint32_t bufSize = 0;
			JpegFileNameFunc( (uint8_t*)m_JpegFileName, bufSize );
		}
		else {
			time_t eTime;
			struct tm *eTm;
			time( &eTime);

			eTm = localtime( &eTime );

			sprintf( (char*)m_JpegFileName, "./capture_%04d%02d%02d_%02d%02d%02d.jpeg",
					eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec );
		}
	}

	m_pJpegCapture->SetNotifier( m_pNotify );
	m_pJpegCapture->SetFileName( (char*)m_JpegFileName );
	m_pJpegCapture->Encode( pSample );
	
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t CNX_VIPCaptureFilter::EnableCapture( void )
{
	pthread_mutex_lock( &m_hCaptureLock );
	m_bCaptured = true;
	pthread_mutex_unlock( &m_hCaptureLock );
	return true;	
}

//------------------------------------------------------------------------------
int32_t CNX_VIPCaptureFilter::RegJpegFileNameCallback( int32_t(*cbFunc)( uint8_t *, uint32_t ) )
{
	if( cbFunc )
		JpegFileNameFunc = cbFunc;

	return 0;
}
