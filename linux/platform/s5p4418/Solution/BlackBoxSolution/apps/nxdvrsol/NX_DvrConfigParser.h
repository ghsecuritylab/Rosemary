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

#ifndef __NX_DVRCONFIGPARSER_H__
#define __NX_DVRCONFIGPARSER_H__

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct CAM_INFO {
	int32_t		iPort;
	int32_t		iWidth;
	int32_t		iHeight;
	int32_t		iFps;
	int32_t		iBitrate;
} CAM_INFO;

typedef struct CONFIG_INFO {
	int32_t		iChannel;
	int32_t		bAudio;
	int32_t		bUserData;
	int32_t		iContainer;
	int32_t		bDisplay;
	int32_t		iNetwork;
	int32_t		bMotion;
	int32_t		iAudioEnc;

	CAM_INFO	cam0Info;
	CAM_INFO	cam1Info;
} CONFIG_INFO;

typedef struct	CONFIG_HANDLE_INFO	*CONFIG_HANDLE;

CONFIG_HANDLE	DvrConfigParserInit( const char *pPath );
void			DvrConfigParserDeinit( CONFIG_HANDLE hConfig );

int32_t			DvrConfigParser( CONFIG_HANDLE hConfig, CONFIG_INFO *pConfig );

#endif	// __NX_DVRCONFIGPARSER_H__