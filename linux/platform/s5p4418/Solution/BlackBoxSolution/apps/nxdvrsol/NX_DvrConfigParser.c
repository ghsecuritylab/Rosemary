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

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "NX_DvrConfigParser.h"

#define CHANNEL			"CHANNEL"
#define AUDIO			"AUDIO"
#define USERDATA		"USERDATA"
#define CONTAINER		"CONTAINER"
#define DISPLAY			"DISPLAY"
#define NETWORK			"NETWORK"
#define MOTION			"MOTION"
#define AUDIO_ENC		"AUDIO_ENC"

#define CAM0_PORT		"CAM0_PORT"
#define CAM0_WIDTH		"CAM0_WIDTH"
#define CAM0_HEIGHT		"CAM0_HEIGHT"
#define CAM0_FPS		"CAM0_FPS"
#define CAM0_BITRATE	"CAM0_BITRATE"

#define CAM1_PORT		"CAM1_PORT"
#define CAM1_WIDTH		"CAM1_WIDTH"
#define CAM1_HEIGHT		"CAM1_HEIGHT"
#define CAM1_FPS		"CAM1_FPS"
#define CAM1_BITRATE	"CAM1_BITRATE"

typedef struct CONFIG_HANDLE_INFO {
	FILE			*fp;
} CONFIG_HANDLE_INFO;

int32_t GetParameter( FILE *fp, const char *pToken )
{
	rewind( fp );

	char strBuf[1024], token[1024];
	int32_t value;

	while( fgets( strBuf, sizeof(strBuf), fp) )
	{
		sscanf( strBuf, "%s %d", token, &value );

		if( !strcmp( token, pToken ) )
			return value;
	}

	return -1;
}

CONFIG_HANDLE DvrConfigParserInit( const char *pPath )
{
	FILE *fp = fopen( pPath, "r" );
	if( NULL == fp )
	{
		printf("%s(): Fail, file open.\n", __FUNCTION__ );
		return NULL;
	}

	CONFIG_HANDLE hConfig = (CONFIG_HANDLE_INFO*)malloc( sizeof(CONFIG_HANDLE_INFO) );
	memset( hConfig, 0x00, sizeof(CONFIG_HANDLE_INFO) );

	hConfig->fp = fp;

	return hConfig;
}

void DvrConfigParserDeinit( CONFIG_HANDLE hConfig )
{
	if( hConfig )
	{
		fclose( hConfig->fp );
		free( hConfig );
	}
}

int32_t DvrConfigParser( CONFIG_HANDLE hConfig, CONFIG_INFO *pConfig )
{
	if( !hConfig )
		return -1;

	pConfig->iChannel			= GetParameter( hConfig->fp, CHANNEL );
	pConfig->bAudio				= GetParameter( hConfig->fp, AUDIO );
	pConfig->bUserData			= GetParameter( hConfig->fp, USERDATA );
	pConfig->iContainer			= GetParameter( hConfig->fp, CONTAINER );
	pConfig->bDisplay			= GetParameter( hConfig->fp, DISPLAY );
	pConfig->iNetwork			= GetParameter( hConfig->fp, NETWORK );
	pConfig->bMotion			= GetParameter( hConfig->fp, MOTION );
	pConfig->iAudioEnc			= GetParameter( hConfig->fp, AUDIO_ENC );

	pConfig->cam0Info.iPort		= GetParameter( hConfig->fp, CAM0_PORT );
	pConfig->cam0Info.iWidth	= GetParameter( hConfig->fp, CAM0_WIDTH );
	pConfig->cam0Info.iHeight	= GetParameter( hConfig->fp, CAM0_HEIGHT );
	pConfig->cam0Info.iFps		= GetParameter( hConfig->fp, CAM0_FPS );
	pConfig->cam0Info.iBitrate	= GetParameter( hConfig->fp, CAM0_BITRATE );
	
	pConfig->cam1Info.iPort		= GetParameter( hConfig->fp, CAM1_PORT );
	pConfig->cam1Info.iWidth	= GetParameter( hConfig->fp, CAM1_WIDTH );
	pConfig->cam1Info.iHeight	= GetParameter( hConfig->fp, CAM1_HEIGHT );
	pConfig->cam1Info.iFps		= GetParameter( hConfig->fp, CAM1_FPS );
	pConfig->cam1Info.iBitrate	= GetParameter( hConfig->fp, CAM1_BITRATE );

	return 0;
}
