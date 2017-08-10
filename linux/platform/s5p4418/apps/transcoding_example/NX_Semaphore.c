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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <NX_Semaphore.h>


//	Create & Initialization
NX_SEMAPHORE *NX_CreateSem( uint32_t initValue, uint32_t maxValue )
{
	//	Create Semaphore
	NX_SEMAPHORE *hSem = malloc( sizeof(NX_SEMAPHORE) );
	if( NULL == hSem )
		return NULL;
	hSem->nValue = initValue;
	hSem->nMaxValue = maxValue;
	if( 0 != pthread_cond_init( &hSem->hCond, NULL ) ){
		free( hSem );
		return NULL;
	}
	if( 0 != pthread_mutex_init( &hSem->hMutex, NULL ) ){
		pthread_cond_destroy( &hSem->hCond );
		free( hSem );
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
		free( hSem );
	}
}

int32_t NX_PendSem( NX_SEMAPHORE *hSem )
{
	int32_t error = 0;
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

int32_t NX_PostSem( NX_SEMAPHORE *hSem )
{
	int32_t error = 0;
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
