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

#include <CNX_JpegCaptureFilter.h>

#define	NX_DTAG	"[CNX_JpegCaptureFilter] "
#include <NX_DbgMsg.h>

//------------------------------------------------------------------------------
CNX_JpegCaptureFilter::CNX_JpegCaptureFilter( void )
	: m_bInit( false )
	, m_bRun( false )
	, m_iCaptureCount( 0 )
	, m_bCaptureWait( false )
{
	m_pJpegCapture 	= new INX_JpegCapture();
	m_pSemWait		= new CNX_Semaphore(1, 0);

	pthread_mutex_init( &m_hLock, NULL );
}

//------------------------------------------------------------------------------
CNX_JpegCaptureFilter::~CNX_JpegCaptureFilter( void )
{
	if( true == m_bInit )
		Deinit();

	pthread_mutex_destroy( &m_hLock );
	
	if( m_pSemWait ) delete m_pSemWait;
	if( m_pJpegCapture ) delete m_pJpegCapture;
}

//------------------------------------------------------------------------------
void	CNX_JpegCaptureFilter::Init( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( false == m_bInit );

	if( false == m_bInit )
	{
		m_bInit = true;
	}
	
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void	CNX_JpegCaptureFilter::Deinit( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NX_ASSERT( true == m_bInit );

	if( true == m_bInit )
	{
		if( m_bRun )	Stop();
		m_bInit = false;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t	CNX_JpegCaptureFilter::Receive( CNX_Sample *pSample )
{
	pSample->Lock();

	JpegEncode( pSample );
	Deliver( pSample );

	pSample->Unlock();
	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_JpegCaptureFilter::ReleaseSample( CNX_Sample *pSample )
{
	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_JpegCaptureFilter::Run( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( false == m_bRun ) {
		m_bRun = true;
	}
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_JpegCaptureFilter::Stop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	if( true == m_bRun ) {
		m_bRun = false;
		m_pSemWait->Post();
	}
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_JpegCaptureFilter::JpegEncode( CNX_Sample *pSample )
{
	CNX_AutoLock lock( &m_hLock );

	if( !m_iCaptureCount ) {
		if( m_bCaptureWait ) m_pSemWait->Post();
		return false;
	}
	
	if( !m_bCaptureWait ) {
		m_pJpegCapture->SetNotifier( m_pNotify );
		m_pJpegCapture->Encode( pSample );
	}
	else {
		m_pJpegCapture->SetNotifier( m_pNotify );
		m_pJpegCapture->Encode( ((CNX_VideoSample*)pSample)->GetVideoMemory() );
	}

	m_iCaptureCount--;

	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_JpegCaptureFilter::Capture( int32_t iCount )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_iCaptureCount ) {
		NxDbgMsg( NX_DBG_WARN, (TEXT("Fail, Capture Running ( remind %d ).\n"), m_iCaptureCount) );
		NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
		return -1;
	}

	m_iCaptureCount = iCount;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

int32_t CNX_JpegCaptureFilter::CaptureWait( int32_t iCount )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	pthread_mutex_lock( &m_hLock );
	m_iCaptureCount = iCount;
	m_bCaptureWait 	= true;
	pthread_mutex_unlock( &m_hLock );

	m_pSemWait->Init();
	m_pSemWait->Pend();

	pthread_mutex_lock( &m_hLock );
	m_bCaptureWait	= false;
	pthread_mutex_unlock( &m_hLock );

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
void CNX_JpegCaptureFilter::RegFileNameCallback( int32_t (*cbFunc)(uint8_t *, uint32_t) )
{
	NX_ASSERT( cbFunc );
	if( cbFunc )
	{
		m_pJpegCapture->RegFileNameCallback( cbFunc );
	}
}
