//
//  main.cpp
//
//  Written by J Brown 2/8/2006 Freeware
//
//
#define _WIN32_WINNT 0x501
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <tchar.h>
#include "resource.h"
#include "sequence.h"
#include "UspLib\UspLib.h"

#pragma comment(lib, "usp10.lib")
#pragma comment(lib, "usplib/usplib.lib")
#pragma comment(lib, "vector/vector.lib")

sequence * g_seq;

HWND hwndMain;
HWND hwndUniView;
HWND hwndSpanView;

HFONT g_hFont;
LOGFONT g_LogFont = {
0};
// TEXTMETRIC   g_textMetric;
// int          g_nLineHeight;

// USPFONT      g_uspFontList[1];

TCHAR szAppName[] = _T("Piece Chain Demo");

#define MAX_STRING_LEN 100

HWND CreateSpanView(HWND hwndParent, sequence * seq);
HWND CreateUniView(HWND hwndParent, sequence * seq);

#define INITIAL_TEXT L"Hello World"
// #define INITIAL_TEXT L"Hello\x64a\x64f\x633\x627\x648\x650\x64aWorld"

//
//  Display the font-chooser dialog
//
BOOL GetFont(HWND hwndParent, LOGFONT * logfont)
{
    CHOOSEFONT cf = {
    sizeof(cf)};

    cf.hwndOwner = hwndParent;
    cf.lpLogFont = logfont;
    cf.Flags = CF_SCREENFONTS | CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT;

    return ChooseFont(&cf);
}

void UpdateFont(void)
{
    g_hFont = CreateFontIndirect(&g_LogFont);
    SendMessage(hwndUniView, WM_SETFONT, (WPARAM)g_hFont, 0);
}

//
//  Main window procedure - just used to host the CodeView and UniView windows
//
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);
    int height2;
    RECT rect;

    switch (msg) {
    case WM_CREATE:
        sequence__init_buffer(g_seq, INITIAL_TEXT, lstrlen(INITIAL_TEXT));
        hwndUniView = CreateUniView(hwnd, g_seq);
        hwndSpanView = CreateSpanView(hwnd, g_seq);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_SIZE:
        GetWindowRect(hwndUniView, &rect);
        height2 = rect.bottom - rect.top;
        MoveWindow(hwndUniView, 0, 0, width, height2, TRUE);
        MoveWindow(hwndSpanView, 0, height2 + 2, width, height - height2 - 2, TRUE);
        InvalidateRect(hwndSpanView, 0, 0);
        return 0;

    case WM_USER:
        InvalidateRect(hwndSpanView, 0, 0);
        return 0;

    case WM_SETFOCUS:
        SetFocus(hwndUniView);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_COMMAND:

        switch (LOWORD(wParam)) {
        case IDM_FILE_FONT:

            if (GetFont(hwnd, &g_LogFont)) {
                DeleteObject(g_hFont);
                g_hFont = CreateFontIndirect(&g_LogFont);
                UpdateFont();

                InvalidateRect(hwndUniView, 0, 0);

                SetFocus(hwndMain);
                SetFocus(hwndUniView);
            }

            return 0;

        case IDM_FILE_ABOUT:

            MessageBox(hwnd, _T("Piece Chain Demo\r\n\r\nCopyright(c) 2006 by ")
                _T("Catch22 Productions.\r\nWritten by J ")
                _T("Brown.\r\n\r\nHompage at www.catch22.net"), _T("Piece Chain Demo"), MB_ICONINFORMATION);
            return 0;

        // Quit :)
        case IDM_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

ATOM InitMainWnd(void)
{
    WNDCLASSEX wc;

    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(0);
    wc.hIcon = LoadIcon(0, MAKEINTRESOURCE(IDI_APPLICATION));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_3DFACE + 1);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
    wc.lpszClassName = szAppName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    return RegisterClassEx(&wc);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int iShowCmd)
{
    MSG msg;
    SCRIPT_DIGITSUBSTITUTE scriptDigitSubstitute;

    g_seq = sequence__new();
    g_LogFont.lfQuality = CLEARTYPE_QUALITY;

    // Register main window class
    InitMainWnd();

    ScriptRecordDigitSubstitution(LOCALE_USER_DEFAULT, &scriptDigitSubstitute);

    //
    //  Create main window
    //
    hwndMain = CreateWindowEx(0, szAppName, // window class name
        szAppName,              // window caption
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,   // initial x position
        CW_USEDEFAULT,          // initial y position
        800,                    // initial x size
        800,                    // initial y size
        NULL,                   // parent window handle
        NULL,                   // use window class menu
        hInst,                  // program instance handle
        NULL);                  // creation parameters

    //
    //  Initialize the font
    //
    g_LogFont.lfHeight = -70;
    lstrcpy(g_LogFont.lfFaceName, L"Times New Roman");
    UpdateFont();

    ShowWindow(hwndMain, iShowCmd);

    //
    //  Standard message loop
    //
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(g_hFont);

    return 0;
}
