//
//	Neatpad - Simple Text Editor application
//
//	www.catch22.net
//	Written by J Brown 2004
//
//	Freeware
//
#define _WIN32_WINNT 0x500
#include <tchar.h>
#include <windows.h>

#include <commctrl.h>

#include "..\TextView\TextView.h"
#include "Neatpad.h"
#include "resource.h"

#define APP_TITLE _T("Neatpad")
#define WEBSITE_STR _T("www.catch22.net")

TCHAR g_szAppName[] = APP_TITLE;
HWND g_hwndMain;
HWND g_hwndTextView;
HFONT g_hFont;

TCHAR g_szFileName[MAX_PATH];
TCHAR g_szFileTitle[MAX_PATH];

#pragma comment(lib, "../TextView/TextView.lib")

#define _MAX_PATH 260
#define _MAX_FNAME 256
#define _MAX_EXT 256

BOOL SaveFileData(TCHAR *szPath, HWND hwnd);
BOOL LoadFileData(TCHAR *szPath, HWND hwnd);

void ShowProperties(HWND hwndParent);
void LoadRegSettings(void);
void SaveRegSettings(void);

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
    TCHAR ach[MAX_PATH + sizeof(g_szAppName) + 4];

    wsprintf(ach, _T("%s - %s"), szFileName, g_szAppName);
    SetWindowText(hwnd, ach);
}

BOOL DoOpenFile(HWND hwndMain, TCHAR *szFileName, TCHAR *szFileTitle)
{
    int fmt, fmtlook[] = {IDM_VIEW_ASCII, IDM_VIEW_UTF8, IDM_VIEW_UTF16,
                          IDM_VIEW_UTF16BE};

    if (TextView_OpenFile(g_hwndTextView, szFileName)) {
        SetWindowFileName(hwndMain, szFileTitle);

        fmt = TextView_GetFormat(g_hwndTextView);

        CheckMenuRadioItem(GetMenu(hwndMain), IDM_VIEW_ASCII, IDM_VIEW_UTF16BE,
                           fmtlook[fmt], MF_BYCOMMAND);

        return TRUE;
    } else {
        MessageBox(hwndMain, _T("Error opening file"), APP_TITLE,
                   MB_ICONEXCLAMATION);
        return FALSE;
    }
}

void NeatpadOpenFile(HWND hwnd, TCHAR *szFile)
{
    TCHAR *name;

    // save current file's position!
    SaveFileData(g_szFileName, hwnd);

    _tcscpy(g_szFileName, szFile);

    name = _tcsrchr(g_szFileName, _T('\\'));
    _tcscpy(g_szFileTitle, name ? name + 1 : szFile);

    DoOpenFile(hwnd, g_szFileName, g_szFileTitle);
}

//
//	How to process WM_DROPFILES
//
void HandleDropFiles(HWND hwnd, HDROP hDrop)
{
    TCHAR buf[MAX_PATH];

    if (DragQueryFile(hDrop, 0, buf, MAX_PATH)) {
        NeatpadOpenFile(hwnd, buf);
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

HFONT EasyCreateFont(int nPointSize, BOOL fBold, DWORD dwQuality, TCHAR *szFace)
{
    return CreateFont(PointsToLogical(nPointSize), 0, 0, 0, fBold ? FW_BOLD : 0,
                      0, 0, 0, 0, 0, 0, dwQuality, 0, szFace);
}

//
//	Main window procedure
//
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int width, height;
    HIMAGELIST hImgList;

    switch (msg) {
    case WM_CREATE:
        g_hwndTextView = CreateTextView(hwnd);

        // load the image list
        hImgList = ImageList_LoadImage(
            GetModuleHandle(0), MAKEINTRESOURCE(IDB_BITMAP1), 16, 0,
            RGB(255, 0, 255), IMAGE_BITMAP,
            LR_LOADTRANSPARENT | LR_CREATEDIBSECTION);

        TextView_SetImageList(g_hwndTextView, hImgList);

        // highlight specific lines with image-index "1"
        TextView_SetLineImage(g_hwndTextView, 16, 1);
        TextView_SetLineImage(g_hwndTextView, 5, 1);
        TextView_SetLineImage(g_hwndTextView, 36, 1);
        TextView_SetLineImage(g_hwndTextView, 11, 1);

        // tell windows that we can handle drag+drop'd files
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_DROPFILES:
        HandleDropFiles(hwnd, (HDROP)wParam);
        return 0;

    case WM_DESTROY:
        SaveFileData(g_szFileName, hwnd);
        PostQuitMessage(0);
        DeleteObject(g_hFont);
        return 0;

    case WM_INITMENU:
        CheckMenuCommand((HMENU)wParam, IDM_VIEW_LINENUMBERS, g_fLineNumbers);
        CheckMenuCommand((HMENU)wParam, IDM_VIEW_LONGLINES, g_fLongLines);
        CheckMenuCommand((HMENU)wParam, IDM_VIEW_SAVEEXIT, g_fSaveOnExit);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_FILE_NEW:

            SetWindowFileName(hwnd, _T("Untitled"));
            TextView_Clear(g_hwndTextView);

            return 0;

        case IDM_FILE_OPEN:

            // get a filename to open
            if (ShowOpenFileDlg(hwnd, g_szFileName, g_szFileTitle)) {
                DoOpenFile(hwnd, g_szFileName, g_szFileTitle);
            }

            return 0;

        case IDM_FILE_PRINT:

            DeleteDC(ShowPrintDlg(hwnd));

            return 0;

        case IDM_VIEW_FONT:
            ShowProperties(hwnd);
            return 0;

        case IDM_VIEW_LINENUMBERS:
            g_fLineNumbers = !g_fLineNumbers;
            TextView_SetStyleBool(g_hwndTextView, TXS_LINENUMBERS,
                                  g_fLineNumbers);
            return 0;

        case IDM_VIEW_LONGLINES:
            g_fLongLines = !g_fLongLines;
            TextView_SetStyleBool(g_hwndTextView, TXS_LONGLINES, g_fLongLines);
            return 0;

        case IDM_VIEW_SAVEEXIT:
            g_fSaveOnExit = !g_fSaveOnExit;
            return 0;

        case IDM_VIEW_SAVENOW:
            SaveRegSettings();
            return 0;

        case IDM_HELP_ABOUT:
            ShowAboutDlg(hwnd);
            return 0;
        }
        return 0;

    case WM_SETFOCUS:
        SetFocus(g_hwndTextView);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_SIZE:
        width = (short)LOWORD(lParam);
        height = (short)HIWORD(lParam);

        MoveWindow(g_hwndTextView, 0, 0, width, height, TRUE);
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
    wcx.lpszClassName = g_szAppName;
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
                          g_szAppName, // window class name
                          g_szAppName, // window caption
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

TCHAR **GetArgvCommandLine(int *argc)
{
#ifdef UNICODE
    return CommandLineToArgvW(GetCommandLineW(), argc);
#else
    *argc = __argc;
    return __argv;
#endif
}

//
//	Entry-point for text-editor application
//
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine,
                   int iShowCmd)
{
    MSG msg;
    HACCEL hAccel;
    TCHAR **argv;
    int argc;

    // initialize window classes
    InitMainWnd();
    InitTextView();

    LoadRegSettings();

    // create the main window!
    g_hwndMain = CreateMainWnd();

    //
    //	Parse the commandline
    //
    argv = GetArgvCommandLine(&argc);

    if (argv && argc >= 2) {
        // open file specified on commmand line
        NeatpadOpenFile(g_hwndMain, argv[1]);
        LoadFileData(argv[1], g_hwndMain);
    } else {
        // automatically create new document when we start
        PostMessage(g_hwndMain, WM_COMMAND, IDM_FILE_NEW, 0);
    }

    ApplyRegSettings();
    ShowWindow(g_hwndMain, iShowCmd);

    //
    // load keyboard accelerator table
    //
    hAccel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));

    //
    // message-loop
    //
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAccelerator(g_hwndMain, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (g_fSaveOnExit)
        SaveRegSettings();

    return 0;
}
