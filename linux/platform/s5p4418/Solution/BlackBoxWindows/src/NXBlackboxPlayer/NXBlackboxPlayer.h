
// NXBlackboxPlayer.h : PROJECT_NAME ���� ���α׷��� ���� �� ��� �����Դϴ�.
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH�� ���� �� ������ �����ϱ� ���� 'stdafx.h'�� �����մϴ�."
#endif

#include "resource.h"		// �� ��ȣ�Դϴ�.


// CNXBlackboxPlayerApp:
// �� Ŭ������ ������ ���ؼ��� NXBlackboxPlayer.cpp�� �����Ͻʽÿ�.
//

class CNXBlackboxPlayerApp : public CWinApp
{
public:
	CNXBlackboxPlayerApp();

// �������Դϴ�.
public:
	virtual BOOL InitInstance();

// �����Դϴ�.

	DECLARE_MESSAGE_MAP()
};

extern CNXBlackboxPlayerApp theApp;