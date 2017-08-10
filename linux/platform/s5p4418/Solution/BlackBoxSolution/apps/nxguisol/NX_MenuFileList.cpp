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

#include <dirent.h>

#include <CNX_BaseWindow.h>
#include <CNX_TextBox.h>
#include <CNX_PushButton.h>
#include <CNX_ProgressBar.h>
#include <CNX_ToggleButton.h>
#include <CNX_ListBox.h>

#include <NX_PlayerMain.h>
#include <NX_JpegDecode.h>

#include "NX_MenuFileList.h"
#include "NX_GuiConfig.h"

static CNX_BaseWindow	*gstWnd 		= NULL;
static CNX_TextBox		*gstTxtTitle	= NULL;
static CNX_PushButton	*gstBtnNormal	= NULL;
static CNX_PushButton	*gstBtnEvent	= NULL;
static CNX_PushButton	*gstBtnCapture	= NULL;
static CNX_PushButton	*gstBtnPlay		= NULL;
static CNX_PushButton	*gstBtnExit		= NULL;
static CNX_ListBox		*gstLstFile		= NULL;

static SDL_Surface		*gstSurface		= NULL;
static TTF_Font			*gstFont		= NULL;

extern CNX_BaseObject *GetMenuTopHandle( SDL_Surface *pSurface, TTF_Font *pFont );
extern CNX_BaseObject *GetMenuPlayerHandle( SDL_Surface *pSurface, TTF_Font *pFont );
extern CNX_BaseObject *GetMenuJpegDecodeHandle( SDL_Surface *pSurface, TTF_Font *pFont );

typedef enum {
	LIST_TYPE_NORMAL,
	LIST_TYPE_EVENT,
	LIST_TYPE_CAPTURE,
} ListType;

static char 	m_FileBuffer[1024][255];
static int32_t	m_FileCnt = 0;
static ListType	m_FileListType;

#define FILE_EXTENSION(A)		(A == DVR_CONTAINER_TS) ? ".ts" : ".mp4"

int32_t CompareStrDecending(const void *str1, const void *str2)
{
	int32_t ret = 0;

	ret = strcmp( (const char *) str1, (const char *) str2);

	if(ret > 0) return 1;
	else if(ret < 0) return -1;

	return 0;
}

static int32_t CreateFileList( const char *dir, const char *extension )
{
	DIR *dp;

	struct dirent *dirEntry = NULL;

	char fileName[64];

	memset(m_FileBuffer, 0, sizeof(m_FileBuffer));
	if( (dp = opendir(dir)) == NULL) {
		return -1;
	}

	m_FileCnt = 0;

	while( ((dirEntry = readdir(dp)) != NULL) )
	{
		if( (strlen(dirEntry->d_name) - strlen(extension)) == (unsigned char)(strstr(dirEntry->d_name, extension) - dirEntry->d_name)) {
			sprintf(fileName, "%s/%s", dir, dirEntry->d_name);
			memcpy(m_FileBuffer[m_FileCnt], dirEntry->d_name, sizeof(dirEntry->d_name));
			m_FileCnt++;
		}
	}
	closedir(dp);

	qsort( (void*) m_FileBuffer, m_FileCnt, sizeof(m_FileBuffer[0]), CompareStrDecending);

	return !m_FileCnt;
}

static void FileListPlay( void )
{
	uint8_t FilePath[256];
	uint8_t *fileName = NULL;
	
	fileName = gstLstFile->GetCurItemString();
	memset(FilePath, 0x00, sizeof(FilePath));

	if( m_FileListType != LIST_TYPE_CAPTURE ) {
		sprintf( (char*)FilePath, 
			(m_FileListType == LIST_TYPE_NORMAL) ? "/mnt/mmc/normal/%s" : "/mnt/mmc/event/%s", fileName );

		PlayerStart( (char*)FilePath );

		CNX_BaseWindow *pWnd = (CNX_BaseWindow*)GetMenuPlayerHandle( gstSurface, gstFont );
		pWnd->EventLoop();
	}
	else {
		sprintf( (char*)FilePath, "/mnt/mmc/capture/%s", fileName );

		JpegDecodeStart( FilePath );

		CNX_BaseWindow *pWnd = (CNX_BaseWindow*)GetMenuJpegDecodeHandle( gstSurface, gstFont );
		pWnd->EventLoop();
	}
}

void FileListNormal(void)
{
	printf("Build Normal List!\n");

	m_FileListType = LIST_TYPE_NORMAL;
	CreateFileList("/mnt/mmc/normal", FILE_EXTENSION(gstContainer) );

	gstLstFile->RemoveAll();
	
	for(int32_t i = 0; i < m_FileCnt; i++ )
	{
		gstLstFile->AddString((uint8_t*)m_FileBuffer[i]);	
	}

	gstLstFile->SetPageItemNum( 10 );
	gstLstFile->SetCurItem(0);
	gstLstFile->Update();
}

void FileListEvent( void )
{
	printf("Build Event List!\n");

	m_FileListType = LIST_TYPE_EVENT;
	CreateFileList("/mnt/mmc/event", FILE_EXTENSION(gstContainer) );

	gstLstFile->RemoveAll();
	
	for(int32_t i = 0; i < m_FileCnt; i++ )
	{
		gstLstFile->AddString((uint8_t*)m_FileBuffer[i]);	
	}

	gstLstFile->SetPageItemNum( 10 );
	gstLstFile->SetCurItem(0);
	gstLstFile->Update();
}

void FileListCapture( void )
{
	printf("Build Jpeg List!\n");

	m_FileListType = LIST_TYPE_CAPTURE;
	CreateFileList("/mnt/mmc/capture", ".jpeg");

	gstLstFile->RemoveAll();

	for(int32_t i = 0; i < m_FileCnt; i++ )
	{
		gstLstFile->AddString((uint8_t*)m_FileBuffer[i]);
	}

	gstLstFile->SetPageItemNum( 10 );
	gstLstFile->SetCurItem(0);
	gstLstFile->Update();
}

void FileListNextPlay(void)
{
	uint8_t FilePath[256];
	uint8_t *fileName = NULL;

	fileName = gstLstFile->GetNextItemString();
	memset(FilePath, 0x00, sizeof(FilePath));

	sprintf( (char*)FilePath, 
		(m_FileListType == LIST_TYPE_NORMAL) ? "/mnt/mmc/normal/%s" : "/mnt/mmc/event/%s", fileName );

	PlayerStop();
	PlayerStart( (char*)FilePath );
}

void FileListPrevPlay(void)
{
	uint8_t FilePath[256];
	uint8_t *fileName = NULL;

	fileName = gstLstFile->GetPrevItemString();
	memset(FilePath, 0x00, sizeof(FilePath));

	sprintf( (char*)FilePath, 
		(m_FileListType == LIST_TYPE_NORMAL) ? "/mnt/mmc/normal/%s" : "/mnt/mmc/event/%s", fileName );

	PlayerStop();
	PlayerStart( (char*)FilePath );
}

void FileListNextJpegDecode(void)
{
	uint8_t FilePath[256];
	uint8_t *fileName = NULL;

	fileName = gstLstFile->GetNextItemString();
	memset(FilePath, 0x00, sizeof(FilePath));
	
	sprintf( (char*)FilePath, "/mnt/mmc/capture/%s", fileName );

	JpegDecodeStop();
	JpegDecodeStart( FilePath );
}

void FileListPrevJpegDecode(void)
{
	uint8_t FilePath[256];
	uint8_t *fileName = NULL;

	fileName = gstLstFile->GetPrevItemString();
	memset(FilePath, 0x00, sizeof(FilePath));
	
	sprintf( (char*)FilePath, "/mnt/mmc/capture/%s", fileName );

	JpegDecodeStop();
	JpegDecodeStart( FilePath );
}

static void Move2MenuTop( void )
{
	PlayerStop();
	JpegDecodeStop();

	CNX_BaseWindow *pWnd = (CNX_BaseWindow*)GetMenuTopHandle( gstSurface, gstFont );
	pWnd->EventLoop();
}

static void BuildMenuFileList( void )
{
	SDL_Rect	rtWnd			= {   0,   0, 800, 480 };
	SDL_Rect	rtTxtTitle		= {  10,  10, 800,  50 };
	SDL_Rect	rtBtnNormal		= { 600, 100, 150,  50 };
	SDL_Rect	rtBtnEvent		= { 600, 160, 150,  50 };
	SDL_Rect	rtBtnCapture	= { 600, 220, 150,  50 };
	SDL_Rect	rtBtnPlay		= { 600, 340, 150,  50 };
	SDL_Rect	rtBtnExit		= { 600, 400, 150,  50 };
	SDL_Rect	rtLstFile		= {	 30, 100, 500, 350 };
	OBJ_ATTRIBUTE attr;

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface			= gstSurface;
	attr.rect				= rtWnd;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0xBB, 0xBB, 0xBB);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0xBB, 0xBB, 0xBB);
	gstWnd->Create( &attr );
	gstWnd->SetChild( (CNX_BaseObject*)gstTxtTitle );

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface			= gstSurface;
	attr.rect				= rtTxtTitle;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.pString			= "- Blackbox Simple Player (800 x 480) -";
	attr.fontColor			= (SDL_Color){ 0x00, 0x00, 0x00, 0 };
	gstTxtTitle->Create( &attr );
	gstTxtTitle->SetChild( (CNX_BaseObject*)gstBtnNormal );

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface 			= gstSurface;
	attr.rect				= rtBtnNormal;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.pString			= "Normal";
	attr.fontColor			= (SDL_Color){ 0x00, 0x00, 0x00, 0 };
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0xFF, 0x00);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	attr.focusBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0xFF);
	attr.focusLineColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	gstBtnNormal->Create( &attr );
	gstBtnNormal->SetAction( OBJ_ACTION_EXEC, &FileListNormal );	
	gstBtnNormal->SetChild( (CNX_BaseObject*)gstBtnEvent );	

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface 			= gstSurface;
	attr.rect				= rtBtnEvent;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.pString			= "Event";
	attr.fontColor			= (SDL_Color){ 0x00, 0x00, 0x00, 0 };
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0xFF, 0x00);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	attr.focusBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0xFF);
	attr.focusLineColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	gstBtnEvent->Create( &attr );
	gstBtnEvent->SetAction( OBJ_ACTION_EXEC, &FileListEvent );	
	gstBtnEvent->SetChild( (CNX_BaseObject*)gstBtnCapture );
	
	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface			= gstSurface;
	attr.rect				= rtBtnCapture;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.pString			= "Capture";
	attr.fontColor			= (SDL_Color){ 0x00, 0x00, 0x00, 0 };
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0xFF, 0x00);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	attr.focusBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0xFF);
	attr.focusLineColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	gstBtnCapture->Create( &attr );
	gstBtnCapture->SetAction( OBJ_ACTION_EXEC, &FileListCapture );
	gstBtnCapture->SetChild( (CNX_BaseObject*)gstBtnPlay );

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface 			= gstSurface;
	attr.rect				= rtBtnPlay;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.pString			= "Play";
	attr.fontColor			= (SDL_Color){ 0x00, 0x00, 0x00, 0 };
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0xFF, 0x00);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	attr.focusBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0xFF);
	attr.focusLineColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	gstBtnPlay->Create( &attr );
	gstBtnPlay->SetAction( OBJ_ACTION_WINDOW, &FileListPlay );
	gstBtnPlay->SetChild( (CNX_BaseObject*)gstBtnExit );		

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface 			= gstSurface;
	attr.rect				= rtBtnExit;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.pString			= "Exit";
	attr.fontColor			= (SDL_Color){ 0x00, 0x00, 0x00, 0 };
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0xFF, 0x00);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	attr.focusBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0xFF);
	attr.focusLineColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	gstBtnExit->Create( &attr );
	gstBtnExit->SetAction( OBJ_ACTION_WINDOW, &Move2MenuTop );
	gstBtnExit->SetChild( (CNX_BaseObject*)gstLstFile );	

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface 			= gstSurface;
	attr.rect				= rtLstFile;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.fontColor			= (SDL_Color){ 0xFF, 0xFF, 0xFF, 0 };
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0xFF, 0x00);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	attr.focusBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0xFF);
	attr.focusLineColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	gstLstFile->Create( &attr );
}

CNX_BaseObject *GetMenuFileListHandle( SDL_Surface *pSurface, TTF_Font *pFont )
{
	if( !gstWnd )
	{
		gstSurface	= pSurface;
		gstFont		= pFont;

		gstWnd 			= new CNX_BaseWindow();
		gstTxtTitle		= new CNX_TextBox();
		
		gstBtnNormal	= new CNX_PushButton();
		gstBtnEvent		= new CNX_PushButton();
		gstBtnCapture	= new CNX_PushButton();
		gstBtnPlay		= new CNX_PushButton();
		gstBtnExit		= new CNX_PushButton();
		gstLstFile		= new CNX_ListBox();
		
		BuildMenuFileList();
	}
	
	return gstWnd;
}

#define SAFE_RELEASE(A)		if(A){delete A; A = NULL;}

void ReleaseMenuFileListHandle( CNX_BaseObject *pBaseWindow )
{
	if( pBaseWindow )
	{
		SAFE_RELEASE(gstWnd);
		SAFE_RELEASE(gstTxtTitle);
		SAFE_RELEASE(gstBtnNormal);
		SAFE_RELEASE(gstBtnEvent);
		SAFE_RELEASE(gstBtnCapture);
		SAFE_RELEASE(gstBtnPlay);
		SAFE_RELEASE(gstLstFile);
	}
}
