//--------------------------------------------------------------------------------
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
//--------------------------------------------------------------------------------

#ifndef __NX_IDUALAUDIO_H__
#define __NX_IDUALAUDIO_H__

#include <stdint.h>

class NX_IDualAudio
{
public:
	virtual ~NX_IDualAudio() {};

public:
	virtual int32_t		SetConfig( int32_t iCard, int32_t iDevice, int32_t iChannelMask, int32_t iSampleRate, int32_t iFormatMask ) = 0;
	virtual int32_t		SetVolume( int32_t iStreamType, float fVolumeWeight ) = 0;

	virtual int32_t		AddTrack( int32_t iIndex, int32_t iStreamType, int32_t bDuplicate, int32_t iChannelMask, int32_t iSampleRate, int32_t iFormatMask ) = 0;
	virtual int32_t		RemoveTrack( int32_t iIndex ) = 0;

	virtual int32_t		Write( int32_t iIndex, int8_t *pBuf, int32_t iBufSize ) = 0;
	virtual int32_t		Flush( int32_t iIndex ) = 0;

public:
	virtual int32_t		GetVersion( int32_t *iMajor, int32_t *iMinor, int32_t *iRevision, int32_t *iReservced ) = 0;
	virtual int32_t		ChangeDebugLevel( int32_t iDebugLevel ) = 0;
};

extern NX_IDualAudio*	GetDualAudioInstance( void );
extern void				ReleaseDualAudioInstance( void );

#endif	// __NX_IDUALAUDIO_H__
