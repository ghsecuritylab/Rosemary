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

#ifndef __CNX_SCALERFILTER_H__
#define __CNX_SCALERFILTER_H__

#include <stdint.h>
extern "C" {
#include <libnxscaler.h>
};

#include "CNX_BaseFilter.h"
#include "NX_FilterConfigTypes.h"

#ifdef __cplusplus

class CNX_ScalerFilter
	: public CNX_BaseFilter
{
public:
	CNX_ScalerFilter();
	~CNX_ScalerFilter();

public:
	//------------------------------------------------------------------------
	virtual void		Init( NX_SCALER_CONFIG *pConfig );
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
	virtual void		AllocateBuffer(int32_t width, int32_t height, int32_t alignx, int32_t aligny, int32_t numOfBuffer, uint32_t dwFourCC);
	virtual void		FreeBuffer( void );
	
	virtual	int32_t		GetSample( CNX_Sample **ppSample );
	virtual int32_t		GetDeliverySample( CNX_Sample **pSample );
	
protected:
			void		ThreadLoop( void );
	static 	void*		ThreadMain( void* arg );

private:
			int32_t		DigitalZoom( NX_VID_MEMORY_HANDLE hSrcMemory, NX_VID_MEMORY_HANDLE hOutMemory );

public:
	//------------------------------------------------------------------------
	//	External Interfaces
	//------------------------------------------------------------------------
			int32_t		SetDigitalZoomLevel( float iLevel );

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
	//	Input / Output buffer
	//------------------------------------------------------------------------
	enum {	MAX_BUFFER = 12, NUM_ALLOC_BUFFER = 4 };

	int32_t					m_iNumOfBuffer;
	CNX_SampleQueue			m_SampleInQueue;
	CNX_SampleQueue			m_SampleOutQueue;
	NX_VID_MEMORY_HANDLE	m_VideoMemory[MAX_BUFFER];
	CNX_VideoSample			m_VideoSample[MAX_BUFFER];
	//------------------------------------------------------------------------
	//	Scaling
	//------------------------------------------------------------------------
	NX_SCALER_HANDLE		m_hScaler;
	float					m_iZoomLevel;
};	

#endif	//	__cplusplus

#endif	//	__CNX_SCALERFILTER_H__ 