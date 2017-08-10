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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>

#include "NX_DvrGsensor.h"

int32_t BMA222_Read( int32_t fd, GSENSOR_VALUE *pValue )
{
	BMA222_GSENSOR_VALUE accValue;

	ioctl( fd, IOCTL_SENSOR_GET_DATA_ACCEL, &accValue);
	
	// return value "mg" - 15.6mg per 1 digit
	pValue->x = (int32_t)((double)accValue.x * 15.6);
	pValue->y = (int32_t)((double)accValue.y * 15.6);
	pValue->z = (int32_t)((double)accValue.z * 15.6);

	//printf("%s(): x( %d mg ), y( %d mg ), z( %d mg )\n", __FUNCTION__, pValue->x, pValue->y, pValue->z );

	return 0;
}

int32_t MMA8653_Read( int32_t fd, GSENSOR_VALUE *pValue )
{
	FILE *fp = fdopen( fd, "rw" );

	rewind( fp );
	// return value "mg" - 15.6mg per 1 digit ( -2G ~ +2G :: 256 digit)
	fscanf( fp, "%d %d %d", &pValue->x, &pValue->y, &pValue->z );

	pValue->x = (int32_t)((double)pValue->x * 15.6);
	pValue->y = (int32_t)((double)pValue->y * 15.6);
	pValue->z = (int32_t)((double)pValue->z * 15.6);

	return 0;
}

GSENSOR_HANDLE DvrGsensorOpen( const char *pDeviceNode )
{
	GSENSOR_HANDLE hGsensor = NULL; 
	int32_t fd = 0;

	if( 0 > (fd = open( pDeviceNode, O_RDONLY ) ) )
	{
		printf("%s(): Device open failed. ( %s )\n", __FUNCTION__, pDeviceNode );
		return NULL;
	}

	hGsensor = (GSENSOR_HANDLE)malloc( sizeof(GSENSOR_HANDLE) );
	memset( hGsensor, 0x00, sizeof(GSENSOR_HANDLE) );

	hGsensor->fd = fd;
	
	// ID Check and Registeration Read function
	//
	//
	if( !strcmp( pDeviceNode, "/dev/accel" ) )
	{
		hGsensor->pReadFunc = &BMA222_Read;
	}
	else if ( !strcmp( pDeviceNode, "/sys/devices/virtual/input/input2/value") )
	{
		hGsensor->pReadFunc = &MMA8653_Read;	
	}

	if( hGsensor->pReadFunc == NULL )
	{
		free( hGsensor );
		return NULL;
	}

	return hGsensor;
}

void DvrGsensorClose( GSENSOR_HANDLE hGsensor )
{
	if( hGsensor )
	{
		close( hGsensor->fd );
		free( hGsensor );
	}
}

int32_t DvrGsensorValue( GSENSOR_HANDLE hGsensor, GSENSOR_VALUE *pValue )
{
	assert( hGsensor );
	hGsensor->pReadFunc( hGsensor->fd, pValue );
	
	return 0;
}

int32_t DvrGsensorAverageValue( GSENSOR_HANDLE hGsensor, GSENSOR_VALUE *pValue )
{
	assert( hGsensor );
	printf("%s(): Not yet.\n", __FUNCTION__);

	return 0;
}