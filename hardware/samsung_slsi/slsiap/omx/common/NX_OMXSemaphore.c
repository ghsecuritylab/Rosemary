//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module     : Semaphore Module
//	File       : 
//	Description:
//	Author     : RayPark
//	History    :
//------------------------------------------------------------------------------
#include <pthread.h>
#include <assert.h>

#include <NX_OMXSemaphore.h>
#include <NX_OMXMem.h>
#include <NX_MediaTypes.h>


//	Create & Initialization
NX_SEMAPHORE *NX_CreateSem( uint32 initValue, uint32 maxValue )
{
	//	Create Semaphore
	NX_SEMAPHORE *hSem = NxMalloc( sizeof(NX_SEMAPHORE) );
	if( NULL == hSem )
		return NULL;
	hSem->nValue = initValue;
	hSem->nMaxValue = maxValue;
	if( 0 != pthread_cond_init( &hSem->hCond, NULL ) ){
		NxFree( hSem );
		return NULL;
	}
	if( 0 != pthread_mutex_init( &hSem->hMutex, NULL ) ){
		pthread_cond_destroy( &hSem->hCond );
		NxFree( hSem );
		return NULL;
	}
	return hSem;
}

void NX_DestroySem( NX_SEMAPHORE *hSem )
{
	//	Destroy Semaphore
	if( hSem ){
		pthread_cond_destroy( &hSem->hCond );
		pthread_mutex_destroy( &hSem->hMutex );
		NxFree( hSem );
	}
}

int32 NX_PendSem( NX_SEMAPHORE *hSem )
{
	int32 error = 0;
	assert( NULL != hSem );
	pthread_mutex_lock( &hSem->hMutex );

	//	Pending�� �䱸�ϴ� thread ���� post�� �� ���� �߻��Ͽ��� ��� signal�� ���� �Ǿ� 
	//	wait�� �� ��� dead lock�� �߻��� �� �ִ�.  ������ pthread_cond_wait�� �̿��Ͽ� 
	//	semaphore�� ������ ��쿡�� �ݵ�� value ���� 0�� ��쿡�� pending�Ѵ�.
	if( hSem->nValue == 0 )
		error = pthread_cond_wait( &hSem->hCond, &hSem->hMutex );
	if( 0 != error ){
		error = NX_ESEM;
	}else{
		hSem->nValue --;
	}
	pthread_mutex_unlock( &hSem->hMutex );
	return error;
}

//int32 NX_PendTimedSem( NX_SEMAPHORE *hSem, uint32 milliSeconds )
//{
//	return -1;
//}

int32 NX_PostSem( NX_SEMAPHORE *hSem )
{
	int32 error = 0;
	assert( NULL != hSem );
	pthread_mutex_lock( &hSem->hMutex );
	if( hSem->nValue >= hSem->nMaxValue ){
		error = NX_ESEM_OVERFLOW;
	}else{
		hSem->nValue ++;
	}
	pthread_cond_signal( &hSem->hCond );
	pthread_mutex_unlock( &hSem->hMutex );
	return error;
}
