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

#include <CNX_BaseWindow.h>
#include <CNX_TextBox.h>
#include <CNX_PushButton.h>
#include <CNX_ProgressBar.h>
#include <CNX_ToggleButton.h>
#include <CNX_ListBox.h>

#include <NX_JpegDecode.h>

#include "NX_MenuJpegDecode.h"

static CNX_BaseWindow	*gstWnd 		= NULL;
static CNX_PushButton	*gstBtnPrev		= NULL;
static CNX_PushButton	*gstBtnNext		= NULL;
static CNX_PushButton	*gstBtnExit		= NULL;

static SDL_Surface		*gstSurface		= NULL;
static TTF_Font			*gstFont		= NULL;

extern CNX_BaseObject *GetMenuFileListHandle( SDL_Surface *pSurface, TTF_Font *pFont );

extern void FileListNextJpegDecode(void);
extern void FileListPrevJpegDecode(void);

static void PlayPrevious( void )
{
	printf("JpegDecode Previous File.\n");
	FileListPrevJpegDecode();
}

static void PlayNext( void )
{
	printf("JpegDecode Next File.\n");
	FileListNextJpegDecode();
}

static void Move2FileList( void )
{
	JpegDecodeStop();

	CNX_BaseWindow *pWnd = (CNX_BaseWindow*)GetMenuFileListHandle( gstSurface, gstFont );
	pWnd->EventLoop();
}

static void BuildMenuJpegDecode( void )
{
	SDL_Rect	rtWnd			= {   0,   0, 800, 480 };
	SDL_Rect	rtBtnPrev		= {  80, 400, 120,  50 };
	SDL_Rect	rtBtnNext		= { 230, 400, 120,  50 };
	SDL_Rect	rtBtnExit		= { 600, 400, 120,  50 };
	OBJ_ATTRIBUTE attr;

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface			= gstSurface;
	attr.rect				= rtWnd;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x09, 0x09, 0x09);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x09, 0x09, 0x09);
	gstWnd->Create( &attr );
	gstWnd->SetChild( (CNX_BaseObject*)gstBtnPrev );

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface 			= gstSurface;
	attr.rect				= rtBtnPrev;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.pString			= "Previous";
	attr.fontColor			= (SDL_Color){ 0x00, 0x00, 0x00, 0 };
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0xFF, 0x00);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	attr.focusBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0xFF);
	attr.focusLineColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	gstBtnPrev->Create( &attr );
	gstBtnPrev->SetAction( OBJ_ACTION_EXEC, &PlayPrevious );	
	gstBtnPrev->SetChild( (CNX_BaseObject*)gstBtnNext );	

	memset( &attr, 0x00, sizeof(OBJ_ATTRIBUTE) );
	attr.pSurface 			= gstSurface;
	attr.rect				= rtBtnNext;
	attr.status				= OBJ_STATUS_NORMAL;
	attr.pFont				= gstFont;
	attr.pString			= "Next";
	attr.fontColor			= (SDL_Color){ 0x00, 0x00, 0x00, 0 };
	attr.normalBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0xFF, 0x00);
	attr.normalLineColor	= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	attr.focusBgColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0xFF);
	attr.focusLineColor		= SDL_MapRGB(gstSurface->format, 0x00, 0x00, 0x00);
	gstBtnNext->Create( &attr );
	gstBtnNext->SetAction( OBJ_ACTION_EXEC, &PlayNext );	
	gstBtnNext->SetChild( (CNX_BaseObject*)gstBtnExit );

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
	gstBtnExit->SetAction( OBJ_ACTION_WINDOW, &Move2FileList );	
}

CNX_BaseObject *GetMenuJpegDecodeHandle( SDL_Surface *pSurface, TTF_Font *pFont )
{
	if( !gstWnd )
	{
		gstSurface	= pSurface;
		gstFont		= pFont;

		gstWnd 		= new CNX_BaseWindow();
		gstBtnPrev	= new CNX_PushButton();
		gstBtnNext	= new CNX_PushButton();
		gstBtnExit	= new CNX_PushButton();
		
		BuildMenuJpegDecode();
	}
	
	return gstWnd;
}

#define SAFE_RELEASE(A)		if(A){delete A; A = NULL;}

void ReleaseMenuJpegDecodeHandle( CNX_BaseObject *pBaseWindow )
{
	if( pBaseWindow )
	{
		SAFE_RELEASE( gstWnd );
		SAFE_RELEASE( gstBtnPrev );
		SAFE_RELEASE( gstBtnNext );
		SAFE_RELEASE( gstBtnExit );
	}
}