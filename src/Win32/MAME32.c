/***************************************************************************

    M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
    Win32 Portions Copyright (C) 1997-98 Michael Soderstrom and Chris Kirmse
    
    This file is part of MAME32, and may only be used, modified and
    distributed under the terms of the MAME license, in "readme.txt".
    By continuing to use, modify or distribute this file you indicate
    that you have read the license and understand and accept it fully.

 ***************************************************************************/

/***************************************************************************

  MAME32.c

 ***************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include <string.h>
#include <excpt.h>
#include "common.h"
#include "usrintrf.h"
#include "osdepend.h"
#include "mame.h"
#include "driver.h"
#include "MAME32.h"
#include "Display.h"
#include "Sound.h"
#include "Keyboard.h"
#include "Joystick.h"
#include "Trak.h"
#include "resource.h"
#include "M32Util.h"

/***************************************************************************
    Function prototypes
 ***************************************************************************/

static LRESULT CALLBACK     MAME32_MessageProc(HWND, UINT, WPARAM, LPARAM);
static HWND                 MAME32_CreateWindow(void);
static void                 MAME32_ProcessMessages(void);
static BOOL                 MAME32_PumpAndReturnMessage(MSG* pMsg);
static void                 MAME32_Quit(void);
static BOOL                 MAME32_DetectMMX();
static BOOL                 MAME32_Done(void);

static BOOL                 OnMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
static void                 OnActivateApp(HWND hWnd, BOOL fActivate, DWORD dwThreadId);
static void                 OnSysCommand(HWND hWnd, UINT cmd, int x, int y);
static void                 OnClose(HWND hWnd);

/***************************************************************************
    External variables
 ***************************************************************************/

struct tMAME32App   MAME32App = 
{
    { NULL },                   /* m_hwndUI          */
    { NULL },                   /* m_hWnd            */
    { "MAME32" },               /* m_pName           */
    { FALSE },                  /* m_bIsInitialized  */
    { FALSE },                  /* m_bIsActive       */
    { FALSE },                  /* m_bIsPaused       */
    { FALSE },                  /* m_bDone           */
    { FALSE },                  /* m_bMMXDetected    */
    { FALSE },                  /* m_bPaused         */
    { FALSE },                  /* m_bUseAIMouse     */

    { NULL },                   /* m_pDisplay        */
    { NULL },                   /* m_pSound          */
    { NULL },                   /* m_pKeyboard       */
    { NULL },                   /* m_pJoystick       */
    { NULL },                   /* m_pTrak           */
    { NULL },                   /* m_pFMSynth        */
#ifdef MAME_NET
    { NULL },                   /* m_pNetwork        */
    { FALSE },                  /* m_bUseNetwork     */
#endif /* MAME_NET */

    { MAME32_CreateWindow },            /* CreateMAMEWindow     */
    { MAME32_ProcessMessages },         /* ProcessMessages      */
    { MAME32_PumpAndReturnMessage },    /* PumpAndReturnMessage */
    { MAME32_Quit },                    /* Quit                 */
    { MAME32_DetectMMX },               /* detect MMX           */
    { MAME32_Done },                    /* close was clicked    */
};

/***************************************************************************
    Internal structures
 ***************************************************************************/

/***************************************************************************
    Internal variables
 ***************************************************************************/

static BOOL auto_pause;

/***************************************************************************
    External functions  
 ***************************************************************************/

void MAME32App_init(options_type *options)
{
    MAME32App.m_hWnd = NULL;

    MAME32App.m_bIsInitialized = FALSE;
    MAME32App.m_bIsActive      = FALSE;
    MAME32App.m_bIsPaused      = FALSE;
    MAME32App.m_pDisplay    = NULL;
    MAME32App.m_pSound      = NULL;
    MAME32App.m_pKeyboard   = NULL;
    MAME32App.m_pJoystick   = NULL;
    MAME32App.m_pTrak       = NULL;
    MAME32App.m_pFMSynth    = NULL;
    MAME32App.m_bDone       = FALSE;

#if defined(MAME_DEBUG)
    auto_pause = FALSE;
#else
    auto_pause = options->auto_pause;
#endif

    /*
        Machine->scrbitmap is not initialized in the mame source
        until after the call to osd_create_display().
        This causes a problem if osd_create_display() detects an error,
        then shows the error message box, which causes focus to change,
        which causes OnPause() to be called, which uses Machine->scrbitmap.
        Whew. So it needs to be set to NULL so it doesn't crash in OnPause().
    */
    Machine->scrbitmap = NULL;
}

/***************************************************************************
    Internal functions
 ***************************************************************************/

static HWND MAME32_CreateWindow(void)
{
    static BOOL     bRegistered = FALSE;
    HINSTANCE       hInstance = GetModuleHandle(NULL);

    if (bRegistered == FALSE)
    {
        WNDCLASS    WndClass;

        WndClass.style          = CS_SAVEBITS | CS_BYTEALIGNCLIENT | CS_OWNDC;
        WndClass.lpfnWndProc    = MAME32_MessageProc;
        WndClass.cbClsExtra     = 0;
        WndClass.cbWndExtra     = 0;
        WndClass.hInstance      = hInstance;
        WndClass.hIcon          = LoadIcon(hInstance, MAKEINTATOM(IDI_MAME32_ICON));
        WndClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        WndClass.hbrBackground  = (HBRUSH)GetStockObject(NULL_BRUSH);
        WndClass.lpszMenuName   = NULL;
        WndClass.lpszClassName  = (LPCSTR)"classMAME32";
        
        if (RegisterClass(&WndClass) == 0)
            return NULL;
        bRegistered = TRUE;
    }

    return CreateWindowEx(0,
                          "classMAME32",
                          MAME32App.m_Name,
                          WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME | WS_BORDER,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          0, 0,
                          NULL,
                          NULL,
                          hInstance,
                          NULL);
}

static void MAME32_ProcessMessages(void)
{
    MSG Msg;

    while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE))
    {
        if (Msg.message == WM_QUIT)
        {
            MAME32App.Quit();
            return;
        }
        
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    return;
}

static BOOL MAME32_PumpAndReturnMessage(MSG* pMsg)
{
    if (!(GetMessage(pMsg, NULL, 0, 0)))
        return FALSE;

    TranslateMessage(pMsg);
    DispatchMessage(pMsg);
    return TRUE;
}

static void MAME32_Quit()
{
    /* Tell MAME to quit. */
    MAME32App.m_bDone = TRUE;
}

static BOOL MAME32_Done()
{
    return MAME32App.m_bDone;
}


static LRESULT CALLBACK MAME32_MessageProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT Result = 0;

    if (OnMessage(hWnd, Msg, wParam, lParam, &Result) == TRUE)
        return Result;

    if (MAME32App.m_pDisplay->OnMessage(hWnd, Msg, wParam, lParam, &Result) == TRUE)
        return Result;

    if (MAME32App.m_pTrak != NULL)
        if (MAME32App.m_pTrak->OnMessage(hWnd, Msg, wParam, lParam, &Result) == TRUE)
            return Result;

    if (MAME32App.m_pKeyboard->OnMessage(hWnd, Msg, wParam, lParam, &Result) == TRUE)
        return Result;
/*
    if (MAME32App.m_pSound->OnMessage(hWnd, Msg, wParam, lParam, &Result) == TRUE)
        return Result;
*/
    if (MAME32App.m_pJoystick->OnMessage(hWnd, Msg, wParam, lParam, &Result) == TRUE)
        return Result;

    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

static BOOL OnMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    switch (Msg)
    {
        PEEK_MESSAGE(hWnd, WM_ACTIVATEAPP,    OnActivateApp);
        HANDLE_MESSAGE(hWnd, WM_SYSCOMMAND,   OnSysCommand);
        HANDLE_MESSAGE(hWnd, WM_CLOSE,        OnClose);
    }
    return FALSE;
}

static void OnActivateApp(HWND hWnd, BOOL fActivate, DWORD dwThreadId)
{

    MAME32App.m_bIsActive = fActivate && MAME32App.m_hWnd == hWnd;

    if (MAME32App.m_bIsActive == FALSE)
    {
        if (auto_pause && MAME32App.m_bIsInitialized == TRUE )
            MAME32App.m_bIsPaused = TRUE;
    }
    else if (MAME32App.m_bIsInitialized && MAME32App.m_bIsPaused)
    {
        extern int bitmap_dirty;
        MAME32App.m_bIsPaused = FALSE;
        bitmap_dirty = 1;

        osd_mark_dirty(0, 0, Machine->scrbitmap->width - 1, Machine->scrbitmap->height - 1, 1);
    }
}


static void OnSysCommand(HWND hWnd, UINT cmd, int x, int y)
{
    if (cmd != SC_KEYMENU || MAME32App.m_bIsPaused == TRUE)
        FORWARD_WM_SYSCOMMAND(hWnd, cmd, x, y, DefWindowProc);
}

static void OnClose(HWND hWnd)
{
    /* hide window to prevent orphan empty rectangles on the taskbar */
    ShowWindow(hWnd,SW_HIDE);

    /* Don't call DestroyWindow, it will be called by osd_exit. */
    MAME32App.Quit();
}

static BOOL MAME32_DetectMMX()
{
#if defined(_M_IX86) && defined(MAME_MMX)

    BOOL result = FALSE;
    int cpu_properties;


#define ASM_CPUID __asm _emit 0xf __asm _emit 0xa2

    __try
    {
        __asm
        {
            mov eax, 1
            ASM_CPUID
            mov cpu_properties, edx
        }
        /* if bit 23 is set, MMX is available */
        if (cpu_properties & 0x800000)
            result = TRUE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }

    return result;

#else
    return FALSE;
#endif
}
