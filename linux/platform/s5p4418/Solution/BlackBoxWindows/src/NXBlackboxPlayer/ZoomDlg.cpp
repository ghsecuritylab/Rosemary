// ZoomDlg.cpp : ���� �����Դϴ�.
//

#include "stdafx.h"
#include "NXBlackboxPlayer.h"
#include "ZoomDlg.h"
#include "afxdialogex.h"

#define TIMER_ZOOM			500 
#define TIMER_ZOOM_REFRESH	10 

#define ZOOM_LEVEL			2

// CZoomDlg ��ȭ �����Դϴ�.

IMPLEMENT_DYNAMIC(CZoomDlg, CDialogEx)

CZoomDlg::CZoomDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CZoomDlg::IDD, pParent)
{

}

CZoomDlg::~CZoomDlg()
{
}

void CZoomDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CZoomDlg, CDialogEx)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_SIZE()
	ON_WM_SIZING()
END_MESSAGE_MAP()


// CZoomDlg �޽��� ó�����Դϴ�.


int CZoomDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialogEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  ���⿡ Ư��ȭ�� �ۼ� �ڵ带 �߰��մϴ�.
	SetTimer( TIMER_ZOOM, TIMER_ZOOM_REFRESH, NULL );

	return 0;
}


void CZoomDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	// TODO: ���⿡ �޽��� ó���� �ڵ带 �߰��մϴ�.
	KillTimer( TIMER_ZOOM );
}


void CZoomDlg::OnClose()
{
	// TODO: ���⿡ �޽��� ó���� �ڵ带 �߰� ��/�Ǵ� �⺻���� ȣ���մϴ�.

	CDialogEx::OnClose();
}


void CZoomDlg::OnPaint()
{
	// TODO: ���⿡ �޽��� ó���� �ڵ带 �߰��մϴ�.
	// �׸��� �޽����� ���ؼ��� CDialogEx::OnPaint()��(��) ȣ������ ���ʽÿ�.
	if (IsIconic()) {
		CPaintDC dc(this);	// device context for painting
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);		
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
	} else {
		CDialog::OnPaint();
	}
}


void CZoomDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: ���⿡ �޽��� ó���� �ڵ带 �߰� ��/�Ǵ� �⺻���� ȣ���մϴ�.
	if( nIDEvent == TIMER_ZOOM ) {
		DrawZoom();
	}

	CDialogEx::OnTimer(nIDEvent);
}


void CZoomDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	// TODO: ���⿡ �޽��� ó���� �ڵ带 �߰��մϴ�.
	RECT wndRect;
	GetWindowRect( &wndRect );
	m_nWndWidth = wndRect.right - wndRect.left;
}


void CZoomDlg::OnSizing(UINT fwSide, LPRECT pRect)
{
	CDialogEx::OnSizing(fwSide, pRect);

	// TODO: ���⿡ �޽��� ó���� �ڵ带 �߰��մϴ�.
	MoveWindow( pRect );
}


void CZoomDlg::DrawZoom()
{
	int scrX = GetSystemMetrics( SM_CXVIRTUALSCREEN );
	int scrY = GetSystemMetrics( SM_CYVIRTUALSCREEN );

	RECT mainRect;
	AfxGetMainWnd()->GetWindowRect( &mainRect );
	
	CClientDC dc( this );
	CDC desktopDC;
	CDC memDC;

	memDC.CreateCompatibleDC( &dc );
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap( &dc, scrX, scrY );
	memDC.SelectObject( &bitmap );

	desktopDC.Attach( ::GetDC(AfxGetMainWnd()->m_hWnd) );
	memDC.BitBlt( 0, 0, mainRect.right - mainRect.left, mainRect.bottom - mainRect.top, &desktopDC, 0, 0, SRCCOPY );

	POINT startPos;
	RECT clientRect;
	
	GetWindowRect( &clientRect );
	int clientWidth = clientRect.right - clientRect.left; 
	int clientHeight = clientRect.bottom - clientRect.top; 
	int srcWidth = clientWidth / ZOOM_LEVEL; 
	int srcHeight = clientHeight / ZOOM_LEVEL; 

	CURSORINFO p;
	p.cbSize = sizeof( CURSORINFO );
	GetCursorInfo( &p );

	ClientToScreen( &clientRect );
	memDC.FillRect( &clientRect, &CBrush(RGB(150, 150, 150)) );

	startPos.x = p.ptScreenPos.x - mainRect.left - srcWidth / 2;
	startPos.y = p.ptScreenPos.y - mainRect.top - srcHeight / 2;

	if(startPos.x < 0) 
		startPos.x = 0;
	if(startPos.y < 0) 
		startPos.y = 0;
	if(startPos.x + srcWidth > mainRect.right - mainRect.left) 
		startPos.x = mainRect.right - mainRect.left - srcWidth; 
	if(startPos.y + srcHeight > mainRect.bottom - mainRect.top) 
		startPos.y = mainRect.bottom - mainRect.top - srcHeight;
	
	dc.StretchBlt( 0, 0, clientWidth, clientHeight, &memDC, startPos.x, startPos.y, srcWidth, srcHeight, SRCCOPY );
}
