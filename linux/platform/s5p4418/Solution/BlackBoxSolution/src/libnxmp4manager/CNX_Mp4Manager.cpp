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

#include "CNX_Mp4Manager.h"

#define NX_DTAG		"[CNX_Mp4Manager] "
#include <NX_DbgMsg.h>

static Mp4ManagerConfig defConfig = {
	0, 640, 480, 30, 
	6000000,
	false,
	0, 0, 640, 480,
	1280, 720
};

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
CNX_Mp4Manager::CNX_Mp4Manager()
	: m_pRefClock( NULL )
	, m_pNotifier( NULL )
	, m_pVipFilter( NULL )
	, m_pVrFilter( NULL )
	, m_pAvcEncFilter( NULL )
	, m_pAudCapFilter( NULL )
	, m_pAacEncFilter( NULL )
	, m_pInterleaverFilter( NULL )
	, m_pMp4MuxerFilter( NULL )
	, m_bInit( false )
	, m_bRun( false )
	
{
	SetConfig( &defConfig );
	pthread_mutex_init( &m_hLock, NULL );

	NxChgFilterDebugLevel( NX_DBG_VBS );
}

//------------------------------------------------------------------------------
CNX_Mp4Manager::~CNX_Mp4Manager()
{
	if( m_bRun )	Stop();
	if( m_bInit )	Deinit();
	pthread_mutex_destroy( &m_hLock );
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::SetConfig( Mp4ManagerConfig *pConfig )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	memset( &m_VipConfig, 0x00, sizeof(m_VipConfig) );
	m_VipConfig.port				= pConfig->port;
	m_VipConfig.width				= pConfig->width;
	m_VipConfig.height				= pConfig->height;
	m_VipConfig.fps					= pConfig->fps;

	memset( &m_VipCaptureConfig, 0x00, sizeof(m_VipCaptureConfig) );
	m_VipCaptureConfig.width		= pConfig->captureWidth;
	m_VipCaptureConfig.height		= pConfig->captureHeight;

	memset( &m_VidRenderConfig, 0x00, sizeof(m_VidRenderConfig) );
	m_VidRenderConfig.port 			= 0;
	m_VidRenderConfig.width			= pConfig->width;
	m_VidRenderConfig.height 		= pConfig->height;
	m_VidRenderConfig.cropLeft		= 0;
	m_VidRenderConfig.cropTop		= 0;
	m_VidRenderConfig.cropRight		= pConfig->width;
	m_VidRenderConfig.cropBottom	= pConfig->height;
	m_VidRenderConfig.dspLeft		= pConfig->dspLeft;
	m_VidRenderConfig.dspTop		= pConfig->dspTop;
	m_VidRenderConfig.dspRight		= pConfig->dspRight;
	m_VidRenderConfig.dspBottom		= pConfig->dspBottom;

	memset( &m_VidEncConfig, 0x00, sizeof(m_VidEncConfig) );
	m_VidEncConfig.width			= pConfig->width;
	m_VidEncConfig.height			= pConfig->height;
	m_VidEncConfig.fps				= pConfig->fps;
	m_VidEncConfig.bitrate			= pConfig->bitrate;
	m_VidEncConfig.codec			= MP4_CODEC_TYPE_H264;

	memset( &m_AudCapConfig, 0x00, sizeof(m_AudCapConfig) );
	m_AudCapConfig.channels			= 2;
	m_AudCapConfig.frequency		= 48000;
	m_AudCapConfig.samples			= FRAME_SIZE_AAC;

	memset( &m_AudEncConfig, 0x00, sizeof(m_AudEncConfig) );
	m_AudEncConfig.channels			= 2;
	m_AudEncConfig.frequency		= 48000;
	m_AudEncConfig.bitrate			= 128000;
	m_AudEncConfig.codec			= MP4_CODEC_TYPE_AAC;

	memset( &m_InterleaverConfig, 0x00, sizeof(m_InterleaverConfig) );
	m_InterleaverConfig.channel		= pConfig->bAudEnable ? 2 : 1;

	memset( &m_Mp4MuxerConfig, 0x00, sizeof(m_Mp4MuxerConfig) );
	m_Mp4MuxerConfig.videoTrack		= 1;
	m_Mp4MuxerConfig.audioTrack		= pConfig->bAudEnable;
	m_Mp4MuxerConfig.textTrack		= 0;
	
	m_Mp4MuxerConfig.trackConfig[0].width		= pConfig->width;
	m_Mp4MuxerConfig.trackConfig[0].height		= pConfig->height;
	m_Mp4MuxerConfig.trackConfig[0].frameRate	= pConfig->fps;
	m_Mp4MuxerConfig.trackConfig[0].bitrate		= pConfig->bitrate;
	m_Mp4MuxerConfig.trackConfig[0].codecType	= MP4_CODEC_TYPE_H264;

	m_Mp4MuxerConfig.trackConfig[1].channel		= 2;
	m_Mp4MuxerConfig.trackConfig[1].frequency	= 48000;
	m_Mp4MuxerConfig.trackConfig[1].bitrate		= 128000;
	m_Mp4MuxerConfig.trackConfig[1].codecType	= MP4_CODEC_TYPE_AAC;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::BuildFilter( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	m_pRefClock				= new CNX_RefClock();
	m_pNotifier 			= new CNX_Mp4Notify();

	m_pVipFilter			= new CNX_VIPFilter();
	m_pVrFilter				= new CNX_VRFilter();
	m_pAvcEncFilter 		= new CNX_H264Encoder();

	if( m_Mp4MuxerConfig.audioTrack ) {
		m_pAudCapFilter		= new CNX_AudCaptureFilter();
		m_pAacEncFilter		= new CNX_AacEncoder();
	}

	m_pInterleaverFilter 	= new CNX_InterleaverFilter();
	m_pMp4MuxerFilter		= new CNX_Mp4MuxerFilter();

	if( m_pVipFilter )			m_pVipFilter->Connect( m_pVrFilter );
	if( m_pVrFilter )			m_pVrFilter->Connect( m_pAvcEncFilter );
	if( m_pAvcEncFilter )		m_pAvcEncFilter->Connect( m_pInterleaverFilter );

	if( m_pAudCapFilter )		m_pAudCapFilter->Connect( m_pAacEncFilter );
	if( m_pAacEncFilter )		m_pAacEncFilter->Connect( m_pInterleaverFilter );
	
	if( m_pInterleaverFilter )	m_pInterleaverFilter->Connect( m_pMp4MuxerFilter );

	if( m_pVipFilter )			m_pVipFilter->Init( &m_VipConfig );
	if( m_pVrFilter )			m_pVrFilter->Init( &m_VidRenderConfig );
	if( m_pAvcEncFilter )		m_pAvcEncFilter->Init( &m_VidEncConfig );

	if( m_pAudCapFilter )		m_pAudCapFilter->Init( &m_AudCapConfig );
	if( m_pAacEncFilter )		m_pAacEncFilter->Init( &m_AudEncConfig );

	if( m_pInterleaverFilter )	m_pInterleaverFilter->Init( &m_InterleaverConfig );
	if( m_pMp4MuxerFilter )		m_pMp4MuxerFilter->Init( &m_Mp4MuxerConfig );

	uint8_t dsiInfo[24] = { 0x00, };
	int32_t dsiSize = 0;

	if( m_pAvcEncFilter )	m_pAvcEncFilter->SetPacketID( 0 );
	if( m_pAvcEncFilter )	m_pAvcEncFilter->GetDsiInfo( dsiInfo, &dsiSize );
	if( m_pMp4MuxerFilter )	m_pMp4MuxerFilter->SetDsiInfo( 0, dsiInfo, dsiSize );

	if( m_pAacEncFilter )	m_pAacEncFilter->SetPacketID( 1 );
	if( m_pAacEncFilter )	m_pAacEncFilter->GetDsiInfo( dsiInfo, &dsiSize );
	if( m_pMp4MuxerFilter )	m_pMp4MuxerFilter->SetDsiInfo( 1, dsiInfo, dsiSize );

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
#ifndef SET_EVENT_NOTIFIER
#define SET_EVENT_NOTIFIER(A, B)	if(A){(A)->SetNotifier((INX_EventNotify *)B);};
#endif

void CNX_Mp4Manager::SetNotifier( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	
	if( m_pNotifier )
	{
		SET_EVENT_NOTIFIER( m_pVipFilter,			m_pNotifier );
		SET_EVENT_NOTIFIER( m_pVrFilter,			m_pNotifier );
		SET_EVENT_NOTIFIER( m_pAvcEncFilter,		m_pNotifier );
		SET_EVENT_NOTIFIER( m_pAudCapFilter,		m_pNotifier );
		SET_EVENT_NOTIFIER( m_pAacEncFilter,		m_pNotifier );
		SET_EVENT_NOTIFIER( m_pInterleaverFilter, 	m_pNotifier );
		SET_EVENT_NOTIFIER( m_pMp4MuxerFilter,		m_pNotifier );
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::Init( Mp4ManagerConfig *pConfig )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( !m_bInit )
	{
		if( pConfig )	SetConfig( pConfig );
		else			SetConfig( &defConfig );
		
		BuildFilter();
		SetNotifier();
		
		m_bInit = true;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::Deinit( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_bInit )
	{
		SAFE_DEINIT_FILTER( m_pVipFilter );
		SAFE_DEINIT_FILTER( m_pVrFilter );
		SAFE_DEINIT_FILTER( m_pAvcEncFilter );
		SAFE_DEINIT_FILTER( m_pAudCapFilter );
		SAFE_DEINIT_FILTER( m_pAacEncFilter );
		SAFE_DEINIT_FILTER( m_pInterleaverFilter );
		SAFE_DEINIT_FILTER( m_pMp4MuxerFilter );

		SAFE_DELETE_FILTER( m_pNotifier );
		SAFE_DELETE_FILTER( m_pVipFilter );
		SAFE_DELETE_FILTER( m_pVrFilter );
		SAFE_DELETE_FILTER( m_pAvcEncFilter );
		SAFE_DELETE_FILTER( m_pAudCapFilter );
		SAFE_DELETE_FILTER( m_pAacEncFilter );
		SAFE_DELETE_FILTER( m_pInterleaverFilter );
		SAFE_DELETE_FILTER( m_pMp4MuxerFilter );

		m_bInit = false;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;	
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::SetFileName( char *pFileName )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_pMp4MuxerFilter )	m_pMp4MuxerFilter->SetFileName( pFileName );

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::Start( int32_t bEncode )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_bInit && !m_bRun )
	{
		SAFE_START_FILTER( m_pNotifier );
		SAFE_START_FILTER( m_pVipFilter );
		SAFE_START_FILTER( m_pVrFilter );
		SAFE_START_FILTER( m_pAvcEncFilter );
		SAFE_START_FILTER( m_pAudCapFilter );
		SAFE_START_FILTER( m_pAacEncFilter );
		SAFE_START_FILTER( m_pInterleaverFilter );
		SAFE_START_FILTER( m_pMp4MuxerFilter );

		if( m_pAvcEncFilter )		m_pAvcEncFilter->EnableFilter( bEncode );
		if( m_pAacEncFilter )		m_pAacEncFilter->EnableFilter( bEncode );
		if( m_pInterleaverFilter )	m_pInterleaverFilter->EnableFilter( bEncode );
		if( m_pMp4MuxerFilter )		m_pMp4MuxerFilter->EnableMp4Muxing( bEncode );
		if( m_pVrFilter ) 			m_pVrFilter->EnableRender( true );

		m_bRun = true;
		m_bEncode = bEncode;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::Stop( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_bRun )
	{
		SAFE_STOP_FILTER( m_pMp4MuxerFilter );
		SAFE_STOP_FILTER( m_pInterleaverFilter );
		SAFE_STOP_FILTER( m_pVipFilter );
		SAFE_STOP_FILTER( m_pVrFilter );
		SAFE_STOP_FILTER( m_pAvcEncFilter );
		SAFE_STOP_FILTER( m_pAudCapFilter );
		SAFE_STOP_FILTER( m_pAacEncFilter );
		SAFE_STOP_FILTER( m_pNotifier );

		if( m_pInterleaverFilter )	m_pInterleaverFilter->EnableFilter( false );
		if( m_pMp4MuxerFilter )		m_pMp4MuxerFilter->EnableMp4Muxing( false );
		if( m_pVrFilter ) 			m_pVrFilter->EnableRender( false );
		
		m_bRun = false;
		m_bEncode = false;
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::Capture( char *pFileName )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( m_bEncode ) {
		if( pFileName ) m_pVipFilter->SetJpegFileName( (uint8_t*)pFileName );
		m_pVipFilter->Capture();
	}
	else {
		if( pFileName )	m_pVipFilter->SetJpegFileName( (uint8_t*)pFileName );

		m_pVrFilter->Pause( true );
		m_pVipFilter->CaptureResize( &m_VipCaptureConfig );
		m_pVrFilter->Pause( false );
	}

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::EnableEncode( int32_t bEnable )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );

	if( !m_bRun )
	{
		NxDbgMsg( NX_DBG_WARN, (TEXT("not running.\n")) );
		goto END;
	}

	if( m_bEncode == bEnable ) 
		goto END;

	if( bEnable ) {
		if( m_pAvcEncFilter )		m_pAvcEncFilter->EnableFilter( true );
		if( m_pAacEncFilter )		m_pAacEncFilter->EnableFilter( true );
		if( m_pInterleaverFilter ) 	m_pInterleaverFilter->EnableFilter( true );
		if( m_pMp4MuxerFilter )		m_pMp4MuxerFilter->EnableMp4Muxing( true );
	}
	else {
		if( m_pAvcEncFilter )		m_pAvcEncFilter->EnableFilter( false );
		if( m_pAacEncFilter )		m_pAacEncFilter->EnableFilter( false );
		if( m_pInterleaverFilter ) 	m_pInterleaverFilter->EnableFilter( false );
		if( m_pMp4MuxerFilter )		m_pMp4MuxerFilter->EnableMp4Muxing( false );
	}

	m_bEncode = bEnable;

END:
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_Mp4Manager::RegisterNotifyCallback( uint32_t (*cbNotify)(uint32_t, uint8_t*, uint32_t) )
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

//------------------------------------------------------------------------------
CNX_Mp4Manager* CNX_Mp4Manager::m_psInstance = NULL;

INX_Mp4Manager* CNX_Mp4Manager::GetInstance()
{
	if( NULL == m_psInstance )
	{
		m_psInstance = new CNX_Mp4Manager();
	}

	return (INX_Mp4Manager*)m_psInstance;
}

//------------------------------------------------------------------------------
void CNX_Mp4Manager::ReleaseIntance()
{
	if( NULL != m_psInstance )
	{
		delete m_psInstance;
	}

	m_psInstance = NULL;
}

//------------------------------------------------------------------------------
INX_Mp4Manager *GetMp4ManagerHandle( void )
{
	return CNX_Mp4Manager::GetInstance();
}

//------------------------------------------------------------------------------
void ReleaseMp4ManagerHandle( void )
{
	CNX_Mp4Manager::ReleaseIntance();
}