//
//	Uniscribe.c
//
//	Written by J Brown 8/3/2003	Freeware
//
//
#define _WIN32_WINNT 0x501
#define UNICODE
#define _UNICODE

#include <tchar.h>
#include <windows.h>

#include "resource.h"
#include "usplib\usplib.h"

#pragma comment(lib, "usp10.lib")
#pragma comment(lib, "usplib/usplib.lib")

HWND hwndMain;
HWND hwndUniView;
HWND hwndCodeView;

HFONT g_hFont;
LOGFONT g_LogFont = {0};
TEXTMETRIC g_textMetric;
int g_nLineHeight;

USPFONT g_uspFontList[1];

TCHAR szAppName[] = _T("Usplib Demo");

#define MAX_STRING_LEN 100

HWND CreateCodePointView(HWND hwndParent);
HWND CreateUniView(HWND hwndParent);

WCHAR g_szCurrentString[MAX_STRING_LEN] = {
    'H',    'e',    'l',    'l', 'o', 0x064a, 0x064f, 0x0633, 0x0627,
    0x0648, 0x0650, 0x064a, 'W', 'o', 'r',    'l',    'd',    0};

//
//	Display the font-chooser dialog
//
BOOL GetFont(HWND hwndParent, LOGFONT *logfont)
{
    CHOOSEFONT cf = {sizeof(cf)};

    cf.hwndOwner = hwndParent;
    cf.lpLogFont = logfont;
    cf.Flags = CF_SCREENFONTS | CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT;

    return ChooseFont(&cf);
}

void UpdateFont(void)
{
    HDC hdc = GetDC(0);

    g_hFont = CreateFontIndirect(&g_LogFont);

    UspInitFont(&g_uspFontList[0], hdc, g_hFont);

    g_nLineHeight =
        g_uspFontList[0].tm.tmHeight + g_uspFontList[0].tm.tmExternalLeading;

    ReleaseDC(0, hdc);
}

void CopyToClipboard(HWND hwnd)
{
    HGLOBAL ptr = GlobalAlloc(GPTR, MAX_STRING_LEN * sizeof(WCHAR));
    memcpy(ptr, g_szCurrentString, lstrlen(g_szCurrentString) * sizeof(WCHAR));

    OpenClipboard(hwnd);
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, ptr);
    CloseClipboard();
}

void PasteFromClipboard(HWND hwnd)
{
    HGLOBAL hMem;
    PVOID ptr;

    OpenClipboard(hwnd);

    hMem = GetClipboardData(CF_UNICODETEXT);
    ptr = GlobalLock(hMem);

    if (ptr) {
        lstrcpyn(g_szCurrentString, ptr, MAX_STRING_LEN);
    }

    GlobalUnlock(hMem);
    CloseClipboard();
}

//
//	Main window procedure - just used to host the CodeView and UniView windows
//
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);

    switch (msg) {
    case WM_CREATE:
        hwndUniView = CreateUniView(hwnd);
        hwndCodeView = CreateCodePointView(hwnd);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_SIZE:
        MoveWindow(hwndUniView, 0, 0, width, 150, TRUE);
        MoveWindow(hwndCodeView, 0, 152, width, height - 152, TRUE);
        return 0;

    case WM_USER:
        InvalidateRect(hwndCodeView, 0, 0);
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

        case IDM_FILE_COPY:
            CopyToClipboard(hwnd);
            return 0;

        case IDM_FILE_PASTE:
            PasteFromClipboard(hwnd);
            InvalidateRect(hwndUniView, 0, 0);
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
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
    wc.lpszClassName = szAppName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    return RegisterClassEx(&wc);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine,
                   int iShowCmd)
{
    MSG msg;
    SCRIPT_DIGITSUBSTITUTE scriptDigitSubstitute;

    // Register main window class
    InitMainWnd();

    ScriptRecordDigitSubstitution(LOCALE_USER_DEFAULT, &scriptDigitSubstitute);

    //
    //	Initialize the font
    //
    g_LogFont.lfHeight = 100;
    lstrcpy(g_LogFont.lfFaceName, L"Times New Roman");

    UpdateFont();

    // g_hFont = CreateFont(-32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"Microsoft
    // Sans Serif");
    //
    //	Create main window
    //
    hwndMain = CreateWindowEx(0,
                              szAppName, // window class name
                              szAppName, // window caption
                              WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                              CW_USEDEFAULT, // initial x position
                              CW_USEDEFAULT, // initial y position
                              560,           // initial x size
                              320,           // initial y size
                              NULL,          // parent window handle
                              NULL,          // use window class menu
                              hInst,         // program instance handle
                              NULL);         // creation parameters

    ShowWindow(hwndMain, iShowCmd);

    //
    //	Standard message loop
    //
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
