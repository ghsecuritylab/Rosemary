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

#ifndef __CNX_MP3MANAGER_H__
#define __CNX_MP3MANAGER_H__

#include <CNX_RefClock.h>
#include <NX_FilterConfigTypes.h>

#include <CNX_AudCaptureFilter.h>
#include <CNX_Mp3Encoder.h>
#include <CNX_SimpleFileWriter.h>

#include "CNX_Mp3Notify.h"
#include "INX_Mp3Manager.h"

enum {
	FRAME_SIZE_AAC			= 1024,
	FRAME_SIZE_MP3			= 1152,
};

class CNX_Mp3Manager
	: public INX_Mp3Manager
{
public:
	CNX_Mp3Manager();
	~CNX_Mp3Manager();

public:
	virtual int32_t	SetConfig( Mp3ManagerConfig *pConfig );
	
	virtual int32_t Init( void );
	virtual int32_t Deinit( void );
	
	virtual int32_t Start( void );
	virtual int32_t Stop( void );

	virtual int32_t RegisterNotifyCallback( uint32_t (*cbNotify)(uint32_t, uint8_t *, uint32_t) );

private:
	int32_t BuildFilter( void );
	void	SetNotifier( void );

private:
	CNX_RefClock 			*m_pRefClock;
	CNX_Mp3Notify			*m_pNotifier;

	CNX_AudCaptureFilter	*m_pAudCapFilter;
	CNX_Mp3Encoder			*m_pMp3EncFilter;
	CNX_SimpleFileWriter	*m_pFileWriter;

	// Configuration
	NX_AUDCAPTURE_CONFIG	m_AudCapConfig;
	NX_AUDENC_CONFIG		m_AudEncConfig;

	Mp3ManagerConfig		m_ManagerConfig;

private:
	int32_t					m_bInit;
	int32_t					m_bRun;

	char 					m_FileName[1024];

	pthread_mutex_t			m_hLock;
};

INX_Mp3Manager *GetMp3ManagerHandle( void )
{
	return (INX_Mp3Manager *)new CNX_Mp3Manager();
}

void ReleaseMp3ManagerHandle( INX_Mp3Manager *pMp3Manager )
{
	if( pMp3Manager ) delete (CNX_Mp3Manager*)pMp3Manager;
}

#endif