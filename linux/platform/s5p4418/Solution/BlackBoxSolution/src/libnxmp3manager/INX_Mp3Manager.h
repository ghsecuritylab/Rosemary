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

#ifndef __INX_MP3MANAGER_H__
#define __INX_MP3MANAGER_H__

typedef struct tagMp3ManagerConfig {
	int32_t			channel;
	int32_t 		frequency;
	int32_t			bitrate;
} Mp3ManagerConfig;

typedef enum {
	NX_NOTIFY_FILEWRITING_DONE	= 0x1001,
	NX_NOTIFY_ERR_OPEN_FAIL		= 0xF003,
	NX_NOTIFY_ERR_WRITE			= 0xF004,
} NX_MP3MGR_NOTIFY_TYPE;

#include <stdint.h>

class INX_Mp3Manager
{
public:
	virtual ~INX_Mp3Manager(){}

public:
	virtual int32_t	SetConfig( Mp3ManagerConfig *pConfig ) = 0;
	
	virtual int32_t	Init( void ) = 0;
	virtual int32_t	Deinit( void ) = 0;
	
	virtual int32_t	Start( void ) = 0;
	virtual int32_t	Stop( void ) = 0;

	virtual int32_t	RegisterNotifyCallback( uint32_t (*cbNotify)(uint32_t, uint8_t*, uint32_t) ) = 0;
};

extern INX_Mp3Manager *GetMp3ManagerHandle( void );
extern void ReleaseMp3ManagerHandle( INX_Mp3Manager *pMp4Manager );

#endif	// __INX_MP3MANAGER_H__
