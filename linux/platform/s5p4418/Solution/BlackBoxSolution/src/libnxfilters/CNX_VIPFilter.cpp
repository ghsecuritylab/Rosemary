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

#include <string.h>			// memset
#include <stdlib.h>
#include <unistd.h>			// getpid(), syscall()
#include <linux/sched.h> 	// SCHED_NORMAL, SCHED_FIFO, SCHED_RR, SCHED_BATCH
#include <asm/unistd.h> 	// __NR_gettid

#include "CNX_VIPFilter.h"

#define	NX_DTAG	"[CNX_VIPFilter] "
#include "NX_DbgMsg.h"

#define	NEW_SCHED_POLICY	SCHED_RR
#define	NEW_SCHED_PRIORITY	25

#define DISPLAY_FPS			0
#define DUMP_YUV			0
#define ENABLE_DEBUG		0

#if( DUMP_YUV )
#define		YUV_DUMP_FILENAME	"/mnt/mmc/dump_video00.yuv"
static FILE *outFp = NULL;
static int32_t bFileOpen = false;
#endif

#if( ENABLE_DEBUG )
#define NxDbgColorMsg(A, B) do {										\
								if( gNxFilterDebugLevel>=A ) {			\
									printf("\033[1;37;41m%s", NX_DTAG);	\
									DEBUG_PRINT B;						\
									printf("\033[0m\r\n");				\
								}										\
							} while(0)
#else
#define NxDbgColorMsg(A, B) do {} while(0)
#endif

//------------------------------------------------------------------------------
CNX_VIPFilter::CNX_VIPFilter()
	: JpegFileNameFunc( NULL )
	, m_bInit( false )
	, m_bRun( false )
	, m_bPause( false )
	, m_bThreadExit( true )
	, m_hThread( 0 )
	, m_hVip( NULL )
	, m_FourCC( FOURCC_MVS0 )
	, m_iNumOfBuffer( 0 )
	, m_bCapture( false )
	, m_bCaptureResize( false )
{
	m_pRefClock			= CNX_RefClock::GetSingletonPtr();
	m_pSemOut			= new CNX_Semaphore(MAX_BUFFER, 0);
	m_pSemPause			= new CNX_Semaphore(1, 0);
	m_pSemCapture		= new CNX_Semaphore(1, 0);
	m_pSemCaptureRun	= new CNX_Semaphore(1, 0);
	m_pJpegCapture		= new INX_JpegCapture();
	m_pOutStatistics	= new CNX_Statistics();
	
	NX_ASSERT( m_pSemOut );

	for( int32_t i = 0; i < MAX_BUFFER; i++)
		m_VideoMemory[i] = NULL;

	memset( m_JpegFileName, 0x00, sizeof(m_JpegFileName) );
	pthread_mutex_init( &m_hLock, NULL );
}

//------------------------------------------------------------------------------
CNX_VIPFilter::~CNX_VIPFilter()
{
	if( true == m_bInit )
		Deinit();

	pthread_mutex_destroy( &m_hLock );

	delete m_pSemOut;
	delete m_pSemPause;
	delete m_pSemCapture;
	delete m_pSemCaptureRun;
	delete m_pJpegCapture;
	delete m_pOutStatistics;
}

//------------------------------------------------------------------------------
void CNX_VIPFilter::Init( NX_VIP_CONFIG *pConfig )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( false == m_bInit );
	NX_ASSERT( NULL != pConfig );
	
	if( false == m_bInit ) {
		m_VipInfo.port			= pConfig->port;
		m_VipInfo.mode			= VIP_MODE_CLIPPER;
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

		NxDbgMsg( NX_DBG_INFO, (TEXT("[Video|Capture] Port = %d, Width = %d, Height = %d, Fps = %d\n"), 
			pConfig->port, pConfig->width, pConfig->height, pConfig->fps) );

		AllocateBuffer( m_VipInfo.width, m_VipInfo.height, 256, 256, NUM_ALLOC_BUFFER, m_FourCC );
		m_bInit = true;
	}

#if( DUMP_YUV )
		bFileOpen = true;
		if( bFileOpen )
			outFp = fopen(YUV_DUMP_FILENAME, "wb+");
#endif

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void CNX_VIPFilter::Deinit( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( true == m_bInit );

	if( true == m_bInit ) {
		if( m_bRun )	Stop();

		FreeBuffer();
		m_bInit = false;
	}

#if( DUMP_YUV )
	if( outFp ) {
		fclose(outFp);
		outFp = NULL;
	}		
#endif

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPFilter::Receive( CNX_Sample *pSample )
{
	NX_ASSERT(false);
	return false;
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPFilter::ReleaseSample( CNX_Sample *pSample )
{
	m_ReleaseQueue.Push( (void*)((CNX_VideoSample*)pSample)->GetVideoMemory() );
	m_SampleOutQueue.PushSample( pSample );
	m_pSemOut->Post();

	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_VIPFilter::Run( void )
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
int32_t	CNX_VIPFilter::Stop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( true == m_bRun ) {
		m_bThreadExit = true;
		m_pSemOut->Post();
		m_pSemPause->Post();
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
void CNX_VIPFilter::AllocateBuffer( int32_t width, int32_t height, int32_t alignx, int32_t aligny, int32_t numOfBuffer, uint32_t dwFourCC )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( numOfBuffer <= MAX_BUFFER );

	m_SampleOutQueue.Reset();
	m_SampleOutQueue.SetQueueDepth( numOfBuffer );

	for( int32_t i = 0; i < numOfBuffer; i++)
	{
		NX_ASSERT(NULL == m_VideoMemory[i]);
		m_VideoMemory[i] = NX_VideoAllocateMemory( 4096, width, height, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
		NX_ASSERT( NULL != m_VideoMemory[i] );
		
		m_VideoSample[i].SetOwner( this );
		m_SampleOutQueue.PushSample( &m_VideoSample[i] );
	}
	m_iNumOfBuffer = numOfBuffer;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()-- (m_iNumOfBuffer=%d)\n"), __FUNCTION__, m_iNumOfBuffer) );
}

//------------------------------------------------------------------------------
void CNX_VIPFilter::FreeBuffer( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	m_SampleOutQueue.Reset();
	for( int32_t i = 0; i < m_iNumOfBuffer; i++ )
	{
		NX_ASSERT(NULL != m_VideoMemory[i]);
		if( m_VideoMemory[i] ) {
			NX_FreeVideoMemory( m_VideoMemory[i] );
			m_VideoMemory[i] = NULL;
		}
	}
	m_pSemOut->Post();		//	Send Dummy
	m_iNumOfBuffer = 0;
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPFilter::GetSample( CNX_Sample **ppSample )
{
	NX_ASSERT(false);
	return false;
}

//------------------------------------------------------------------------------
int32_t	CNX_VIPFilter::GetDeliverySample( CNX_Sample **ppSample )
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
void CNX_VIPFilter::ThreadLoop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	CNX_VideoSample		*pSample = NULL;
	NX_VID_MEMORY_INFO	*pVideoMemory = NULL;
	uint64_t			sampleTime = 0;
	int64_t				systemTime = 0;

#if(ENABLE_DEBUG)	
	uint64_t			prvTime = 0, curTime = 0;
	uint64_t			expectTime = 0, delayMargin = 0;
#endif

#if(DISPLAY_FPS)
	int32_t 			cnt = 0;
#endif
	
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
	
	m_ReleaseQueue.Reset();

	for( int32_t i = 0; i < m_iNumOfBuffer; i++ )
	{
		if( 0 > NX_VipQueueBuffer( m_hVip, m_VideoMemory[i] ) ) {
			NxDbgMsg( NX_DBG_ERR, (TEXT("VipQueueBuffer() Failed.\n")) );
		}
	}

	m_pSemOut->Init();
	m_pSemCapture->Init();
	m_pSemCaptureRun->Init();
	m_pSemPause->Init();

	for( int32_t i = 0; i < m_iNumOfBuffer - 1; i++ )
		m_pSemOut->Post();

	while( !m_bThreadExit )
	{
		pthread_mutex_lock( &m_hLock );
		int32_t bPause = m_bPause;
		int32_t bCaptureResize = m_bCaptureResize;
		pthread_mutex_unlock( &m_hLock );

		if( bPause ) 
			m_pSemPause->Pend();

		if( bCaptureResize ) {
			m_pSemCaptureRun->Post();
			m_pSemCapture->Pend();
		}

		if( false == GetDeliverySample( (CNX_Sample **)&pSample) )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("GetDeliverySample() Failed\n")) );
			continue;
		}
		if( NULL == pSample )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("Sample is NULL\n")) );
			continue;
		}

		while( m_ReleaseQueue.IsReady() )
		{
			m_ReleaseQueue.Pop( (void**)&pVideoMemory );

			if( 0 > NX_VipQueueBuffer( m_hVip,  pVideoMemory ) ) {
				NxDbgMsg( NX_DBG_ERR, (TEXT("VipQueueBuffer() Failed.\n")) );
			}
		}

		// if( 3 > m_SampleOutQueue.GetSampleCount() ) {
		// 	NxDbgMsg( NX_DBG_WARN, (TEXT("Buffer is not enough. ( port = %s, cur = %2d, alloc = %2d )\n"), 
		// 		(m_VipInfo.port == VIP_PORT_0) ? "VIP0" :
		// 		(m_VipInfo.port == VIP_PORT_1) ? "VIP1" :
		// 		(m_VipInfo.port == VIP_PORT_2) ? "VIP2" : "MIPI",
		// 		m_SampleOutQueue.GetSampleCount(),
		// 		NUM_ALLOC_BUFFER) );
		// }

		if( 0 > NX_VipDequeueBuffer( m_hVip, &pVideoMemory, &systemTime ) ) {
			NxDbgMsg( NX_DBG_WARN, (TEXT("VipDequeueBuffer() Failed.\n")) );
			pSample->Unlock();
			continue;
		}

		if( m_pRefClock ) 	sampleTime = m_pRefClock->GetCorrectTickCount( systemTime );
		else 				sampleTime = systemTime / 1000000;

		pSample->SetTimeStamp( sampleTime );
		pSample->SetVideoMemory( pVideoMemory );

		int32_t nSampleCount = m_SampleOutQueue.GetSampleCount();

		m_pOutStatistics->CalculateFps();
		m_pOutStatistics->CalculateBufNumber( m_iNumOfBuffer - nSampleCount );

#if(ENABLE_DEBUG)	// Internal Debug Message ( Thread Hit / Delayed Capture )
		expectTime	= (uint64_t)(1. / (double)(m_VipInfo.fpsNum / m_VipInfo.fpsDen) * 1000.);
		delayMargin = (uint64_t)(1. / (double)(m_VipInfo.fpsNum / m_VipInfo.fpsDen) * 1000. / 2.);

		// Internal Debug Message ( Delayed Capture )
		if( !prvTime )	prvTime = curTime = sampleTime;
		else 			curTime = sampleTime;

		if( (curTime - prvTime) >= ( expectTime + delayMargin) ) {
			NxDbgMsg( NX_DBG_VBS, (TEXT("[%d] Delayed Capture. (DelayedCapture = %lldms / TargetTime = %lldms / Margine = %lldms, TargetFps = %dfps)\n"),
				nSampleCount, curTime - prvTime, expectTime, delayMargin, m_VipInfo.fpsNum / m_VipInfo.fpsDen) );
		}

		prvTime = curTime;
#endif

#if(DISPLAY_FPS)	// Internal Debug Message : Display frame rate.
		cnt++;
		if( !(cnt % 100) ) {
			cnt = 0;
			NxDbgMsg( NX_DBG_DEBUG, (TEXT("[%s] FrameRate = %.03f fps\n"), 
				(m_VipInfo.port == 2) ? "MIPI" : (m_VipInfo.port == 0) ? "VIP0" : "VIP1", m_pOutStatistics->GetFpsCurrent()));
		}
#endif		
		// Capture
		pthread_mutex_lock( &m_hLock );
		if( m_bCapture ) {
			JpegEncode( pSample );
			m_bCapture = false;
		}
		pthread_mutex_unlock( &m_hLock );

		Deliver( pSample );

#if( DUMP_YUV )
		unsigned char *pAddr = (unsigned char*)pVideoMemory->luVirAddr;
		if( outFp ) {
			fwrite( pAddr, 1, pVideoMemory->imgWidth * pVideoMemory->imgHeight, outFp );
		}
#endif

		if( pSample ) 
			pSample->Unlock();
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void* CNX_VIPFilter::ThreadMain( void *arg )
{
	CNX_VIPFilter *pClass = (CNX_VIPFilter *)arg;
	NX_ASSERT(NULL != pClass);

	pClass->ThreadLoop();

	return (void*)0xDEADDEAD;
}

//------------------------------------------------------------------------------
void CNX_VIPFilter::JpegEncode( CNX_Sample *pSample )
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
void CNX_VIPFilter::SetJpegFileName( uint8_t *pFileName )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( pFileName )
		sprintf( (char*)m_JpegFileName, "%s", pFileName );
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t CNX_VIPFilter::RegJpegFileNameCallback( int32_t(*cbFunc)( uint8_t *, uint32_t ) )
{
	if( cbFunc )
		JpegFileNameFunc = cbFunc;

	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_VIPFilter::Capture( void )
{
	pthread_mutex_lock( &m_hLock );
	m_bCapture = true;
	pthread_mutex_unlock( &m_hLock );
	return true;	
}

//------------------------------------------------------------------------------
#define TARGET_CAPTURE_NUMBER		2

int32_t CNX_VIPFilter::CaptureResize( NX_VIP_CONFIG *pConfig  )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	pthread_mutex_lock( &m_hLock );
	m_bCaptureResize = true;
	pthread_mutex_unlock( &m_hLock );
	m_pSemCaptureRun->Pend();

	if( !m_hVip ) {
		NxDbgMsg( NX_DBG_ERR, (TEXT("Vip is not initialize.\n")) );
		NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
		return -1;
	}

	// 1. Change Vip Config
	VIP_INFO vipInfo;
	memset( &vipInfo, 0x00, sizeof(vipInfo) );
	
	vipInfo.port			= m_VipInfo.port;
	vipInfo.mode			= m_VipInfo.mode;
	vipInfo.numPlane		= m_VipInfo.numPlane;

	vipInfo.fpsNum			= m_VipInfo.fpsNum;
	vipInfo.fpsDen			= m_VipInfo.fpsDen;

	vipInfo.width			= pConfig->width;
	vipInfo.height			= pConfig->height;

	vipInfo.cropX			= 0;
	vipInfo.cropY			= 0;
	vipInfo.cropWidth		= pConfig->width;
	vipInfo.cropHeight		= pConfig->height;

	vipInfo.outWidth		= pConfig->width;
	vipInfo.outHeight		= pConfig->height;

	NX_VipChangeConfig( m_hVip, &vipInfo );

	// 2. Allocate VideoMemory & Queue VideoMemory
	NX_VID_MEMORY_HANDLE videoMemory[2];
	for( int32_t i = 0; i < 2; i++ ) {
		videoMemory[i] = NX_VideoAllocateMemory( 4096, vipInfo.width, vipInfo.height, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );	
		if( 0 > NX_VipQueueBuffer( m_hVip, videoMemory[i] ) ) {
			NxDbgMsg( NX_DBG_ERR, (TEXT("VipQueueBuffer() Failed.\n")) );
		}
	}

	// 3. Dequeue VideoMemory
	NX_VID_MEMORY_INFO	*pVideoMemory = NULL;
	int64_t systemTime = 0;
	
	if( 0 > NX_VipDequeueBuffer( m_hVip, &pVideoMemory, &systemTime ) ) {
		NxDbgMsg( NX_DBG_WARN, (TEXT("VipDequeueBuffer() Failed.\n")) );
	}
	
	for( int32_t i = 0; i < TARGET_CAPTURE_NUMBER - 1; i++ ) {
		if( 0 > NX_VipQueueBuffer( m_hVip, pVideoMemory ) ) {
			NxDbgMsg( NX_DBG_ERR, (TEXT("VipQueueBuffer() Failed.\n")) );
		}
		if( 0 > NX_VipDequeueBuffer( m_hVip, &pVideoMemory, &systemTime ) ) {
			NxDbgMsg( NX_DBG_WARN, (TEXT("VipDequeueBuffer() Failed.\n")) );
		}
	}

	// 4. Jpeg Encoding
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
	m_pJpegCapture->Encode( pVideoMemory );

	// 5. Release resource
	for( int32_t i = 0; i < 2; i++ ) {
		if( videoMemory[i] ) NX_FreeVideoMemory( videoMemory[i] );
	}

	// 6. Restore Configuration & Queue VideoMemory
	NX_VipChangeConfig( m_hVip, &m_VipInfo );
	
	m_SampleOutQueue.Reset();
	m_SampleOutQueue.SetQueueDepth( m_iNumOfBuffer );
	
	m_ReleaseQueue.Reset();

	for( int32_t i = 0; i < m_iNumOfBuffer; i++ )
	{
		m_SampleOutQueue.PushSample( &m_VideoSample[i] );
		if( 0 > NX_VipQueueBuffer( m_hVip, m_VideoMemory[i] ) ) {
			NxDbgMsg( NX_DBG_ERR, (TEXT("VipQueueBuffer() Failed.\n")) );
		}
	}

	m_pSemOut->Init();
	for( int32_t i = 0; i < m_iNumOfBuffer - 1; i++ )
		m_pSemOut->Post();

	pthread_mutex_lock( &m_hLock );
	m_bCaptureResize = false;
	pthread_mutex_unlock( &m_hLock );

	m_pSemCapture->Post();

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_VIPFilter::Pause( int32_t enable )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	if( m_bPause == enable ) {
		NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
		return -1;
	}

	pthread_mutex_lock( &m_hLock);
	m_bPause = enable;
	pthread_mutex_unlock( &m_hLock );

	if( enable == false ) {
		m_pSemPause->Post();
	}
	else {
		while( m_SampleOutQueue.GetSampleCount() != m_iNumOfBuffer );
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_VIPFilter::ChangeConfig( NX_VIP_CONFIG *pConfig  )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	m_VipInfo.width			= pConfig->width;
	m_VipInfo.height		= pConfig->height;
	m_VipInfo.fpsNum		= pConfig->fps;
	m_VipInfo.fpsDen		= 1;
	m_VipInfo.cropX			= 0;
	m_VipInfo.cropY			= 0;
	m_VipInfo.cropWidth		= pConfig->width;
	m_VipInfo.cropHeight	= pConfig->height;
	m_VipInfo.outWidth		= pConfig->width;
	m_VipInfo.outHeight		= pConfig->height;
	
	NX_VipChangeConfig( m_hVip, &m_VipInfo );

	FreeBuffer();
	AllocateBuffer( m_VipInfo.width, m_VipInfo.height, 256, 256, NUM_ALLOC_BUFFER, m_FourCC );

	for( int32_t i = 0; i < m_iNumOfBuffer; i++ )
	{
		if( 0 > NX_VipQueueBuffer( m_hVip, m_VideoMemory[i] ) ) {
			NxDbgMsg( NX_DBG_ERR, (TEXT("VipQueueBuffer() Failed.\n")) );
		}
	}

	m_ReleaseQueue.Reset();
	m_pSemOut->Init();
	for( int32_t i = 0; i < m_iNumOfBuffer - 1; i++ )
		m_pSemOut->Post();

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_VIPFilter::GetStatistics( NX_FILTER_STATISTICS *pStatistics )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}