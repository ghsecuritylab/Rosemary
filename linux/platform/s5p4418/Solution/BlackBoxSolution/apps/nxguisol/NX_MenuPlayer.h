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

#ifndef __NX_MENUPLAYER_H__
#define	__NX_MENUPLAYER_H__

#include <CNX_BaseObject.h>

enum {
	PLAYER_STATUS_RUN,
	PLAYER_STATUS_STOP,
	PLAYER_STATUS_EOS,
};

CNX_BaseObject	*GetMenuPlayerHandle( SDL_Surface *pSurface, TTF_Font *pFont );
void 			ReleaseMenuPlayerHandle( CNX_BaseObject *pBaseWindow );

void			SetPlayerStatus( int32_t status );
int32_t			GetPlayerStatus( void );

#endif	// __NX_MENUPLAYER_H__


