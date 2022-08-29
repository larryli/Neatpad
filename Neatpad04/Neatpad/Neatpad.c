//
//	Neatpad - Simple Text Editor application
//
//	www.catch22.net
//	Written by J Brown 2004
//
//	Freeware
//
#include <tchar.h>
#include <windows.h>

#include <commctrl.h>

#include "..\TextView\TextView.h"
#include "resource.h"

#define APP_TITLE _T("Neatpad")
#define WEBSITE_STR _T("www.catch22.net")

TCHAR szAppName[] = APP_TITLE;
HWND hwndMain;
HWND hwndTextView;

TCHAR szFileName[MAX_PATH];
TCHAR szFileTitle[MAX_PATH];

#pragma comment(lib, "../TextView/TextView.lib")

#define _MAX_PATH 260
#define _MAX_FNAME 256
#define _MAX_EXT 256

BOOL ShowOpenFileDlg(HWND hwnd, TCHAR *pstrFileName, TCHAR *pstrTitleName)
{
    TCHAR *szFilter = _T("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0");

    OPENFILENAME ofn = {sizeof(ofn)};

    ofn.hwndOwner = hwnd;
    ofn.hInstance = GetModuleHandle(0);
    ofn.lpstrFilter = szFilter;
    ofn.lpstrFile = pstrFileName;
    ofn.lpstrFileTitle = pstrTitleName;

    ofn.nFilterIndex = 1;
    ofn.nMaxFile = _MAX_PATH;
    ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;

    // flags to control appearance of open-file dialog
    ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_ALLOWMULTISELECT |
                OFN_FILEMUSTEXIST;

    return GetOpenFileName(&ofn);
}

void ShowAboutDlg(HWND hwndParent)
{
    MessageBox(hwndParent, APP_TITLE _T("\r\n\r\n") WEBSITE_STR, APP_TITLE,
               MB_OK | MB_ICONINFORMATION);
}

void SetWindowFileName(HWND hwnd, TCHAR *szFileName)
{
    TCHAR ach[MAX_PATH + sizeof(szAppName) + 4];

    wsprintf(ach, _T("%s - %s"), szFileName, szAppName);
    SetWindowText(hwnd, ach);
}

BOOL DoOpenFile(HWND hwnd, TCHAR *szFileName, TCHAR *szFileTitle)
{
    if (TextView_OpenFile(hwndTextView, szFileName)) {
        SetWindowFileName(hwnd, szFileTitle);
        return TRUE;
    } else {
        MessageBox(hwnd, _T("Error opening file"), APP_TITLE,
                   MB_ICONEXCLAMATION);
        return FALSE;
    }
}

//
//	How to process WM_DROPFILES
//
void HandleDropFiles(HWND hwnd, HDROP hDrop)
{
    TCHAR buf[MAX_PATH];
    TCHAR *name;

    if (DragQueryFile(hDrop, 0, buf, MAX_PATH)) {
        strcpy(szFileName, buf);

        name = strrchr(szFileName, '\\');
        strcpy(szFileTitle, name ? name + 1 : buf);

        DoOpenFile(hwnd, szFileName, szFileTitle);
    }

    DragFinish(hDrop);
}

int PointsToLogical(int nPointSize)
{
    HDC hdc = GetDC(0);
    int nLogSize = -MulDiv(nPointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(0, hdc);

    return nLogSize;
}

HFONT EasyCreateFont(int nPointSize, BOOL fBold, TCHAR *szFace)
{
    return CreateFont(PointsToLogical(nPointSize), 0, 0, 0, fBold ? FW_BOLD : 0,
                      0, 0, 0, 0, 0, 0, 0, 0, szFace);
}

//
//	Main window procedure
//
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int width, height;
    HFONT hFont;

    switch (msg) {
    case WM_CREATE:
        hwndTextView = CreateTextView(hwnd);

        // change the font to look like visual-studio 2003
        hFont = EasyCreateFont(10, FALSE, "Courier New");
        SendMessage(hwndTextView, WM_SETFONT, (WPARAM)hFont, 0);
        TextView_SetLineSpacing(hwndTextView, 0, 1);

        // add a new font
        // hFont = EasyCreateFont(10, FALSE, "Lucida Console");
        // hFont = EasyCreateFont(16, FALSE, "Arial");
        // TextView_AddFont(hwndTextView, hFont);

        // automatically create new document when we start
        PostMessage(hwnd, WM_COMMAND, IDM_FILE_NEW, 0);

        // tell windows that we can handle drag+drop'd files
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_DROPFILES:
        HandleDropFiles(hwnd, (HDROP)wParam);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_FILE_NEW:

            SetWindowFileName(hwnd, _T("Untitled"));
            TextView_Clear(hwndTextView);

            return 0;

        case IDM_FILE_OPEN:

            // get a filename to open
            if (ShowOpenFileDlg(hwnd, szFileName, szFileTitle)) {
                DoOpenFile(hwnd, szFileName, szFileTitle);
            }

            return 0;

        case IDM_HELP_ABOUT:
            ShowAboutDlg(hwnd);
            return 0;
        }
        return 0;

    case WM_SETFOCUS:
        SetFocus(hwndTextView);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_SIZE:
        width = (short)LOWORD(lParam);
        height = (short)HIWORD(lParam);

        MoveWindow(hwndTextView, 0, 0, width, height, TRUE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//
//	Register main window class
//
void InitMainWnd(void)
{
    WNDCLASSEX wcx;
    HANDLE hInst = GetModuleHandle(0);

    // Window class for the main application parent window
    wcx.cbSize = sizeof(wcx);
    wcx.style = 0;
    wcx.lpfnWndProc = WndProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.hInstance = hInst;
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)0;
    wcx.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
    wcx.lpszClassName = szAppName;
    wcx.hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32,
                          LR_CREATEDIBSECTION);
    wcx.hIconSm = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16,
                            16, LR_CREATEDIBSECTION);

    RegisterClassEx(&wcx);
}

//
//	Create a top-level window
//
HWND CreateMainWnd(void)
{
    return CreateWindowEx(0,
                          szAppName, // window class name
                          szAppName, // window caption
                          WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                          CW_USEDEFAULT,      // initial x position
                          CW_USEDEFAULT,      // initial y position
                          560,                // initial x size
                          320,                // initial y size
                          NULL,               // parent window handle
                          NULL,               // use window class menu
                          GetModuleHandle(0), // program instance handle
                          NULL);              // creation parameters
}

//
//	Entry-point for text-editor application
//
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine,
                   int iShowCmd)
{
    MSG msg;
    HACCEL hAccel;

    // initialize window classes
    InitMainWnd();
    InitTextView();

    // create the main window!
    hwndMain = CreateMainWnd();

    ShowWindow(hwndMain, iShowCmd);

    // load keyboard accelerator table
    hAccel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));

    // message-loop
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAccelerator(hwndMain, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
