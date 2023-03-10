/***************************************************************************

    M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
    Win32 Portions Copyright (C) 1997-98 Michael Soderstrom and Chris Kirmse
    
    This file is part of MAME32, and may only be used, modified and
    distributed under the terms of the MAME license, in "readme.txt".
    By continuing to use, modify or distribute this file you indicate
    that you have read the license and understand and accept it fully.

 ***************************************************************************/

/***************************************************************************

  M32Util.c

 ***************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <assert.h>
#include <stdio.h>
#include "MAME32.h"
#include "M32Util.h"

/***************************************************************************
    function prototypes
 ***************************************************************************/

/***************************************************************************
    External variables
 ***************************************************************************/

BOOL    bErrorMsgBox = TRUE;

/***************************************************************************
    Internal structures
 ***************************************************************************/

/***************************************************************************
    Internal variables
 ***************************************************************************/


/***************************************************************************
    External functions
 ***************************************************************************/

/*
    ErrorMsg
*/
void __cdecl ErrorMsg(const char* fmt, ...)
{
    static FILE*    pFile = NULL;
    DWORD           dwWritten;
    char            buf[5000];
    va_list         va;

    va_start(va, fmt);

    if (bErrorMsgBox == TRUE)
    {
        wvsprintf(buf, fmt, va);
        MessageBox(GetActiveWindow(), buf, MAME32App.m_Name, MB_OK | MB_ICONERROR);
    }

    lstrcpy(buf, MAME32App.m_Name);
    lstrcat(buf, ": ");

    wvsprintf(&buf[lstrlen(buf)], fmt, va);
    lstrcat(buf, "\n");

    OutputDebugString(buf);

    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, strlen(buf), &dwWritten, NULL);

    if (pFile == NULL)
        pFile = fopen("debug.txt", "wt");
    if (pFile != NULL)
        fprintf(pFile, buf);

    va_end(va);
}

/* Used by common.c */
int strcasecmp(const char* a, const char* b)
{
    return stricmp(a, b);
}

/* Parse the given comma-delimited string into a LOGFONT structure */
void FontDecodeString(char *string, LOGFONT *f)
{
    char *ptr;
    
    sscanf(string, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i",
        &f->lfHeight,
        &f->lfWidth,
        &f->lfEscapement,
        &f->lfOrientation,
        &f->lfWeight,
        &f->lfItalic,
        &f->lfUnderline,
        &f->lfStrikeOut,
        &f->lfCharSet,
        &f->lfOutPrecision,
        &f->lfClipPrecision,
        &f->lfQuality,
        &f->lfPitchAndFamily);
    ptr = strrchr(string, ',');
    if (ptr != NULL)
        strcpy(f->lfFaceName, ptr + 1);
}

/* Encode the given LOGFONT structure into a comma-delimited string */
void FontEncodeString(LOGFONT *f, char *string)
{
    sprintf(string, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%s",
        f->lfHeight,
        f->lfWidth,
        f->lfEscapement,
        f->lfOrientation,
        f->lfWeight,
        f->lfItalic,
        f->lfUnderline,
        f->lfStrikeOut,
        f->lfCharSet,
        f->lfOutPrecision,
        f->lfClipPrecision,
        f->lfQuality,
        f->lfPitchAndFamily,
        f->lfFaceName
        );
}

UINT GetDepth(HWND hWnd)
{
    UINT    nBPP;
    HDC     hDC;

    hDC = GetDC(hWnd);
    
    nBPP = GetDeviceCaps(hDC, BITSPIXEL) * GetDeviceCaps(hDC, PLANES);

    ReleaseDC(hWnd, hDC);

    return nBPP;
}

BOOL OnNT()
{
    OSVERSIONINFO version_info;

    version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&version_info);

    if (version_info.dwPlatformId == VER_PLATFORM_WIN32_NT)
        return TRUE;
    else
        return FALSE;
}

/*
 * Return TRUE if comctl32.dll is version 4.71 or greater
 * otherwise return FALSE.
 */
BOOL GetDllVersion()
{
    HMODULE hModule = GetModuleHandle("comctl32");

    if (hModule)
    {
        FARPROC lpfnICCE = GetProcAddress(hModule, "InitCommonControlsEx");

        if (NULL != lpfnICCE)
        {
            FARPROC lpfnDLLI = GetProcAddress(hModule, "DllInstall");

            if (NULL != lpfnDLLI) 
            {
                /* comctl 4.71 or greater */
                return TRUE;
            }
            /* comctl 4.7 - fall through
             * return FALSE;
             */
        }
        /* comctl 4.0 - fall through
         * return FALSE;
         */
    }
    /* DLL not found */
    return FALSE;
}

void DisplayTextFile(HWND hWnd, char *cName)
{
    HINSTANCE hErr;
    char      *msg = 0;

    hErr = ShellExecute(hWnd, NULL, cName, NULL, NULL, SW_SHOWNORMAL);
    if ((int)hErr > 32)
        return;

    switch((int)hErr)
    {
    case 0:
        msg = "The operating system is out of memory or resources.";
        break;

    case ERROR_FILE_NOT_FOUND:
        msg = "The specified file was not found."; 
        break;

    case SE_ERR_NOASSOC :
        msg = "There is no application associated with the given filename extension.";
        break;

    case SE_ERR_OOM :
        msg = "There was not enough memory to complete the operation.";
        break;

    case SE_ERR_PNF :
        msg = "The specified path was not found.";
        break;

    case SE_ERR_SHARE :
        msg = "A sharing violation occurred.";
        break;

    default:
        msg = "Unknown error.";
    }
 
    MessageBox(NULL, msg, cName, MB_OK); 
}

/***************************************************************************
    Internal functions
 ***************************************************************************/

