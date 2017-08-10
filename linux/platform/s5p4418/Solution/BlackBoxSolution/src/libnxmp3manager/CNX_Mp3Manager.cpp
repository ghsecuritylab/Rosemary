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

#include <time.h>
#include "CNX_Mp3Manager.h"

#define NX_DTAG		"[CNX_Mp3Manager]"
#include <NX_DbgMsg.h>

#ifndef	SAFE_DELETE_FILTER
#define	SAFE_DELETE_FILTER(A)	if(A){delete A;A=NULL;}
#endif
#ifndef	SAFE_START_FILTER
#define	SAFE_START_FILTER(A)    if(A){A->Run();};
#endif
#ifndef	SAFE_STOP_FILTER
#define	SAFE_STOP_FILTER(A)     if(A){A->Stop();};
#endif
#ifndef	SAFE_DEINIT_FILTER
#define	SAFE_DEINIT_FILTER(A)   if(A){A->Deinit();};
#endif

//------------------------------------------------------------------------------
static Mp3ManagerConfig defConfig = {
	2, 48000, 128000,
};

CNX_Mp3Manager::CNX_Mp3Manager()
	: m_pRefClock( NULL )
	, m_pNotifier( NULL )
	, m_pAudCapFilter( NULL )
	, m_pMp3EncFilter( NULL )
	, m_pFileWriter( NULL )
	, m_bInit( false )
	, m_bRun( false )
{
	SetConfig( &defConfig );
	pthread_mutex_init( &m_hLock, NULL );
}

//------------------------------------------------------------------------------
CNX_Mp3Manager::~CNX_Mp3Manager()
{
	if( m_bRun ) Stop();
	if( m_bInit ) Deinit();
	pthread_mutex_destroy( &m_hLock );
}

//------------------------------------------------------------------------------
int32_t cbFileNameCallback( uint8_t *buf, uint32_t bufSize )
{
	time_t eTime;
	struct tm *eTm;

	time( &eTime );
	eTm = localtime( &eTime );

	sprintf((char*)buf, "aud_%04d%02d%02d_%02d%02d%02d.mp3",
		eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec );

	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp3Manager::BuildFilter( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	m_pRefClock				= new CNX_RefClock();
	m_pNotifier 			= new CNX_Mp3Notify();

	m_pAudCapFilter			= new CNX_AudCaptureFilter();
	m_pMp3EncFilter			= new CNX_Mp3Encoder();
	m_pFileWriter			= new CNX_SimpleFileWriter();

	if( m_pAudCapFilter )	m_pAudCapFilter->Connect( m_pMp3EncFilter );
	if( m_pMp3EncFilter )	m_pMp3EncFilter->Connect( m_pFileWriter );

	if( m_pAudCapFilter )	m_pAudCapFilter->Init( &m_AudCapConfig );
	if( m_pMp3EncFilter )	m_pMp3EncFilter->Init( &m_AudEncConfig );
	if( m_pFileWriter )		m_pFileWriter->Init();
	if( m_pFileWriter )		m_pFileWriter->RegFileNameCallback( &cbFileNameCallback );

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
#ifndef SET_EVENT_NOTIFIER
#define SET_EVENT_NOTIFIER(A, B)	if(A){(A)->SetNotifier((INX_EventNotify *)B);};
#endif

void CNX_Mp3Manager::SetNotifier( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	
	if( m_pNotifier )
	{
		SET_EVENT_NOTIFIER( m_pAudCapFilter,	m_pNotifier );
		SET_EVENT_NOTIFIER( m_pMp3EncFilter,	m_pNotifier );
		SET_EVENT_NOTIFIER( m_pFileWriter,		m_pNotifier );
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t CNX_Mp3Manager::SetConfig( Mp3ManagerConfig *pConfig )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	m_AudCapConfig.channels			= pConfig->channel;
	m_AudCapConfig.frequency		= pConfig->frequency;
	m_AudCapConfig.samples			= FRAME_SIZE_MP3;

	m_AudEncConfig.channels			= pConfig->channel;
	m_AudEncConfig.frequency		= pConfig->frequency;
	m_AudEncConfig.bitrate			= pConfig->bitrate;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp3Manager::Init( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );
	int32_t ret = 0;

	if( !m_bInit )
	{
		ret = BuildFilter();
		if( !ret )
		{
			SetNotifier();
			m_bInit = true;
		}
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return ret;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp3Manager::Deinit( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_bInit )
	{
		SAFE_DEINIT_FILTER( m_pAudCapFilter );
		SAFE_DEINIT_FILTER( m_pMp3EncFilter );
		SAFE_DEINIT_FILTER( m_pFileWriter );

		SAFE_DELETE_FILTER( m_pAudCapFilter );
		SAFE_DELETE_FILTER( m_pMp3EncFilter );
		SAFE_DELETE_FILTER( m_pFileWriter );
		SAFE_DELETE_FILTER( m_pNotifier );	

		m_bInit = false;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;	
}

//------------------------------------------------------------------------------
int32_t CNX_Mp3Manager::Start( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_bInit && !m_bRun )
	{
		SAFE_START_FILTER( m_pAudCapFilter );
		SAFE_START_FILTER( m_pMp3EncFilter );
		SAFE_START_FILTER( m_pFileWriter );
		SAFE_START_FILTER( m_pNotifier );

		if( m_pFileWriter ) m_pFileWriter->EnableWriting( true );

		m_bRun = true;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp3Manager::Stop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_bRun )
	{
		if( m_pFileWriter ) m_pFileWriter->EnableWriting( false );

		SAFE_STOP_FILTER( m_pAudCapFilter );
		SAFE_STOP_FILTER( m_pMp3EncFilter );
		SAFE_STOP_FILTER( m_pFileWriter );
		SAFE_STOP_FILTER( m_pNotifier );

		m_bRun = false;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp3Manager::RegisterNotifyCallback( uint32_t (*cbNotify)(uint32_t, uint8_t*, uint32_t) )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );
	
	if( cbNotify )
	{
		m_pNotifier->RegisterNotifyCallback( cbNotify );
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}