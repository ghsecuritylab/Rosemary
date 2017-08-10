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

#ifndef __CNX_USERTEXTOVERLAYFILTER_H__
#define __CNX_USERTEXTOVERLAYFILTER_H__

#include <nx_alloc_mem.h>

#include "CNX_BaseFilter.h"
#include "NX_FilterConfigTypes.h"

#ifdef __cplusplus

class CNX_UserOverlayFilter
	: public CNX_BaseFilter
{
public:
	CNX_UserOverlayFilter();
	~CNX_UserOverlayFilter();

public:
	//------------------------------------------------------------------------
	virtual void		Init( NX_USEROVERLAY_CONFIG *pConfig );
	virtual void		Deinit( void );
	//------------------------------------------------------------------------
	//	Override from CNX_BaseFilter
	//------------------------------------------------------------------------
	virtual int32_t		Receive( CNX_Sample *pSample );
	virtual int32_t		ReleaseSample( CNX_Sample *pSample );

	virtual int32_t		Run( void );
	virtual int32_t		Stop( void );
	//------------------------------------------------------------------------

protected:
	virtual void		AllocateBuffer( int32_t width, int32_t height, int32_t alignx, int32_t aligny, int32_t numOfBuffer, uint32_t dwFourCC );
	virtual void		FreeBuffer( void );

	virtual int32_t		GetSample( CNX_Sample **ppSample );
	virtual int32_t		GetDeliverySample( CNX_Sample **ppSample );

protected:
			void		ThreadLoop( void );
	static	void*		ThreadMain( void *arg );

public:
	//------------------------------------------------------------------------
	//	External Interfaces
	//------------------------------------------------------------------------
			void 		RegUserOverlayCallback( int32_t(*cbFunc)( NX_VID_MEMORY_INFO *) );

protected:
	//------------------------------------------------------------------------
	//	Filter status
	//------------------------------------------------------------------------
	int32_t					m_bInit;
	int32_t					m_bRun;
	CNX_Semaphore			*m_pSemIn;
	CNX_Semaphore			*m_pSemOut;
	pthread_mutex_t			m_hLock;
	//------------------------------------------------------------------------
	//	Thread
	//------------------------------------------------------------------------
	int32_t					m_bThreadExit;
	pthread_t				m_hThread;
	//------------------------------------------------------------------------
	//	Input / Output Buffer
	//------------------------------------------------------------------------
	enum { MAX_BUFFER = 12, NUM_ALLOC_BUFFER = 4 };
	
	int32_t					m_iNumOfBuffer;
	CNX_SampleQueue			m_SampleInQueue;
	CNX_SampleQueue			m_SampleOutQueue;
	NX_VID_MEMORY_HANDLE	m_VideoMemory[MAX_BUFFER];
	CNX_VideoSample			m_VideoSample[MAX_BUFFER];
	//------------------------------------------------------------------------
	//	UserOverlay
	//------------------------------------------------------------------------
	int32_t		(*UserOverlayCallbackFunc)( NX_VID_MEMORY_INFO *pVideoMemory );
};

#endif	//	__cplusplus

#endif	// __CNX_USERTEXTOVERLAYFILTER_H__			
