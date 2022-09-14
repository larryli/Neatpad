//
//	Helper routines for Neatpad
//
//	www.catch22.net
//
#include <tchar.h>
#include <windows.h>

#include "neatpad.h"
#include <shellapi.h>
#include <shlguid.h>
#include <shobjidl.h>

BOOL CheckMenuCommand(HMENU hMenu, int nCommandId, BOOL fChecked)
{
    if (fChecked) {
        CheckMenuItem(hMenu, nCommandId, MF_CHECKED | MF_BYCOMMAND);
        return TRUE;
    } else {
        CheckMenuItem(hMenu, nCommandId, MF_UNCHECKED | MF_BYCOMMAND);
        return FALSE;
    }
}

BOOL ToggleMenuItem(HMENU hmenu, UINT menuid)
{
    if (MF_CHECKED & GetMenuState(hmenu, menuid, MF_BYCOMMAND)) {
        CheckMenuItem(hmenu, menuid, MF_UNCHECKED | MF_BYCOMMAND);
        return FALSE;
    } else {
        CheckMenuItem(hmenu, menuid, MF_CHECKED | MF_BYCOMMAND);
        return TRUE;
    }
}

BOOL EnableMenuCommand(HMENU hmenu, int nCommandId, BOOL fEnable)
{
    if (fEnable) {
        EnableMenuItem(hmenu, nCommandId, MF_ENABLED | MF_BYCOMMAND);
        return TRUE;
    } else {
        EnableMenuItem(hmenu, nCommandId,
                       MF_GRAYED | MF_DISABLED | MF_BYCOMMAND);
        return FALSE;
    }
}

BOOL EnableDlgItem(HWND hDlg, UINT nCommandId, BOOL fEnable)
{
    return EnableWindow(GetDlgItem(hDlg, nCommandId), fEnable);
}

BOOL SaveFileData(TCHAR *szPath, HWND hwnd)
{
    WINDOWPLACEMENT wp = {sizeof(wp)};

    TCHAR szStream[MAX_PATH];
    HANDLE hFile;
    DWORD len;
    BOOL restoretime = FALSE;
    FILETIME ctm, atm, wtm;

    if (szPath == 0 || szPath[0] == 0)
        return FALSE;

    wsprintf(szStream, _T("%s:Neatpad.Winpos"), szPath);

    if (!GetWindowPlacement(hwnd, &wp))
        return FALSE;

    //
    //	Get the filetimes first of all
    //
    if ((hFile = CreateFile(szStream, GENERIC_READ, 0, 0, OPEN_EXISTING, 0,
                            0)) != INVALID_HANDLE_VALUE) {
        if (GetFileTime(hFile, &ctm, &atm, &wtm))
            restoretime = TRUE;

        CloseHandle(hFile);
    }

    //
    //	Now open the stream for writing
    //
    if ((hFile = CreateFile(szStream, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, 0,
                            0)) == INVALID_HANDLE_VALUE)
        return FALSE;

    WriteFile(hFile, &wp, sizeof(wp), &len, 0);

    //
    //	Restore timestamp if necessary
    //
    if (restoretime == TRUE)
        SetFileTime(hFile, &ctm, &atm, &wtm);

    CloseHandle(hFile);

    return TRUE;
}

BOOL LoadFileData(TCHAR *szPath, HWND hwnd)
{
    TCHAR szStream[MAX_PATH];
    HANDLE hFile;
    WINDOWPLACEMENT wp = {sizeof(wp)};
    DWORD len;

    if (szPath == 0 || szPath[0] == 0)
        return FALSE;

    wsprintf(szStream, _T("%s:Neatpad.Winpos"), szPath);

    //
    //	Can only set the window-position if the alternate-data-stream exists
    //
    if ((hFile = CreateFile(szStream, GENERIC_READ, 0, 0, OPEN_EXISTING, 0,
                            0)) == INVALID_HANDLE_VALUE)
        return FALSE;

    if (!ReadFile(hFile, &wp, sizeof(wp), &len, 0))
        return FALSE;

    //
    //	Only set the window-position if we read a valid WINDOWPLACEMENT
    // structure!!
    //
    if (len == sizeof(wp) && wp.length == sizeof(wp)) {
        wp.flags = SW_HIDE;
        SetWindowPlacement(hwnd, &wp);
    }

    CloseHandle(hFile);
    return TRUE;
}

BOOL ResolveShortcut(TCHAR *pszShortcut, TCHAR *pszFilePath, int nPathLen)
{
    IShellLink *psl;
    SHFILEINFO info;
    IPersistFile *ppf;

    *pszFilePath = 0; // assume failure

    if ((SHGetFileInfo(pszShortcut, 0, &info, sizeof(info), SHGFI_ATTRIBUTES) ==
         0)) {
        return FALSE;
    }

    // not a shortcut?
    if (!(info.dwAttributes & SFGAO_LINK)) {
        lstrcpyn(pszFilePath, pszShortcut, nPathLen);
        return TRUE;
    }

    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IShellLink, (LPVOID *)&psl)))
        return FALSE;

    if (SUCCEEDED(psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile,
                                              (LPVOID *)&ppf))) {
        if (SUCCEEDED(ppf->lpVtbl->Load(ppf, pszShortcut, STGM_READ))) {
            // Resolve the link, this may post UI to find the link
            if (SUCCEEDED(psl->lpVtbl->Resolve(psl, 0, SLR_NO_UI))) {
                psl->lpVtbl->GetPath(psl, pszFilePath, nPathLen, NULL, 0);
                ppf->lpVtbl->Release(ppf);
                psl->lpVtbl->Release(psl);
                return TRUE;
            }
        }

        ppf->lpVtbl->Release(ppf);
    }

    psl->lpVtbl->Release(psl);
    return FALSE;
}
