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
#include <stdbool.h>

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/mount.h>

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>

#include <NX_DvrControl.h>
#include <nx_fourcc.h>

#include "NX_DvrCmdQueue.h"

#include "NX_DvrGpsManager.h"
#include "NX_DvrGsensorManager.h"
#include "NX_DvrPowerManager.h"
#include "NX_DvrFileManager.h"
#include "NX_DvrLedCtrl.h"
#include "NX_DvrTools.h"
#include "NX_DvrConfigParser.h"

#include <nx_alloc_mem.h>
#include <nx_graphictools.h>
#include <nx_audio.h>
#include <nx_gpio.h>
#include <nx_dsp.h>

// #define ENABLE_AUDIO
// #define ENABLE_POWER_MANAGER

#define DISPLAY_TICK

#define AUDIO_TYPE_AAC
#define EXTERNAL_MOTION_DETECTION

#define MMCBLOCK				"mmcblk0"
#define DIRECTORY_TOP			"/mnt/mmc"
#define CONFIG_FILE				"/root/dvr.cfg"

static int32_t DvrEventTaskStart( void );
static int32_t DvrEventTaskStop( void );

static int32_t DvrInputEventTaskStart( void );
static int32_t DvrInputEventTaskStop( void );

static int32_t DvrConsoleTaskStart( void );
static int32_t DvrConsoleTaskStop( void );

static int32_t DvrSDCheckerTaskStart( void );
static int32_t DvrSDCheckerTaskStop( void );

typedef struct AppInfo {
	NX_DVR_HANDLE			hDvr;
	GPS_MANAGER_HANDLE		hGpsManager;
	GSENSOR_MANAGER_HANDLE	hGsensorManager;
	POWER_MANAGER_HANDLE	hPowerManager;
	FILE_MANAGER_HANDLE		hNormalFileManager;
	FILE_MANAGER_HANDLE		hEventFileManager;
	FILE_MANAGER_HANDLE		hCaptureFileManager;
	NX_GT_SCALER_HANDLE		hFrontEffect;
	NX_GT_SCALER_HANDLE		hRearEffect;
	NX_AUDIO_HANDLE			hAudio;
	CMD_QUEUE_HANDLE		hCmd;

	CONFIG_HANDLE			hConfig;
	CONFIG_INFO				sConfig;

	int32_t					iMode;
	int32_t					bConsoleDebug;
	int32_t					bEventDebug;
	int32_t					bLoopMsg;

	int32_t					iVersionMajor;
	int32_t					iVersionMinor;
	int32_t					iVersionRevision;

	char					cSDNode[256];

	char					cTopDir[256];
	char					cNormalDir[256];
	char					cEventDir[256];
	char					cCaptureDir[256];

	pthread_mutex_t			hLock;

	pthread_t				hEventTask;
	int32_t					bEventTaskRun;
	
	pthread_t				hConsoleTask;
	int32_t					bConsoleTaskRun;
	
	pthread_t				hSDCheckerTask;
	int32_t					bSDCheckerTaskRun;

	pthread_t				hInputEventTask;
	int32_t					bInputEventTaskRun;
} AppInfo;

static AppInfo gstAppInfo;

////////////////////////////////////////////////////////////////////////////////
//
// Signal Handler
//
static void MicomKillSignal( void )
{
	NX_GPIO_HANDLE hGPIOA18 = NX_GpioInit( GPIOA18 );

	if( hGPIOA18 ) NX_GpioDirection( hGPIOA18, GPIO_DIRECTION_OUT );
	if( hGPIOA18 ) NX_GpioSetValue( hGPIOA18, false );
	if( hGPIOA18 ) NX_GpioDeinit( hGPIOA18 );
}

static void ExitApp( void )
{
	if( gstAppInfo.hAudio )	NX_AudioPlay( gstAppInfo.hAudio, "/root/wav/stop.wav" );

	DvrInputEventTaskStop();
	DvrSDCheckerTaskStop();
	DvrEventTaskStop();
	DvrConsoleTaskStop();

	if( gstAppInfo.hDvr )				NX_DvrStop( gstAppInfo.hDvr );
	if( gstAppInfo.hDvr )				NX_DvrDeinit( gstAppInfo.hDvr );

	if( gstAppInfo.hFrontEffect )		NX_GTSclClose( gstAppInfo.hFrontEffect );
	if( gstAppInfo.hRearEffect )		NX_GTSclClose( gstAppInfo.hRearEffect );

	if( gstAppInfo.hGpsManager ) 		DvrGpsManagerStop( gstAppInfo.hGpsManager );
	if( gstAppInfo.hGpsManager ) 		DvrGpsManagerDeinit( gstAppInfo.hGpsManager );

	if( gstAppInfo.hGsensorManager )	DvrGsensorManagerStop( gstAppInfo.hGsensorManager );
	if( gstAppInfo.hGsensorManager )	DvrGsensorManagerDeinit( gstAppInfo.hGsensorManager );

	if( gstAppInfo.hPowerManager )		DvrPowerManagerStop( gstAppInfo.hPowerManager );
	if( gstAppInfo.hPowerManager )		DvrPowerManagerDeinit( gstAppInfo.hPowerManager );

	if( gstAppInfo.hNormalFileManager )	DvrFileManagerStop( gstAppInfo.hNormalFileManager );
	if( gstAppInfo.hEventFileManager )	DvrFileManagerStop( gstAppInfo.hEventFileManager );
	if( gstAppInfo.hCaptureFileManager )DvrFileManagerStop( gstAppInfo.hCaptureFileManager );

	if( gstAppInfo.hNormalFileManager )	DvrFileManagerDeinit( gstAppInfo.hNormalFileManager );
	if( gstAppInfo.hEventFileManager )	DvrFileManagerDeinit( gstAppInfo.hEventFileManager );
	if( gstAppInfo.hCaptureFileManager )DvrFileManagerDeinit( gstAppInfo.hCaptureFileManager );

	gstAppInfo.bLoopMsg = false;
	DvrCmdQueueDeinit( gstAppInfo.hCmd );
	pthread_mutex_destroy( &gstAppInfo.hLock );

	printf("%s(): umount mmc.\n", __FUNCTION__);
	int32_t iPendCount = 0;

	while( 1 )
	{
		iPendCount++;
		sync();
		usleep(100000);
		if( !umount("/mnt/mmc") ) break;
		if( iPendCount > 10) {
			printf("Fail, umount..\n");
			break;
		}
		printf("[%d] Retry umount..\n", iPendCount);
	}

	if( gstAppInfo.hAudio )	NX_AudioStop( gstAppInfo.hAudio, true );
	if( gstAppInfo.hAudio )	NX_AudioDeinit( gstAppInfo.hAudio );
}

static void signal_handler(int sig)
{
	printf("Aborted by signal %s (%d)...\n", (char*)strsignal(sig), sig);

	switch(sig)
	{   
		case SIGINT :
			printf("SIGINT..\n");   break;
		case SIGTERM :
			printf("SIGTERM..\n");  break;
		case SIGABRT :
			printf("SIGABRT..\n");  break;
		default :
			break;
	}   

	ExitApp();
	exit(EXIT_FAILURE);
}

static void register_signal(void)
{
	signal( SIGINT, signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGABRT, signal_handler );
}

////////////////////////////////////////////////////////////////////////////////
//
// Callback function
//
int32_t cbGetNormalFileName( uint8_t *buf, uint32_t bufSize )
{
	time_t eTime;
	struct tm *eTm;
	
	time( &eTime);
	eTm = localtime( &eTime );

	if( gstAppInfo.sConfig.iContainer ) {
		sprintf((char*)buf, "%s/normal_%04d%02d%02d_%02d%02d%02d.ts", gstAppInfo.cNormalDir,
			eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec);
	}
	else {
		sprintf((char*)buf, "%s/normal_%04d%02d%02d_%02d%02d%02d.mp4", gstAppInfo.cNormalDir,
			eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec);
	}

	return 0;
}

int32_t cbGetEventFileName( uint8_t *buf, uint32_t bufSize )
{
	time_t eTime;
	struct tm *eTm;
	
	time( &eTime);
	eTm = localtime( &eTime );

	if( gstAppInfo.sConfig.iContainer ) {
		sprintf((char*)buf, "%s/event_%04d%02d%02d_%02d%02d%02d.ts", gstAppInfo.cEventDir,
			eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec);
	}
	else {
		sprintf((char*)buf, "%s/event_%04d%02d%02d_%02d%02d%02d.mp4", gstAppInfo.cEventDir,
			eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec);
	}

	return 0;
}

int32_t cbGetCaptureFileName( uint8_t *buf, uint32_t bufSize )
{
	time_t eTime;
	struct tm *eTm;
	
	time( &eTime);
	eTm = localtime( &eTime );

	sprintf((char*)buf, "%s/capture_%04d%02d%02d_%02d%02d%02d.jpg", gstAppInfo.cCaptureDir,
		eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec);

	return 0;
}

int32_t cbGetFrontTextOverlay( uint8_t *buf, uint32_t *bufSize, uint32_t *x, uint32_t *y)
{
	struct nmea_gprmc	gpsData;
	GSENSOR_VALUE		gsensorData;
	int32_t				powerData = 0, tempData = 0;
	static uint64_t		frameCounter = 0;
	
	memset( &gpsData, 0x00, sizeof( struct nmea_gprmc ) );
	memset( &gsensorData, 0x00, sizeof( GSENSOR_VALUE) );

	if( gstAppInfo.hGpsManager )		DvrGpsGetData( gstAppInfo.hGpsManager, &gpsData );
	if( gstAppInfo.hGsensorManager )	DvrGsensorGetData( gstAppInfo.hGsensorManager, &gsensorData );
	if( gstAppInfo.hPowerManager )		DvrPowerGetData( gstAppInfo.hPowerManager, &powerData, &tempData );

	pthread_mutex_lock( &gstAppInfo.hLock );
	int32_t iMode = gstAppInfo.iMode;
	pthread_mutex_unlock( &gstAppInfo.hLock );

#ifdef DISPLAY_TICK
	static uint64_t uPrvTick = 0;
	if( !uPrvTick ) GetSystemTickCount();
	uint64_t uCurTick = GetSystemTickCount();
	
	sprintf((char*)buf, "[CAM#0 %s] [%08lld] [%08lld/%04lld] [lat = %+010.06f, long = %+011.06f, speed = %03dKm, x = %+05dmg, y = %+05dmg, z = %+05dmg, pwr = %+05.02fV]",
		(iMode == DVR_MODE_NORMAL) ? "NOR" : (iMode == DVR_MODE_EVENT) ? "EVT" : "MOT",
		++frameCounter, uCurTick, uCurTick - uPrvTick,
		gpsData.latitude, gpsData.longitude, (int)(gpsData.ground_speed * 1.852),
		gsensorData.x, gsensorData.y, gsensorData.z,
		(double)(powerData / 1000.)
	);

	uPrvTick = uCurTick;
#else
	time_t eTime;
	struct tm *eTm;
	time( &eTime);
	eTm = localtime( &eTime );

	sprintf((char*)buf, "[CAM#0 %s] [%08lld] [%4d-%02d-%02d %02d:%02d:%02d] [lat = %+010.06f, long = %+011.06f, speed = %03dKm, x = %+05dmg, y = %+05dmg, z = %+05dmg, pwr = %+05.02fV]",
		(iMode == DVR_MODE_NORMAL) ? "NOR" : (iMode == DVR_MODE_EVENT) ? "EVT" : "MOT",
		++frameCounter,
		eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec,
		gpsData.latitude, gpsData.longitude, (int)(gpsData.ground_speed * 1.852),
		gsensorData.x, gsensorData.y, gsensorData.z,
		(double)(powerData / 1000.)
	);
#endif

	*bufSize = strlen((char*)buf);
	*x = *y = 1;

	return 0;
}

int32_t cbGetRearTextOverlay( uint8_t *buf, uint32_t *bufSize, uint32_t *x, uint32_t *y)
{
	struct nmea_gprmc	gpsData;
	GSENSOR_VALUE		gsensorData;
	int32_t				powerData = 0, tempData = 0;
	static uint64_t		frameCounter = 0;

	memset( &gpsData, 0x00, sizeof( struct nmea_gprmc ) );
	memset( &gsensorData, 0x00, sizeof( GSENSOR_VALUE) );

	if( gstAppInfo.hGpsManager ) 	DvrGpsGetData( gstAppInfo.hGpsManager, &gpsData );
	if( gstAppInfo.hGsensorManager )DvrGsensorGetData( gstAppInfo.hGsensorManager, &gsensorData);
	if( gstAppInfo.hPowerManager )	DvrPowerGetData( gstAppInfo.hPowerManager, &powerData, &tempData );

	pthread_mutex_lock( &gstAppInfo.hLock );
	int32_t iMode = gstAppInfo.iMode;
	pthread_mutex_unlock( &gstAppInfo.hLock );

#ifdef DISPLAY_TICK
	static uint64_t uPrvTick = 0;
	if( !uPrvTick ) GetSystemTickCount();
	uint64_t uCurTick = GetSystemTickCount();
	
	sprintf((char*)buf, "[CAM#1 %s] [%08lld] [%08lld/%04lld] [lat = %+010.06f, long = %+011.06f, speed = %03dKm, x = %+05dmg, y = %+05dmg, z = %+05dmg, pwr = %+05.02fV]",
		(iMode == DVR_MODE_NORMAL) ? "NOR" : (iMode == DVR_MODE_EVENT) ? "EVT" : "MOT",
		++frameCounter, uCurTick, uCurTick - uPrvTick,
		gpsData.latitude, gpsData.longitude, (int)(gpsData.ground_speed * 1.852),
		gsensorData.x, gsensorData.y, gsensorData.z,
		(double)(powerData / 1000.)
	);

	uPrvTick = uCurTick;
#else
	time_t eTime;
	struct tm *eTm;
	time( &eTime);
	eTm = localtime( &eTime );

	sprintf((char*)buf, "[CAM#1 %s] [%08lld] [%4d-%02d-%02d %02d:%02d:%02d] [lat = %+010.06f, long = %+011.06f, speed = %03dKm, x = %+05dmg, y = %+05dmg, z = %+05dmg, pwr = %+05.02fV]",
		(iMode == DVR_MODE_NORMAL) ? "NOR" : (iMode == DVR_MODE_EVENT) ? "EVT" : "MOT",
		++frameCounter,
		eTm->tm_year + 1900, eTm->tm_mon + 1, eTm->tm_mday, eTm->tm_hour, eTm->tm_min, eTm->tm_sec,
		gpsData.latitude, gpsData.longitude, (int)(gpsData.ground_speed * 1.852),
		gsensorData.x, gsensorData.y, gsensorData.z,
		(double)(powerData / 1000.)
	);
#endif

	*bufSize = strlen((char*)buf);
	*x = *y = 1;

	return 0;
}

int32_t cbUserData( uint8_t *buf, uint32_t bufSize)
{
	struct nmea_gprmc gpsData;
	GSENSOR_VALUE gsensorData;

	memset( &gpsData, 0x00, sizeof( struct nmea_gprmc ) );
	memset( &gsensorData, 0x00, sizeof( GSENSOR_VALUE) );

	if( gstAppInfo.hGpsManager )		DvrGpsGetData( gstAppInfo.hGpsManager, &gpsData );
	if( gstAppInfo.hGsensorManager )	DvrGsensorGetData( gstAppInfo.hGsensorManager, &gsensorData );

	sprintf((char*)buf, "%+010.06f %+011.06f %03d %+05d %+05d %+05d",
		gpsData.latitude, gpsData.longitude, (int)(gpsData.ground_speed * 1.852),
		gsensorData.x, gsensorData.y, gsensorData.z
	);
	
	return strlen((char*)buf);
}

int32_t cbNotifier( uint32_t eventCode, uint8_t *pEventData, uint32_t dataSize )
{
	switch( eventCode )
	{
	case DVR_NOTIFY_NORALWIRTHING_DONE :
		if( pEventData && dataSize > 0 )
		{
			printf("[%s] : Normal file writing done. ( %s )\n", __FUNCTION__, pEventData);
			DvrFileManagerPush( gstAppInfo.hNormalFileManager, (char*)pEventData );
		}
		break;
	
	case DVR_NOTIFY_EVENTWRITING_DONE :
		if( pEventData && dataSize > 0 )
		{
			printf("[%s] : Event file writing done. ( %s )\n", __FUNCTION__, pEventData);
			pthread_mutex_lock( &gstAppInfo.hLock );
			if( gstAppInfo.iMode != DVR_MODE_MOTION )
			{
				gstAppInfo.iMode = DVR_MODE_NORMAL;
				pthread_mutex_unlock( &gstAppInfo.hLock );
				
				DvrLedEventStop();
				if( gstAppInfo.hGsensorManager ) 
					DvrGsensorManagerSetStatus( gstAppInfo.hGsensorManager, DVR_MODE_NORMAL );
			}
			else
			{
				pthread_mutex_unlock( &gstAppInfo.hLock );
				DvrLedEventStop();
			}
			DvrFileManagerPush( gstAppInfo.hEventFileManager, (char*)pEventData );
		}
		break;
	
	case DVR_NOTIFY_JPEGWRITING_DONE :
		if( pEventData && dataSize > 0 )
		{
			printf( "[%s] : Jpeg file writing done. ( %s )\n", __FUNCTION__, pEventData );
			DvrFileManagerPush( gstAppInfo.hCaptureFileManager, (char*)pEventData );
		}
		break;
	
	case DVR_NOTIFY_MOTION :
		DvrLedEventStart();
		if( gstAppInfo.hDvr )	NX_DvrEvent( gstAppInfo.hDvr );
		break;
	
	case DVR_NOTIFY_ERR_VIDEO_INPUT :
		break;
	
	case DVR_NOTIFY_ERR_VIDEO_ENCODE :
		break;
	
	case DVR_NOTIFY_ERR_OPEN_FAIL :
		if( pEventData && dataSize > 0 ) {
			printf("[%s] : File open failed. ( %s )\n", __FUNCTION__, pEventData);
		}
		break;

	case DVR_NOTIFY_ERR_WRITE :
		if( pEventData && dataSize > 0 ) {	
			printf("[%s] : File writing failed. ( %s )\n", __FUNCTION__, pEventData);
		}
		break;
	}
	
	return 0;
}

int32_t cbFrontImageEffect( NX_VID_MEMORY_INFO *pInBuf, NX_VID_MEMORY_INFO *pOutBuf )
{
	if( gstAppInfo.hFrontEffect ) NX_GTSclDoScale( gstAppInfo.hFrontEffect, pInBuf, pOutBuf);
	return 0;
}

int32_t cbRearImageEffect( NX_VID_MEMORY_INFO *pInBuf, NX_VID_MEMORY_INFO *pOutBuf )
{
	if( gstAppInfo.hRearEffect ) NX_GTSclDoScale( gstAppInfo.hRearEffect, pInBuf, pOutBuf);
	return 0;
}

#ifdef EXTERNAL_MOTION_DETECTION
int32_t cbMotionDetect( NX_VID_MEMORY_INFO *pPrevMemory, NX_VID_MEMORY_INFO *pCurMemory )
{
	int32_t i, j;

	unsigned char *pPrevAddr	= (unsigned char*)pPrevMemory->luVirAddr;
	unsigned char *pCurAddr		= (unsigned char*)pCurMemory->luVirAddr;

	unsigned char prevValue	= 0;
	unsigned char curValue	= 0;

	unsigned char diffValue	= 0;
	
	int32_t		diffCnt		= 0;

	for( i = 0; i < pPrevMemory->imgHeight; i += 16 )
	{
		for( j = 0; j < pPrevMemory->imgWidth; j += 16 )
		{
			prevValue = *(pPrevAddr + j + (pPrevMemory->luStride * i));
			curValue  = *(pCurAddr  + j + (pCurMemory->luStride  * i));
			
			if(prevValue > curValue )
				diffValue = prevValue - curValue;
			else
				diffValue = curValue - prevValue;

			if( diffValue > 100 ) diffCnt++;
		}
	}

	if( diffCnt > 100 )	{
		return true;
	}
	
	return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Test Thread
//
void *DvrEventTaskThread( void *arg )
{
	int32_t i = 0, bLoop;
	CMD_MESSAGE	cmd;

	memset( &cmd, 0x00, sizeof(CMD_MESSAGE) );
	cmd.cmdType = CMD_TYPE_EVENT;

	while( gstAppInfo.bEventTaskRun )
	{
		for( i = 0; i < 100; i++)
		{
			usleep(100000);
			
			pthread_mutex_lock( &gstAppInfo.hLock );
			bLoop = gstAppInfo.bEventTaskRun;
			pthread_mutex_unlock( &gstAppInfo.hLock );
			
			if( !bLoop ) break;
		}

		if( bLoop )
			DvrCmdQueuePush( gstAppInfo.hCmd, &cmd );
	}

	return (void*)0xDEADDEAD;
}

static int32_t DvrEventTaskStart( void )
{
	if( !gstAppInfo.bEventDebug )
		return 0;

	if( gstAppInfo.bEventTaskRun ) {
		printf("%s(): Fail, Already running.\n", __FUNCTION__);
		return -1;
	}

	gstAppInfo.bEventTaskRun = true;
	if( 0 > pthread_create( &gstAppInfo.hEventTask, NULL, &DvrEventTaskThread, NULL) ) {
		printf("%s(): Fail, Create Thread.\n", __FUNCTION__ );
		return -1;
	}

	printf("%s(): Activation Test Event Task.\n", __FUNCTION__);
	
	return 0;
}

static int32_t DvrEventTaskStop( void )
{
	if( !gstAppInfo.bEventDebug )
		return 0;

	if( !gstAppInfo.bEventTaskRun )
		return -1;

	gstAppInfo.bEventTaskRun = false;
	pthread_join( gstAppInfo.hEventTask, NULL );
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// debug shell
//
#define	SHELL_MAX_ARG	32
#define	SHELL_MAX_STR	1024

static int32_t GetArgument( char *pSrc, char arg[][SHELL_MAX_STR] )
{
	int32_t i, j;

	// Reset all arguments
	for( i = 0; i < SHELL_MAX_ARG; i++ )
	{
		arg[i][0] = 0x00;
	}

	for( i = 0; i < SHELL_MAX_ARG; i++ )
	{
		// Remove space char
		while( *pSrc == ' ' )
			pSrc++;
		
		// check end of string.
		if( *pSrc == 0 || *pSrc == '\n' )
			break;

		j = 0;
		while( (*pSrc != ' ') && (*pSrc != 0) && *pSrc != '\n' )
		{
			arg[i][j] = *pSrc++;
			j++;
			if( j > (SHELL_MAX_STR-1) )
				j = SHELL_MAX_STR-1;
		}
		arg[i][j] = 0;
	}
	return i;
}

void shell_usage( void )
{
	printf("\n");
	printf("+----------------------------------------------------------------+\n");
	printf("|                 Nexell DVR Test Application                    |\n");
	printf("+----------------------------------------------------------------+\n");
	printf("| help                        : help                             |\n");
	printf("| event                       : occur event                      |\n");
	printf("| capture [channel]           : jpeg capture                     |\n");
	printf("| dsp [enable]                : display enable                   |\n");
	printf("| crop [L] [T] [L] [B]        : source image display crop        |\n");
	printf("| pos [L] [T] [L] [B]         : change display position          |\n");
	printf("| lcd [channel]               : lcd preview channel              |\n");
	printf("| hdmi [channel]              : hdmi preview channel             |\n");
	printf("| dbg [debug_level]           : change debug level( 0 - 5)       |\n");
	printf("| exit                        : exit application                 |\n");
	printf("+----------------------------------------------------------------+\n");
}

void *DvrConsoleTask( void *arg )
{
	static char cmdString[SHELL_MAX_ARG * SHELL_MAX_STR];
	static char cmd[SHELL_MAX_ARG][SHELL_MAX_STR];
	int32_t cmdCnt;

    fd_set readfds;
    int32_t state;
	struct timeval tv;
	
	CMD_MESSAGE cmdMsg; 

	shell_usage();

	printf(" > ");

	while( gstAppInfo.bConsoleTaskRun )
	{
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		
		state = select( STDIN_FILENO + 1, &readfds, NULL, NULL, &tv );

		if( state < 0 )
		{
			printf("%s(): Select Error!!!\n", __FUNCTION__);
			break;
		}
		else if( state == 0 )
		{
		}
		else
		{
			printf(" > ");

			memset( cmdString, 0x00, sizeof( cmdString ) );
			fgets(cmdString, sizeof(cmdString), stdin);
			if( strlen(cmdString) != 0 ) cmdString[strlen(cmdString) - 1] = 0x00;

			cmdCnt = GetArgument( cmdString, cmd );

			if( cmdCnt == 0 )
				continue;

			if( !strcasecmp( cmd[0], "exit") ) {
				printf("Exit.\n");
				ExitApp();
				break;
			}
			else if( !strcasecmp( cmd[0], "help" ) ) {
				shell_usage();
			}
			else if( !strcasecmp( cmd[0], "event") ) {
				printf("Event.\n");
				memset( &cmdMsg, 0x00, sizeof(CMD_MESSAGE) );
				cmdMsg.cmdType = CMD_TYPE_EVENT;
				DvrCmdQueuePush( gstAppInfo.hCmd, &cmdMsg );
			}
			else if( !strcasecmp( cmd[0], "capture") ) {
				if( !strcasecmp( cmd[1], "0" ) || !strcasecmp( cmd[1], "1" ) ) {
					int32_t ch = atoi(cmd[1]);
					printf("Capture. (ch = %d)\n", ch );
					cmdMsg.cmdType = CMD_TYPE_CAPTURE;
					cmdMsg.cmdData = ch;
					DvrCmdQueuePush( gstAppInfo.hCmd, &cmdMsg );
				}
				else {
					printf("unknown argument. ( 0 or 1 )\n");
				}
			}
			else if( !strcasecmp( cmd[0], "dsp") ) {
				if( !strcasecmp( cmd[1], "0" ) || !strcasecmp( cmd[1], "1" ) ) {
					NX_DVR_DISPLAY_CONFIG dspConf;
					memset( &dspConf, 0x00, sizeof(dspConf) );
					dspConf.bEnable	= atoi(cmd[1]);

					NX_DvrSetDisplay( gstAppInfo.hDvr, &dspConf );
				}
				else {
					printf("unknown argument. ( 0 or 1 )\n");	
				}
			}
			else if( !strcasecmp( cmd[0], "crop") ) {
				if( cmdCnt == 5 ) {
					NX_DVR_DISPLAY_CONFIG dspConf;
					memset( &dspConf, 0x00, sizeof(dspConf) );
					dspConf.bEnable				= true;
					dspConf.cropRect.nLeft 		= atoi(cmd[1]);
					dspConf.cropRect.nTop		= atoi(cmd[2]);
					dspConf.cropRect.nRight 	= atoi(cmd[3]);
					dspConf.cropRect.nBottom	= atoi(cmd[4]);

					NX_DvrSetDisplay( gstAppInfo.hDvr, &dspConf );
				}
				else {
					printf("unknwon argument.\n");
				}
			}
			else if( !strcasecmp( cmd[0], "pos") ) {
				if( cmdCnt == 5 ) {
					NX_DVR_DISPLAY_CONFIG dspConf;
					memset( &dspConf, 0x00, sizeof(dspConf) );
					dspConf.bEnable				= true;
					dspConf.dspRect.nLeft 		= atoi(cmd[1]);
					dspConf.dspRect.nTop		= atoi(cmd[2]);
					dspConf.dspRect.nRight 		= atoi(cmd[3]);
					dspConf.dspRect.nBottom		= atoi(cmd[4]);

					NX_DvrSetDisplay( gstAppInfo.hDvr, &dspConf );
				}
				else {
					printf("unknwon argument.\n");
				}
			}
			else if( !strcasecmp( cmd[0], "lcd") ) {
				if( !strcasecmp( cmd[1], "0" ) || !strcasecmp( cmd[1], "1" ) ) {
					int32_t ch = atoi(cmd[1]);
					printf("LCD Preview. (ch = %d)\n", ch );
					cmdMsg.cmdType = CMD_TYPE_CHG_LCD;
					cmdMsg.cmdData = ch;
					DvrCmdQueuePush( gstAppInfo.hCmd, &cmdMsg );
				}
				else {
					printf("unknown argument. ( 0 or 1 )\n");
				}				
			}
			else if( !strcasecmp( cmd[0], "hdmi") ) {
				if( !strcasecmp( cmd[1], "0" ) || !strcasecmp( cmd[1], "1" ) ) {
					int32_t ch = atoi(cmd[1]);
					printf("HDMI Preview. (ch = %d)\n", ch );
					cmdMsg.cmdType = CMD_TYPE_CHG_HDMI;
					cmdMsg.cmdData = ch;
					DvrCmdQueuePush( gstAppInfo.hCmd, &cmdMsg );
				}
				else {
					printf("unknown argument. ( 0 or 1 )\n");
				}				
			}
			else if( !strcasecmp( cmd[0], "dbg") ) {
				if( !strcasecmp( cmd[1], "0" ) || 
					!strcasecmp( cmd[1], "1" ) ||
					!strcasecmp( cmd[1], "2" ) ||
					!strcasecmp( cmd[1], "3" ) ||
					!strcasecmp( cmd[1], "4" ) ||
					!strcasecmp( cmd[1], "5" ) ) {
					int32_t level = atoi(cmd[1]);

					printf("Change Debug Level. ( %s )\n", 
						level == 0 ? "NX_DBG_DISABLE" :
						level == 1 ? "NX_DBG_ERR" :
						level == 2 ? "NX_DBG_WARN" :
						level == 3 ? "NX_DBG_INFO" :
						level == 4 ? "NX_DBG_DEBUG" : "NX_DBG_VBS");
					
					NX_DvrChgDebugLevel( gstAppInfo.hDvr, level );
				}
				else {
					printf("unknown argument. (0 - 5)\n");
				}
			}			
			else {
				printf("unknown command. (%s)\n", cmdString );
			}
		}
	}

	FD_CLR( STDIN_FILENO, &readfds );
	return (void*)0xDEADDEAD;
}

static int32_t DvrConsoleTaskStart( void )
{
	if( !gstAppInfo.bConsoleDebug )
		return 0;

	if( gstAppInfo.bConsoleTaskRun )
	{
		printf("%s(): Fail, Already running.\n", __FUNCTION__);
		return -1;
	}

	gstAppInfo.bConsoleTaskRun = true;
	if( 0 > pthread_create( &gstAppInfo.hConsoleTask, NULL, &DvrConsoleTask, NULL) )
	{
		printf("%s(: Fail, Create Thread.\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

static int32_t DvrConsoleTaskStop( void )
{
	if( !gstAppInfo.bConsoleDebug )
		return 0;

	if( !gstAppInfo.bConsoleTaskRun )
		return -1;

	gstAppInfo.bConsoleTaskRun = false;
	fputs("exit", stdin);
	pthread_join( gstAppInfo.hConsoleTask, NULL );

	return 0;
}

static void Usage( void )
{
	printf(" Nexell Blackbox Encoding Test Application.\n\n");
	printf(" Usage :\n");
	printf("   -h                     : Show usage.\n");
	printf("   -b                     : No command line.\n");
	printf("   -e                     : Test event task. ( 100sec cycle )\n");

	printf("\n");
}

////////////////////////////////////////////////////////////
//
// SD Card Checker
//
void *DvrSDCheckerTask( void *arg )
{
	CMD_MESSAGE	cmd;

	char buf[4096], sdNodekeyword[64];
	int32_t len, socketFd = 0;
	struct sockaddr_nl sockAddr;
	struct iovec	iov	= { buf, sizeof(buf) };
	struct msghdr	msg	= { &sockAddr, sizeof(sockAddr), &iov, 1, NULL, 0, 0 };

    fd_set readfds;
    int32_t state;
	struct timeval tv;

	memset( &sockAddr, 0, sizeof(sockAddr) );
	sockAddr.nl_family = AF_NETLINK;
	sockAddr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

	socketFd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);

	if( socketFd < 0 ) {
		printf("%s(): socket error.\n", __FUNCTION__);
		goto ERROR;
	}
	if( bind(socketFd, (struct sockaddr*)&sockAddr, sizeof(sockAddr)) ) {
		printf("%s(): bind error.\n", __FUNCTION__);
		close( socketFd );
		goto ERROR;
	}

	// first node checker
	sprintf( sdNodekeyword, "%sp1", MMCBLOCK );
	sprintf( (char*)gstAppInfo.cSDNode, "/dev/%s", sdNodekeyword );
	if( !access( (char*)gstAppInfo.cSDNode, F_OK) )
	{
		printf("%s(): Insert SD Card. ( node : %s)\n", __FUNCTION__, gstAppInfo.cSDNode);
		memset( &cmd, 0x00, sizeof(CMD_MESSAGE) );
		cmd.cmdType = CMD_TYPE_SD_INSERT;
		if( gstAppInfo.hCmd )	DvrCmdQueuePush( gstAppInfo.hCmd, &cmd );
	}

	while( gstAppInfo.bSDCheckerTaskRun )
	{
		FD_ZERO(&readfds);
		FD_SET(socketFd, &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		state = select( socketFd+1, &readfds, NULL, NULL, &tv );

		if( state < 0 )
		{
			printf("%s(): Select Error!!!\n", __FUNCTION__);
			break;
		}
		else if( state == 0 )
		{
			//printf("%s(): System Message Wait Timeout\n", __FUNCTION__);
		}
		else
		{
			memset( buf, 0x00, sizeof(buf) );
			len = recvmsg( socketFd, &msg, 0 );
			if( len < 0 ) {
				printf("%s(): recvmsg error.\n", __FUNCTION__);
				break;
			}

			if( strstr(buf, "add@") && strstr(buf, sdNodekeyword) )
			{
				printf("%s(): Insert SD Card. ( node : %s)\n", __FUNCTION__, gstAppInfo.cSDNode);
				memset( &cmd, 0x00, sizeof(CMD_MESSAGE) );
				cmd.cmdType = CMD_TYPE_SD_INSERT;
				if( gstAppInfo.hCmd )	DvrCmdQueuePush( gstAppInfo.hCmd, &cmd );
			}

			if( strstr(buf, "remove@") && strstr(buf, "mmcblk0p1") )
			{
				printf("%s(): Remove SD Card. ( node : %s)\n", __FUNCTION__, gstAppInfo.cSDNode);
				memset( &cmd, 0x00, sizeof(CMD_MESSAGE) );
				cmd.cmdType = CMD_TYPE_SD_REMOVE;
				if( gstAppInfo.hCmd )	DvrCmdQueuePush( gstAppInfo.hCmd, &cmd );
			}
		}
	}
	
	FD_CLR(socketFd, &readfds);
	close( socketFd );

ERROR:
	gstAppInfo.bSDCheckerTaskRun = false;
	return (void*)0xDEADDEAD;
}

static int32_t DvrSDCheckerTaskStart( void )
{
	if( gstAppInfo.bSDCheckerTaskRun )
	{
		printf("%s(): Fail, Already running.\n", __FUNCTION__);
		return -1;
	}

	gstAppInfo.bSDCheckerTaskRun = true;
	if( 0 > pthread_create( &gstAppInfo.hSDCheckerTask, NULL, &DvrSDCheckerTask, NULL) )
	{
		printf("%s(: Fail, Create Thread.\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

static int32_t DvrSDCheckerTaskStop( void )
{
	if( !gstAppInfo.bSDCheckerTaskRun )
	{
		printf("%s(): Fail, Already stopppng.\n", __FUNCTION__);
		return -1;
	}

	gstAppInfo.bSDCheckerTaskRun = false;

	pthread_join( gstAppInfo.hSDCheckerTask, NULL );
	return 0;
}

////////////////////////////////////////////////////////////
//
// Input Event
//
#include <linux/input.h>

#define EVENT_BUF_NUM 	1
#define	EVENT_DEV_NAME	"/dev/input/event0"

#define KEY_CAPTURE		234

static int parse_ev_key(struct input_event * evt)
{
	uint16_t code = evt->code;
	uint32_t val  = evt->value;

	CMD_MESSAGE cmd;
	//printf("KEY  	=%03d, val=%s\n", code, val?"PRESS":"RELEASE");

	if( (code == KEY_CAPTURE) && !val)
	{
		memset( &cmd, 0x00, sizeof(CMD_MESSAGE) );
		cmd.cmdType = CMD_TYPE_CAPTURE;
		DvrCmdQueuePush( gstAppInfo.hCmd, &cmd );
	}

	return 0;
}

void *DvrInputEventTask( void* arg )
{
	int    evt_fd;
    char   evt_dev[256] = EVENT_DEV_NAME;

    struct input_event evt_buf;
    size_t length;

    fd_set fdset;
	struct timeval zero;

	/* event wait time */
   	zero.tv_sec  = 1000;
   	zero.tv_usec = 0;

	evt_fd = open(evt_dev, O_RDONLY);
    if( 0 > evt_fd )
    {
		printf("%s: Device open failed!\n", __FUNCTION__);
        goto ERROR;
    }

    while( gstAppInfo.bInputEventTaskRun )
    {
		FD_ZERO(&fdset);
   		FD_SET(evt_fd, &fdset);

   		zero.tv_sec  = 2;
        if (select(evt_fd + 1, &fdset, NULL, NULL, &zero) > 0) {

            if (FD_ISSET(evt_fd, &fdset)) {

                length = read(evt_fd, &evt_buf, sizeof(struct input_event) );
                if (0 > (int)length) {
                    printf("%s(): read failed.\n", __FUNCTION__);
                    close(evt_fd);
                    goto ERROR;
                }

				parse_ev_key( &evt_buf );
            }
        }
		else
		{
		}
    }

   	FD_CLR( evt_fd, &fdset );

    close( evt_fd );

ERROR:
    return (void*)0xDEADDEAD;
}

static int32_t DvrInputEventTaskStart( void )
{
	if( gstAppInfo.bInputEventTaskRun )
	{
		printf("%s(): Fail, Already running.\n", __FUNCTION__);
		return -1;
	}

	gstAppInfo.bInputEventTaskRun = true;
	if( 0 > pthread_create( &gstAppInfo.hInputEventTask, NULL, &DvrInputEventTask, NULL) )
	{
		printf("%s(: Fail, Create Thread.\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

static int32_t DvrInputEventTaskStop( void )
{
	if( !gstAppInfo.bInputEventTaskRun )
	{
		printf("%s(): Fail, Already stopppng.\n", __FUNCTION__);
		return -1;
	}

	gstAppInfo.bInputEventTaskRun = false;

	pthread_join( gstAppInfo.hInputEventTask, NULL );
	return 0;
}

void DvrPrintConfig( NX_DVR_MEDIA_CONFIG *pMediaConfig )
{
	//      0         1         2         3         4         5         6         7         8       
	//      0123456789012345678901234567890123456789012345678901234567890123456789012345678901
	printf("##################################################################################\n");
	printf(" Confiugration value\n");
	printf("   API Version   : %d.%d.%d        \n", gstAppInfo.iVersionMajor , gstAppInfo.iVersionMinor, gstAppInfo.iVersionRevision);
	printf("   Storage       : %s              \n", gstAppInfo.cTopDir );
	printf("   Video Channel : %d channel      \n", gstAppInfo.sConfig.iChannel );
	printf("   Audio         : %s              \n", gstAppInfo.sConfig.bAudio ? "enable" : "disable");
	printf("   UserData      : %s              \n", gstAppInfo.sConfig.bUserData ? "enable" : "disable" );
	printf("   Network       : %s              \n", (gstAppInfo.sConfig.iNetwork == DVR_NETWORK_HLS) ? "HLS enable" : (gstAppInfo.sConfig.iNetwork == DVR_NETWORK_RTP) ? "RTP enable" : "disable" );
	printf("   Motion Detect : %s              \n", gstAppInfo.sConfig.bMotion ? "enable" : "disable" );
	
	{
		printf("   Cam0 Resol    : %d x %d, %dfps, %dbps\n",
			pMediaConfig->videoConfig[0].nSrcWidth,
			pMediaConfig->videoConfig[0].nSrcHeight,
			pMediaConfig->videoConfig[0].nFps,
			pMediaConfig->videoConfig[0].nBitrate );
	}
	
	if( pMediaConfig->nVideoChannel == 2 )
	{
		printf("   Cam1 Resol    : %d x %d, %dfps, %dbps\n",
			pMediaConfig->videoConfig[1].nSrcWidth,
			pMediaConfig->videoConfig[1].nSrcHeight,
			pMediaConfig->videoConfig[1].nFps,
			pMediaConfig->videoConfig[1].nBitrate );
	}
	printf("##################################################################################\n");
}

int main( int32_t argc, char *argv[] )
{
	int32_t opt;
	uint64_t interval;

	NX_DVR_MEDIA_CONFIG		mediaConfig;
	NX_DVR_RECORD_CONFIG	recordConfig;
	NX_DVR_DISPLAY_CONFIG	displayConfig;

	CMD_MESSAGE cmd; 

	// Core Dump Debug
	// echo "1" > /proc/sys/kernel/core_uses_pid;echo "/mnt/mmc/core.%e" > /proc/sys/kernel/core_pattern;ulimit -c 99999999
	// system("echo \"1\" > /proc/sys/kernel/core_uses_pid");
	// system("echo \"/mnt/mmc/core.%e\" > /proc/sys/kernel/core_pattern");
	// system("ulimit -c 99999999");
	
	memset( &gstAppInfo, 0x00, sizeof(AppInfo) );
	gstAppInfo.hConfig = DvrConfigParserInit( CONFIG_FILE );
	if( !gstAppInfo.hConfig ) {
		printf("Invalid Configuration File.\n");
		goto END;
	}

	DvrConfigParser( gstAppInfo.hConfig, &gstAppInfo.sConfig );
	DvrConfigParserDeinit( gstAppInfo.hConfig );
	
	gstAppInfo.bConsoleDebug = true;

	pthread_mutex_init( &gstAppInfo.hLock, NULL );

	memset( gstAppInfo.cTopDir, 0x00, sizeof(gstAppInfo.cTopDir) );
	sprintf( (char*)gstAppInfo.cTopDir, "%s", DIRECTORY_TOP );

	while( -1 != (opt = getopt(argc, argv, "hbe"))  )
	{
		switch(opt)
		{
		case 'h':
			Usage();
			goto END;

		case 'b':
			gstAppInfo.bConsoleDebug = false;
			break;

		case 'e':
			gstAppInfo.bConsoleDebug = false;
			gstAppInfo.bEventDebug = true;
			break;
		}
	}

	mediaConfig.nVideoChannel				= gstAppInfo.sConfig.iChannel;
	mediaConfig.bEnableAudio				= gstAppInfo.sConfig.bAudio;
	mediaConfig.bEnableUserData 			= gstAppInfo.sConfig.bUserData;
	mediaConfig.nContainer					= gstAppInfo.sConfig.iContainer;

	mediaConfig.videoConfig[0].nPort 		= gstAppInfo.sConfig.cam0Info.iPort;
	mediaConfig.videoConfig[0].nSrcWidth	= gstAppInfo.sConfig.cam0Info.iWidth;
	mediaConfig.videoConfig[0].nSrcHeight	= gstAppInfo.sConfig.cam0Info.iHeight;
	mediaConfig.videoConfig[0].nFps			= gstAppInfo.sConfig.cam0Info.iFps;
	mediaConfig.videoConfig[0].bExternProc	= false;
	mediaConfig.videoConfig[0].nDstWidth	= !mediaConfig.videoConfig[0].bExternProc ? mediaConfig.videoConfig[0].nSrcWidth : 1920;
	mediaConfig.videoConfig[0].nDstHeight	= !mediaConfig.videoConfig[0].bExternProc ? mediaConfig.videoConfig[0].nSrcHeight : 1080;
	mediaConfig.videoConfig[0].nBitrate		= gstAppInfo.sConfig.cam0Info.iBitrate;
	mediaConfig.videoConfig[0].nCodec		= DVR_CODEC_H264;
	
	mediaConfig.videoConfig[1].nPort		= gstAppInfo.sConfig.cam1Info.iPort;
	mediaConfig.videoConfig[1].nSrcWidth	= gstAppInfo.sConfig.cam1Info.iWidth;
	mediaConfig.videoConfig[1].nSrcHeight	= gstAppInfo.sConfig.cam1Info.iHeight;
	mediaConfig.videoConfig[1].nFps			= gstAppInfo.sConfig.cam1Info.iFps;
	mediaConfig.videoConfig[1].bExternProc	= false;
	mediaConfig.videoConfig[1].nDstWidth	= !mediaConfig.videoConfig[1].bExternProc ? mediaConfig.videoConfig[1].nSrcWidth : 720;
	mediaConfig.videoConfig[1].nDstHeight	= !mediaConfig.videoConfig[1].bExternProc ? mediaConfig.videoConfig[1].nSrcHeight : 480;
	mediaConfig.videoConfig[1].nBitrate		= gstAppInfo.sConfig.cam1Info.iBitrate;
	mediaConfig.videoConfig[1].nCodec		= DVR_CODEC_H264;
	
	mediaConfig.textConfig.nBitrate			= 3000000;
	mediaConfig.textConfig.nInterval		= 200;

	mediaConfig.audioConfig.nChannel		= 2;
	mediaConfig.audioConfig.nFrequency		= 48000;
	mediaConfig.audioConfig.nBitrate		= 128000;
	mediaConfig.audioConfig.nCodec			= gstAppInfo.sConfig.iAudioEnc ? DVR_CODEC_AAC : DVR_CODEC_MP3 ;

	recordConfig.nNormalDuration			= 20000;
	recordConfig.nEventDuration				= 10000;
	recordConfig.nEventBufferDuration		= 10000;
	
	recordConfig.networkType				= gstAppInfo.sConfig.iNetwork;

	// HLS Configuration
	recordConfig.hlsConfig.nSegmentDuration = 10;
	recordConfig.hlsConfig.nSegmentNumber	= 3;
	sprintf( (char*)recordConfig.hlsConfig.MetaFileName,	"test.m3u8" );
	sprintf( (char*)recordConfig.hlsConfig.SegmentFileName,	"segment" );
	sprintf( (char*)recordConfig.hlsConfig.SegmentRootDir,	"/www" );

	// RTP Configuration
	recordConfig.rtpConfig.nPort			= 554;
	recordConfig.rtpConfig.nSessionNum		= gstAppInfo.sConfig.iChannel;
	recordConfig.rtpConfig.nConnectNum		= 2;
	sprintf( (char*)recordConfig.rtpConfig.sessionName[0],	"video0" );
	sprintf( (char*)recordConfig.rtpConfig.sessionName[1],	"video1" );

	recordConfig.bMdEnable[0]				= gstAppInfo.sConfig.bMotion;
	recordConfig.mdConfig[0].nMdThreshold	= 100;
	recordConfig.mdConfig[0].nMdSensitivity	= 100;
	recordConfig.mdConfig[0].nMdSampingFrame= 1;

	recordConfig.bMdEnable[1]				= false;
	recordConfig.mdConfig[1].nMdThreshold	= 100;
	recordConfig.mdConfig[1].nMdSensitivity	= 100;
	recordConfig.mdConfig[1].nMdSampingFrame= 1;

	displayConfig.bEnable			= gstAppInfo.sConfig.bDisplay;
	displayConfig.nChannel			= 0;
	displayConfig.nModule			= 0;
	displayConfig.cropRect.nLeft	= 0;
	displayConfig.cropRect.nTop		= 0;
	displayConfig.cropRect.nRight	= gstAppInfo.sConfig.cam0Info.iWidth;
	displayConfig.cropRect.nBottom	= gstAppInfo.sConfig.cam0Info.iHeight;
	displayConfig.dspRect.nLeft		= 0;
	displayConfig.dspRect.nTop		= 0;
	displayConfig.dspRect.nRight	= 720;
	displayConfig.dspRect.nBottom	= 480;

	if( gstAppInfo.sConfig.bDisplay )
		NX_DspVideoSetPriority( DISPLAY_MODULE_MLC0, 0 );

#ifdef ENABLE_AUDIO
	gstAppInfo.hAudio	= NX_AudioInit();	
#endif
	gstAppInfo.hCmd		= DvrCmdQueueInit();

	printf("############################## STARTING APPLICATION ##############################\n");
	printf(" Build Infomation\n");
	printf("   Build Time : %s, %s\n", __TIME__, __DATE__);
	printf("   Author     : Sung-won Jo\n");
	printf("   Mail       : doriya@nexell.co.kr\n");
	printf("   COPYRIGHT@2013 NEXELL CO. ALL RIGHT RESERVED.\n");
	printf("##################################################################################\n");

	register_signal( );

	DvrSDCheckerTaskStart();

	gstAppInfo.bLoopMsg = true;

	while( gstAppInfo.bLoopMsg )
	{
		memset( &cmd, 0x00, sizeof(CMD_MESSAGE) );
		if( 0 > DvrCmdQueuePop( gstAppInfo.hCmd, &cmd) )
			continue;

		switch( cmd.cmdType )
		{
			case CMD_TYPE_SD_INSERT :
				usleep(100000);
				
				if( !mount( (char*)gstAppInfo.cSDNode, DIRECTORY_TOP, "vfat", 0, "") ) {
					printf("%s(): mount mmc. (mount pos: %s)\n", __FUNCTION__, DIRECTORY_TOP);
				}
				else {
					printf("%s(): mount failed.", __FUNCTION__);
				}

				gstAppInfo.hDvr = NX_DvrInit( &mediaConfig, &recordConfig, &displayConfig );
				if( gstAppInfo.hDvr )	NX_DvrGetAPIVersion( gstAppInfo.hDvr, &gstAppInfo.iVersionMajor, &gstAppInfo.iVersionMinor, &gstAppInfo.iVersionRevision );

				DvrPrintConfig( &mediaConfig );

				memset( gstAppInfo.cNormalDir,	0x00, sizeof(gstAppInfo.cNormalDir)	);
				memset( gstAppInfo.cEventDir,	0x00, sizeof(gstAppInfo.cEventDir)	);
				memset( gstAppInfo.cCaptureDir,	0x00, sizeof(gstAppInfo.cCaptureDir));

				sprintf( (char*)gstAppInfo.cNormalDir,	"%s/normal",	gstAppInfo.cTopDir );
				sprintf( (char*)gstAppInfo.cEventDir,	"%s/event",		gstAppInfo.cTopDir );
				sprintf( (char*)gstAppInfo.cCaptureDir,	"%s/capture",	gstAppInfo.cTopDir );

				gstAppInfo.hNormalFileManager	= DvrFileManagerInit( (const char*)gstAppInfo.cNormalDir,	50,	gstAppInfo.sConfig.iContainer ? "ts" : "mp4" );
				gstAppInfo.hEventFileManager	= DvrFileManagerInit( (const char*)gstAppInfo.cEventDir,	20,	gstAppInfo.sConfig.iContainer ? "ts" : "mp4" );
				gstAppInfo.hCaptureFileManager	= DvrFileManagerInit( (const char*)gstAppInfo.cCaptureDir,	1,	"jpeg" );

				gstAppInfo.hGpsManager			= DvrGpsManagerInit();
				gstAppInfo.hGsensorManager 		= DvrGsensorManagerInit();

#ifdef ENABLE_POWER_MANAGER
				gstAppInfo.hPowerManager		= DvrPowerManagerInit();
#endif

				if( gstAppInfo.hPowerManager )			DvrPowerManagerRegCmd( gstAppInfo.hPowerManager, gstAppInfo.hCmd );
				if( gstAppInfo.hGsensorManager )		DvrGsensorManagerRegCmd( gstAppInfo.hGsensorManager, gstAppInfo.hCmd );
				if( gstAppInfo.hGsensorManager )		DvrGsensorManagerMotionEnable( gstAppInfo.hGsensorManager, gstAppInfo.sConfig.bMotion );

				if( gstAppInfo.hNormalFileManager )		DvrFileManagerStart( gstAppInfo.hNormalFileManager );
				if( gstAppInfo.hEventFileManager )		DvrFileManagerStart( gstAppInfo.hEventFileManager );
				if( gstAppInfo.hCaptureFileManager )	DvrFileManagerStart( gstAppInfo.hCaptureFileManager );
	
				if( mediaConfig.videoConfig[0].bExternProc )
					gstAppInfo.hFrontEffect		= NX_GTSclOpen( mediaConfig.videoConfig[0].nSrcWidth, mediaConfig.videoConfig[0].nSrcHeight, mediaConfig.videoConfig[0].nDstWidth, mediaConfig.videoConfig[0].nDstHeight, 12 );
				if( mediaConfig.videoConfig[1].bExternProc )
					gstAppInfo.hRearEffect		= NX_GTSclOpen( mediaConfig.videoConfig[1].nSrcWidth, mediaConfig.videoConfig[1].nSrcHeight, mediaConfig.videoConfig[1].nDstWidth, mediaConfig.videoConfig[1].nDstHeight, 12 );

				if( gstAppInfo.hDvr )	NX_DvrRegisterFileNameCallback( gstAppInfo.hDvr, cbGetNormalFileName, cbGetEventFileName, NULL , cbGetCaptureFileName );
				if( gstAppInfo.hDvr )	NX_DvrRegisterTextOverlayCallback( gstAppInfo.hDvr, cbGetFrontTextOverlay, cbGetRearTextOverlay );
				if( gstAppInfo.hDvr )	NX_DvrRegisterUserDataCallback( gstAppInfo.hDvr, cbUserData );
				if( gstAppInfo.hDvr )	NX_DvrRegisterNotifyCallback( gstAppInfo.hDvr, cbNotifier );
				if( gstAppInfo.hDvr )	NX_DvrRegisterImageEffectCallback( gstAppInfo.hDvr, cbFrontImageEffect, cbRearImageEffect );
#ifdef EXTERNAL_MOTION_DETECTION
				if( gstAppInfo.hDvr )	NX_DvrRegisterMotionDetectCallback( gstAppInfo.hDvr, cbMotionDetect, cbMotionDetect );
#endif
				NX_DvrStart( gstAppInfo.hDvr, (gstAppInfo.iMode == DVR_MODE_NORMAL) ? DVR_ENCODE_NORMAL : DVR_ENCODE_MOTION );
				if( gstAppInfo.hAudio )	NX_AudioPlay( gstAppInfo.hAudio, "/root/wav/start.wav");

				if( gstAppInfo.hGpsManager )		DvrGpsManagerStart( gstAppInfo.hGpsManager );
				if( gstAppInfo.hGsensorManager )	DvrGsensorManagerStart( gstAppInfo.hGsensorManager );
				if( gstAppInfo.hPowerManager )		DvrPowerManagerStart( gstAppInfo.hPowerManager );

				DvrConsoleTaskStart();
				DvrEventTaskStart();

				DvrInputEventTaskStart();
				break;

			case CMD_TYPE_SD_REMOVE :
				DvrInputEventTaskStop();

				DvrEventTaskStop();
				DvrConsoleTaskStop();

				if( gstAppInfo.hDvr ) NX_DvrStop( gstAppInfo.hDvr );
				if( gstAppInfo.hDvr ) NX_DvrDeinit( gstAppInfo.hDvr );

				if( gstAppInfo.hAudio )	NX_AudioPlay( gstAppInfo.hAudio, "/root/wav/stop.wav" );
				if( gstAppInfo.hAudio )	NX_AudioStop( gstAppInfo.hAudio, true );

				if( gstAppInfo.hGpsManager ) 	DvrGpsManagerStop( gstAppInfo.hGpsManager );
				if( gstAppInfo.hGpsManager ) 	DvrGpsManagerDeinit( gstAppInfo.hGpsManager );

				if( gstAppInfo.hGsensorManager ) DvrGsensorManagerStop( gstAppInfo.hGsensorManager );
				if( gstAppInfo.hGsensorManager ) DvrGsensorManagerDeinit( gstAppInfo.hGsensorManager );

				if( gstAppInfo.hPowerManager )	DvrPowerManagerStop( gstAppInfo.hPowerManager );
				if( gstAppInfo.hPowerManager )	DvrPowerManagerDeinit( gstAppInfo.hPowerManager );

				if( gstAppInfo.hNormalFileManager )	DvrFileManagerStop( gstAppInfo.hNormalFileManager );
				if( gstAppInfo.hNormalFileManager )	DvrFileManagerDeinit( gstAppInfo.hNormalFileManager );
				if( gstAppInfo.hEventFileManager )	DvrFileManagerStop( gstAppInfo.hEventFileManager );
				if( gstAppInfo.hEventFileManager )	DvrFileManagerDeinit( gstAppInfo.hEventFileManager );
				if( gstAppInfo.hCaptureFileManager )	DvrFileManagerStop( gstAppInfo.hCaptureFileManager );
				if( gstAppInfo.hCaptureFileManager )	DvrFileManagerDeinit( gstAppInfo.hCaptureFileManager );				

				if( gstAppInfo.hGpsManager ) 		gstAppInfo.hGpsManager = NULL;
				if( gstAppInfo.hGsensorManager ) 	gstAppInfo.hGsensorManager = NULL;
				if( gstAppInfo.hPowerManager )		gstAppInfo.hPowerManager = NULL;
				if( gstAppInfo.hNormalFileManager )	gstAppInfo.hNormalFileManager = NULL;
				if( gstAppInfo.hEventFileManager )	gstAppInfo.hEventFileManager = NULL;
				if( gstAppInfo.hCaptureFileManager )	gstAppInfo.hCaptureFileManager = NULL;

				printf("%s(): umount mmc.\n", __FUNCTION__);
				int32_t pendCnt = 0;
				while( 1 )
				{
					pendCnt++;
					sync();
					usleep(100000);
					if( !umount("/mnt/mmc") ) break;
					if( pendCnt < 10) {
						printf("Fail, umount..\n");
						break;
					}
					printf("[%d] Retry umount..\n", pendCnt);
				}
				break;

			case CMD_TYPE_EVENT :
				pthread_mutex_lock( &gstAppInfo.hLock );
				if( gstAppInfo.iMode != DVR_MODE_EVENT ) {
					gstAppInfo.iMode = DVR_MODE_EVENT;
					pthread_mutex_unlock( &gstAppInfo.hLock );	
					if( gstAppInfo.hAudio )	NX_AudioPlay( gstAppInfo.hAudio, "/root/wav/event.wav" );
					DvrLedEventStart();
					if( gstAppInfo.hDvr )	NX_DvrEvent( gstAppInfo.hDvr );
				}
				else {
					pthread_mutex_unlock( &gstAppInfo.hLock );	
				}
				break;

			case CMD_TYPE_CAPTURE :
				if( gstAppInfo.hAudio )	NX_AudioPlay( gstAppInfo.hAudio, "/root/wav/capture.wav" );
				if( gstAppInfo.hDvr )	NX_DvrCapture( gstAppInfo.hDvr, cmd.cmdData );
				break;

			case CMD_TYPE_CHG_LCD :
				if( gstAppInfo.hDvr )	NX_DvrSetPreview( gstAppInfo.hDvr, cmd.cmdData );
				break;

			case CMD_TYPE_CHG_HDMI :
				if( gstAppInfo.hDvr )	NX_DvrSetPreviewHdmi( gstAppInfo.hDvr, cmd.cmdData );
				break;

			case CMD_TYPE_CHG_MODE :
				pthread_mutex_lock( &gstAppInfo.hLock );
				if( gstAppInfo.iMode != DVR_MODE_MOTION ) {
					gstAppInfo.iMode = DVR_MODE_MOTION;
					DvrLedMotionStart();
				}
				else {
					gstAppInfo.iMode = DVR_MODE_NORMAL;
					DvrLedMotionStop();
				}
				pthread_mutex_unlock( &gstAppInfo.hLock );

				if( gstAppInfo.hDvr )	NX_DvrChangeMode( gstAppInfo.hDvr, cmd.cmdData );
				break;

			case CMD_TYPE_LOW_VOLTAGE :
				printf("Detect Low voltage.\n");
				if( gstAppInfo.hAudio )	NX_AudioPlay( gstAppInfo.hAudio, "/root/wav/poweroff.wav" );
				interval = GetSystemTickCount();
				ExitApp();
				interval = GetSystemTickCount() - interval;
				printf("Stop Application. ( %lld mSec )\n", interval );

				MicomKillSignal();
				gstAppInfo.bLoopMsg = false;
				break;

			case CMD_TYPE_HIGH_TEMP :
				printf("Detect High Temperature.\n");
				if( gstAppInfo.hAudio )	NX_AudioPlay( gstAppInfo.hAudio, "/root/wav/poweroff.wav" );
				interval = GetSystemTickCount();
				ExitApp();
				interval = GetSystemTickCount() - interval;
				printf("Stop Application. ( %lld mSec )\n", interval );

				MicomKillSignal();
				gstAppInfo.bLoopMsg = false;
				break;

			case CMD_TYPE_MICOM_INT :
				printf("Micom Interrput.\n");
				if( gstAppInfo.hAudio )	NX_AudioPlay( gstAppInfo.hAudio, "/root/wav/poweroff.wav" );
				interval = GetSystemTickCount();
				ExitApp();
				interval = GetSystemTickCount() - interval;
				printf("Stop Application. ( %lld mSec )\n", interval );

				MicomKillSignal();
				gstAppInfo.bLoopMsg = false;
				break;
				
			default :
				printf("Unknown Command!\n");
				break;
		}
	}

END :
	return 0;
}
