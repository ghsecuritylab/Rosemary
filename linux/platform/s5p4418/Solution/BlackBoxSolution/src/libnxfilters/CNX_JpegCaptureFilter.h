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

#ifndef __CNX_JPEGCAPTUREFILTER_H__
#define __CNX_JPEGCAPTUREFILTER_H__

#include <CNX_BaseFilter.h>
#include <NX_FilterConfigTypes.h>
#include "INX_JpegCapture.h"

#include <nx_dsp.h>

#ifdef __cplusplus

class	CNX_JpegCaptureFilter
	: public CNX_BaseFilter
{
public:
	CNX_JpegCaptureFilter();
	virtual ~CNX_JpegCaptureFilter();

public:
	//------------------------------------------------------------------------
	virtual void	Init( void );
	virtual void	Deinit( void );
	//------------------------------------------------------------------------
	//	Override from CNX_BaseFilter
	//------------------------------------------------------------------------
	virtual	int32_t	Receive( CNX_Sample *pSample );
	virtual int32_t	ReleaseSample( CNX_Sample *pSample );

	virtual	int32_t	Run( void );
	virtual	int32_t	Stop( void );
	//------------------------------------------------------------------------

private:
			int32_t JpegEncode( CNX_Sample *pSample );

public:
	//------------------------------------------------------------------------
	//	External Interfaces
	//------------------------------------------------------------------------
			void 	RegFileNameCallback( int32_t (*cbFunc)(uint8_t *, uint32_t) );
			int32_t Capture( int32_t iCount );
			int32_t CaptureWait( int32_t iCount );

protected:
	//------------------------------------------------------------------------
	//	Filter status
	//------------------------------------------------------------------------
	int32_t					m_bInit;
	int32_t					m_bRun;
	pthread_mutex_t			m_hLock;
	//------------------------------------------------------------------------
	//	Capture
	//------------------------------------------------------------------------
	INX_JpegCapture			*m_pJpegCapture;
	int32_t					m_iCaptureCount;
	int32_t					m_bCaptureWait;

	CNX_Semaphore			*m_pSemWait;
};

#endif //	__cplusplus

#endif //	__CNX_JPEGCAPTUREFILTER_H__

