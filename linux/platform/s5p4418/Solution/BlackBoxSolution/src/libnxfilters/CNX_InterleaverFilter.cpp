//------------------------------------------------------------------------------
//
//	Copyright (C) 2013 Nexell Co. All Rights Reserved
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <CNX_InterleaverFilter.h>

#define	NX_DTAG	"[CNX_InterleaverFilter] "
#include <NX_DbgMsg.h>

//------------------------------------------------------------------------------
CNX_InterleaverFilter::CNX_InterleaverFilter()
	: m_bInit( false )
	, m_bRun( false )
	, m_bEnable( true )
	, m_bStartInterleaver( false )
{
	memset( &m_FilterStatistics, 0x00, sizeof(NX_FILTER_STATISTICS) );
	pthread_mutex_init( &m_hLock, NULL );
	pthread_mutex_init( &m_hStatisticsLock, NULL );
}

//------------------------------------------------------------------------------
CNX_InterleaverFilter::~CNX_InterleaverFilter()
{
	if( true == m_bInit )
		Deinit();
	pthread_mutex_destroy( &m_hLock );
	pthread_mutex_destroy( &m_hStatisticsLock );
}

//------------------------------------------------------------------------------
void CNX_InterleaverFilter::Init(NX_INTERLEAVER_CONFIG *pConfig)
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	m_InterleaverChannel = pConfig->channel;
	for(uint32_t i = 0; i < m_InterleaverChannel; i++) {
		m_InterleaverQueue[i].Reset();
	}
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
void CNX_InterleaverFilter::Deinit()
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
}

//------------------------------------------------------------------------------
int32_t	CNX_InterleaverFilter::Receive( CNX_Sample *pSample)
{
	CNX_AutoLock lock ( &m_hLock );
	if( !m_bEnable )
		return true;

	int32_t bLoop = true;

	CNX_MuxerSample *pDestSample = NULL;
	CNX_MuxerSample *pOutSample = NULL;

	uint8_t *pSrcBuf = NULL;
	uint8_t *pDestBuf = NULL;
	
	int32_t size, type, key, flags;
	uint64_t timeStamp;

	pSample->Lock();
	
	// 1. Get Source Sample Properties
	((CNX_MuxerSample*)pSample)->GetBuffer( &pSrcBuf, &size );
	type		= ((CNX_MuxerSample*)pSample)->GetDataType();
	key			= ((CNX_MuxerSample*)pSample)->GetSyncPoint();
	size		= ((CNX_MuxerSample*)pSample)->GetActualDataLength();
	timeStamp	= ((CNX_MuxerSample*)pSample)->GetTimeStamp();
	flags		= ((CNX_MuxerSample*)pSample)->GetFlags();
	
	// 2. Video / Audio sync
	if( m_bStartInterleaver == false ) {
		if( type == CNX_MuxerSample::DATA_VIDEO0 ) {
			m_bStartInterleaver = true;
		}
		else {
			pSample->Unlock();
			return false;
		}
	}

	// 3. Prepare Destination Sample
	pDestSample	= new CNX_MuxerSample();
	pDestBuf	= (uint8_t*)malloc( size );

	if( !pDestSample ) {
		NxDbgMsg( NX_DBG_ERR, (TEXT("Sample is NULL.\n")) );
		pSample->Unlock();
		return false;
	}
	if( !pDestBuf ) {
		NxDbgMsg( NX_DBG_ERR, (TEXT("Buffer is NULL.\n")) );
		pSample->Unlock();
		return false;
	}

	memcpy( pDestBuf, pSrcBuf, size );
	pSample->Unlock();
	
	pDestSample->SetOwner( this );
	pDestSample->SetBuffer( pDestBuf, size );
	pDestSample->SetActualDataLength( size );
	pDestSample->SetSyncPoint( key );
	pDestSample->SetDataType( type );
	pDestSample->SetTimeStamp( timeStamp );
	pDestSample->SetFlags( flags );

	// Debug code
	// static int32_t bEmpty = false;
	// if( bEmpty ) {
	// 	printf("id[%d], timeStamp[%lld], pDestBuf[%p], size[%d], bufCount[%d]\n", type, timeStamp, pDestBuf, size, m_InterleaverQueue[type].GetSampleCount());
	// }

	// 4. Interleave Samples
	// 4-1. Check Interleaver Queue. ( Is Full - Force Delivery )
	if( m_InterleaverQueue[type].IsFull() ) {
		NxDbgMsg( NX_DBG_WARN, (TEXT("Buffer(id = %d) is Full. Force Delivery.\n"), type) );
		m_InterleaverQueue[type].PopSample( (CNX_Sample**)&pOutSample );
		Deliver( pOutSample );

		// Debug Code
		// bEmpty = true;
	}

	// 4-2. Push New Sample at Interleaver Queue.
	m_InterleaverQueue[type].PushSample( (CNX_Sample*)pDestSample );

	// 4-3. Check Interleaver Queue. ( Is Empty - Stand by )
	for(uint32_t i = 0; i < m_InterleaverChannel; i++)
	{
		if( !m_InterleaverQueue[i].IsReady() ) {
			return false;
		}
	}

	// 4-4. Delivery Samples.
	while( bLoop )
	{
		// a. Search fastest samples
		type = 0;
		timeStamp = m_InterleaverQueue[type].GetSampleTimeStamp();
		for( uint32_t i = 1; i < m_InterleaverChannel; i++ ) {
			if( timeStamp > m_InterleaverQueue[i].GetSampleTimeStamp() ) {
				timeStamp = m_InterleaverQueue[i].GetSampleTimeStamp();
				type  = i;
			}
		}

		// b. Fastest samples deliver.
		m_InterleaverQueue[type].PopSample( (CNX_Sample**)&pOutSample );
		//NxDbgMsg( NX_DBG_DEBUG, (TEXT("[ID = %d][TimeStamp = %lld]\n"), ((CNX_MuxerSample*)pOutSample)->GetDataType(), ((CNX_MuxerSample*)pOutSample)->GetTimeStamp()) );
		Deliver( pOutSample );
		
		// c. Loop until Interleaver Queue Empty.
		for(uint32_t i = 0; i < m_InterleaverChannel; i++)
		{
			if( !m_InterleaverQueue[i].IsReady() ) {
				bLoop = false;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_InterleaverFilter::ReleaseSample( CNX_Sample *pSample )
{
	uint8_t *pBuf = NULL;
	int32_t size = 0;

	((CNX_MuxerSample*)pSample)->GetBuffer( &pBuf, &size );
	
	if( pBuf ) {
		free( pBuf );
		pBuf = NULL;
	}
	if( pSample ) {
		delete (CNX_MuxerSample*)pSample;
		pSample = NULL;
	}

	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_InterleaverFilter::Run( void )
{
	if( m_bRun == false ) {
		m_bRun = true;
		m_bStartInterleaver = false;
	}
	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_InterleaverFilter::Stop( void )
{
	if( true == m_bRun ) {
		m_bRun = false;
		m_bStartInterleaver = false;
	}
	return true;
}

//------------------------------------------------------------------------------
int32_t	CNX_InterleaverFilter::EnableFilter( uint32_t enable )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );
	CNX_AutoLock lock( &m_hLock );
	NxDbgMsg( NX_DBG_DEBUG, (TEXT("%s : %s -- > %s\n"), __FUNCTION__, (m_bEnable)?"Enable":"Disable", (enable)?"Enable":"Disable") );

	Flush();
	m_bEnable = enable;

	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_InterleaverFilter::GetStatistics( NX_FILTER_STATISTICS *pStatistics )
{
	NX_ASSERT( NULL != pStatistics );

	pthread_mutex_lock( &m_hStatisticsLock );
	memset( pStatistics, 0x00, sizeof(NX_FILTER_STATISTICS) );
	for( uint32_t i = 0; i < m_InterleaverChannel; i++ )
	{
		m_FilterStatistics.inBuf[i].limit 	= m_InterleaverQueue[i].GetMaxSampleCount();
		m_FilterStatistics.inBuf[i].cur		= m_InterleaverQueue[i].GetSampleCount();

		if( m_FilterStatistics.inBuf[i].cur > m_FilterStatistics.inBuf[i].max )
			m_FilterStatistics.inBuf[i].max	= m_FilterStatistics.inBuf[i].cur;
	}

	memcpy( pStatistics, &m_FilterStatistics, sizeof(NX_FILTER_STATISTICS) );
	pthread_mutex_unlock( &m_hStatisticsLock );

	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_InterleaverFilter::Flush( void )
{
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()++\n"), __FUNCTION__) );

	for( uint32_t i = 0; i < m_InterleaverChannel; i++ )
	{
		while( 1 )
		{
			if( !m_InterleaverQueue[i].IsReady() ) break;

			CNX_MuxerSample *pOutSample = NULL;
			m_InterleaverQueue[i].PopSample( (CNX_Sample**)&pOutSample );
			if( pOutSample ) ReleaseSample( pOutSample );
		}
	}

	m_bStartInterleaver = false;
	NxDbgMsg( NX_DBG_VBS, (TEXT("%s()--\n"), __FUNCTION__) );
	return true;
}