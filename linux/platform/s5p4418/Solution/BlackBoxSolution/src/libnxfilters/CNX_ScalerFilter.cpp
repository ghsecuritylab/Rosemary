//------------------------------------------------------------------------------
//
//	Copyright (C) 2015 Nexell Co. All Rights Reserved
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

#include <string.h>
#include <linux/sched.h> 	// SCHED_NORMAL, SCHED_FIFO, SCHED_RR, SCHED_BATCH
#include <asm/unistd.h> 	// __NR_gettid
#include <unistd.h> 		// __NR_gettid

#include <nx_fourcc.h>

#include "CNX_ScalerFilter.h"
#define	NX_DTAG	"[CNX_ScalerFilter] "
#include "NX_DbgMsg.h"

#define	NEW_SCHED_POLICY	SCHED_RR
#define	NEW_SCHED_PRIORITY	25

//------------------------------------------------------------------------------
CNX_ScalerFilter::CNX_ScalerFilter()
	: m_bInit( false )
	, m_bRun( false )
	, m_pSemIn( NULL )
	, m_pSemOut( NULL )
	, m_bThreadExit( true )
	, m_hThread( 0x00 )
	, m_iNumOfBuffer( 0 )
	, m_hScaler( 0x00 )
	, m_iZoomLevel( 1 )
{
	for( int32_t i = 0; i < MAX_BUFFER; i++ )
		m_VideoMemory[i] = NULL;

	m_pSemIn	= new CNX_Semaphore( MAX_BUFFER, 0 );
	m_pSemOut	= new CNX_Semaphore( MAX_BUFFER, 0 );
	
	NX_ASSERT( m_pSemIn );
	NX_ASSERT( m_pSemOut );

	pthread_mutex_init( &m_hLock, NULL );
}

//------------------------------------------------------------------------------
CNX_ScalerFilter::~CNX_ScalerFilter()
{
	if( true == m_bInit )
		Deinit();
	
	pthread_mutex_destroy( &m_hLock );

	delete m_pSemIn;
	delete m_pSemOut;
}

//------------------------------------------------------------------------------
void CNX_ScalerFilter::Init( NX_SCALER_CONFIG *pConfig )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( false == m_bInit );

	if( false == m_bInit )
	{
		AllocateBuffer( pConfig->width, pConfig->height, 256, 256, NUM_ALLOC_BUFFER, FOURCC_MVS0);
		m_bInit = true;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//----------------------------------------------------------------------------
void CNX_ScalerFilter::Deinit( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( true == m_bInit );

	if( true == m_bInit )
	{
		if( m_bRun )	Stop();
		FreeBuffer();
		m_bInit = false;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t CNX_ScalerFilter::Receive( CNX_Sample *pSample )
{
	NX_ASSERT( NULL != pSample );

	if( !m_pOutFilter ) {
		NxDbgMsg( NX_DBG_WARN, (TEXT("Filter is not connected!")) );
		return true;
	}

	pSample->Lock();
	m_SampleInQueue.PushSample(pSample);
	m_pSemIn->Post();
	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_ScalerFilter::ReleaseSample(CNX_Sample *pSample)
{
	m_SampleOutQueue.PushSample( pSample );
	m_pSemOut->Post();

	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_ScalerFilter::Run( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( m_bRun == false ) {
		m_bThreadExit 	= false;
		NX_ASSERT( !m_hThread );

		if( 0 > pthread_create( &this->m_hThread, NULL, this->ThreadMain, this ) )
		{
			NxDbgMsg( NX_DBG_ERR, (TEXT("%s(): Fail, Create Thread\n"), __FUNCTION__) );
			return false;
		}

		m_bRun = true;
	}
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_ScalerFilter::Stop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( true == m_bRun ) {
		m_bThreadExit = true;
		m_pSemIn->Post();
		m_pSemOut->Post();
		pthread_join( m_hThread, NULL );
		m_hThread = 0x00;
		m_bRun = false;
	}
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
void CNX_ScalerFilter::AllocateBuffer( int32_t width, int32_t height, int32_t alignx, int32_t aligny, int32_t numOfBuffer, uint32_t dwFourCC )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( numOfBuffer <= MAX_BUFFER );

	m_SampleOutQueue.Reset();
	m_SampleOutQueue.SetQueueDepth( numOfBuffer );

	m_pSemOut->Init();
	for( int32_t i = 0; i < numOfBuffer; i++) {
		NX_ASSERT( NULL == m_VideoMemory[i] );
		m_VideoMemory[i] = NX_VideoAllocateMemory( 4096, width, height, NX_MEM_MAP_LINEAR, dwFourCC );
		NX_ASSERT( NULL != m_VideoMemory[i] );

		m_VideoSample[i].SetOwner( this );
		m_VideoSample[i].SetVideoMemory( m_VideoMemory[i] );

		m_SampleOutQueue.PushSample( &m_VideoSample[i] );
		m_pSemOut->Post();
	}
	m_iNumOfBuffer = numOfBuffer;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()-- (m_iNumOfBuffer=%d)\n"), __FUNCTION__, m_iNumOfBuffer) );
}

//------------------------------------------------------------------------------
void CNX_ScalerFilter::FreeBuffer( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	m_SampleOutQueue.Reset();
	for( int32_t i = 0; i < m_iNumOfBuffer; i++ )
	{
		NX_ASSERT(NULL != m_VideoMemory[i]);
		if( m_VideoMemory[i] )
		{
			NX_FreeVideoMemory( m_VideoMemory[i] );
		}
	}
	m_pSemIn->Post();
	m_pSemOut->Post();
	m_iNumOfBuffer = 0;
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t CNX_ScalerFilter::GetSample( CNX_Sample **ppSample )
{
	m_pSemIn->Pend();
	if( true == m_SampleInQueue.IsReady() ){
		m_SampleInQueue.PopSample( ppSample );
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
int32_t CNX_ScalerFilter::GetDeliverySample( CNX_Sample **ppSample )
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
void CNX_ScalerFilter::ThreadLoop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	CNX_VideoSample	*pSample = NULL;
	CNX_VideoSample	*pOutSample = NULL;

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

	m_hScaler = NX_SCLOpen();

	while( !m_bThreadExit )
	{
		if( false == GetSample( (CNX_Sample **)&pSample) )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("GetSample() Failed.\n")) );
			continue;
		}
		if( NULL == pSample )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("Sample is NULL.\n")) );
			continue;
		}
		if( false == GetDeliverySample( (CNX_Sample **)&pOutSample) )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("GetDeliverySample() Failed.\n")) );
			pSample->Unlock();
			continue;
		}
		if( NULL == pOutSample )
		{
			NxDbgMsg( NX_DBG_WARN, (TEXT("DeliverySample is NULL.\n")) );
			pSample->Unlock();
			continue;
		}

		NX_VID_MEMORY_HANDLE hSrcMemory = pSample->GetVideoMemory();
		NX_VID_MEMORY_HANDLE hOutMemory = pOutSample->GetVideoMemory();

		if( !DigitalZoom( hSrcMemory, hOutMemory ) ) {
			Deliver( pSample );
		}
		else {
			pOutSample->SetTimeStamp( pSample->GetTimeStamp() );
			Deliver( pOutSample );
		}

		if( pSample ) 
			pSample->Unlock();

		if( pOutSample ) 
			pOutSample->Unlock();
	}

	if( m_hScaler ) {
		NX_SCLClose( m_hScaler );
		m_hScaler = 0x00;
	}

	while( m_SampleInQueue.IsReady() )
	{
		m_SampleInQueue.PopSample( (CNX_Sample**)&pSample );
		if( pSample )
			pSample->Unlock();
	}	

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void* CNX_ScalerFilter::ThreadMain( void* arg )
{
	CNX_ScalerFilter *pClass = (CNX_ScalerFilter *)arg;
	NX_ASSERT( NULL != pClass );

	pClass->ThreadLoop();

	return (void*)0xDEADDEAD;
}

//------------------------------------------------------------------------------
#define ALIGN(X,N)      ((X+N-1) & (~(N-1)))

#define ALIGN8(X)		ALIGN(X,8)
#define ALIGN32(X)		ALIGN(X,32)

int32_t CNX_ScalerFilter::DigitalZoom( NX_VID_MEMORY_HANDLE hSrcMemory, NX_VID_MEMORY_HANDLE hOutMemory )
{
	CNX_AutoLock lock( &m_hLock );

	if( m_iZoomLevel == 1. )
		return false;

	int32_t  	imgWidth	= hSrcMemory->imgWidth;
	int32_t 	imgHeight	= hSrcMemory->imgHeight;
	uint32_t	luPhyAddr	= hSrcMemory->luPhyAddr;
	uint32_t	cbPhyAddr	= hSrcMemory->cbPhyAddr;
	uint32_t	crPhyAddr	= hSrcMemory->crPhyAddr;

	hSrcMemory->imgWidth	= ALIGN8((int32_t)((float)imgWidth / m_iZoomLevel));
	hSrcMemory->imgHeight	= (int32_t)((float)imgHeight / m_iZoomLevel);

	hSrcMemory->luPhyAddr	+= ALIGN32((imgWidth - hSrcMemory->imgWidth) / 2);
	hSrcMemory->cbPhyAddr	+= ALIGN32((imgWidth - hSrcMemory->imgWidth ) / 4);
	hSrcMemory->crPhyAddr	+= ALIGN32((imgWidth - hSrcMemory->imgWidth ) / 4);

	hSrcMemory->luPhyAddr	+= (imgHeight - hSrcMemory->imgHeight ) / 2 * hSrcMemory->luStride;
	hSrcMemory->cbPhyAddr	+= (imgHeight - hSrcMemory->imgHeight ) / 4 * hSrcMemory->cbStride;
	hSrcMemory->crPhyAddr	+= (imgHeight - hSrcMemory->imgHeight ) / 4 * hSrcMemory->crStride;
	
	int32_t ret = NX_SCLScaleImage( m_hScaler, hSrcMemory, hOutMemory );
	hSrcMemory->imgWidth	= imgWidth;
	hSrcMemory->imgHeight	= imgHeight;
	hSrcMemory->luPhyAddr	= luPhyAddr;
	hSrcMemory->cbPhyAddr	= cbPhyAddr;
	hSrcMemory->crPhyAddr	= crPhyAddr;

	if( 0 > ret ) {
		NxDbgMsg( NX_DBG_ERR, (TEXT("Fail, Image Scaling!\n")) );
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_ScalerFilter::SetDigitalZoomLevel( float iLevel )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	NxDbgMsg( NX_DBG_DEBUG, (TEXT("%s : %.1f --> %.1f\n"), __FUNCTION__, m_iZoomLevel, iLevel ) );
	m_iZoomLevel = iLevel;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}