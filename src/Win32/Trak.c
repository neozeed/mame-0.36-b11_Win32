/***************************************************************************

    M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
    Win32 Portions Copyright (C) 1997-98 Michael Soderstrom and Chris Kirmse
    
    This file is part of MAME32, and may only be used, modified and
    distributed under the terms of the MAME license, in "readme.txt".
    By continuing to use, modify or distribute this file you indicate
    that you have read the license and understand and accept it fully.

 ***************************************************************************/

/***************************************************************************

  Trak.c

 ***************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include "MAME32.h"
#include "Trak.h"
#include "M32Util.h"
#include "win32ui.h"

/*
    Portions shamelessly copied from msdos.c!
    Credit to Patrick Lawrence (pjl@ns.net) I believe.
 */

#define TRAK_CENTER_X   TRAK_MAXX_RES / 2
#define TRAK_CENTER_Y   TRAK_MAXY_RES / 2
#define TRAK_MIN_X      TRAK_MAXX_RES / 6
#define TRAK_MAX_X      TRAK_MAXX_RES * 5 / 6
#define TRAK_MIN_Y      TRAK_MAXY_RES / 6
#define TRAK_MAX_Y      TRAK_MAXY_RES * 5 / 6

/***************************************************************************
    function prototypes
 ***************************************************************************/

static void         OnMove(HWND hWnd, int x, int y);
static void         OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
static void         OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags);
static void         OnRButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
static void         OnRButtonUp(HWND hWnd, int x, int y, UINT keyFlags);
static void         OnMButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
static void         OnMButtonUp(HWND hWnd, int x, int y, UINT keyFlags);

static int          Trak_init(options_type *options);
static void         Trak_exit(void);
static void         Trak_read(int player, int *deltax, int *deltay);
static int          Trak_pressed(int trakcode);
static BOOL         Trak_OnMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);

/***************************************************************************
    External variables
 ***************************************************************************/

struct OSDTrak  Trak = 
{
    { Trak_init },              /* init              */
    { Trak_exit },              /* exit              */
    { Trak_read },              /* trak_read         */
    { Trak_pressed },           /* trak_pressed      */

    { Trak_OnMessage }          /* OnMessage         */
};

/***************************************************************************
    Internal structures
 ***************************************************************************/

struct tTrak_private
{
    BOOL        m_bLButton;
    BOOL        m_bMButton;
    BOOL        m_bRButton;
    POINT       m_Pos;
    POINT       m_Large;

    POINT       m_ptCenter;
    RECT        m_Rect;
};

/***************************************************************************
    Internal variables
 ***************************************************************************/

static struct tTrak_private This;

/***************************************************************************
    External OSD functions  
 ***************************************************************************/

/*
    put here anything you need to do when the program is started. Return 0 if 
    initialization was successful, nonzero otherwise.
*/
static int Trak_init(options_type *options)
{
    POINT   pt;

    This.m_bLButton     = FALSE;
    This.m_bMButton     = FALSE;
    This.m_bRButton     = FALSE;
    This.m_Pos.x        = 0;
    This.m_Pos.y        = 0;
    This.m_Large.x      = 0;
    This.m_Large.y      = 0;
    This.m_ptCenter.x   = TRAK_CENTER_X;
    This.m_ptCenter.y   = TRAK_CENTER_Y;
    This.m_Rect.left    = TRAK_MIN_X;
    This.m_Rect.right   = TRAK_MAX_X;
    This.m_Rect.top     = TRAK_MIN_Y;
    This.m_Rect.bottom  = TRAK_MAX_Y;

    /* Adjust Rect so that it is within the window. */
    pt.x = 0;
    pt.y = 0;
    ClientToScreen(MAME32App.m_hWnd, &pt);

    OffsetRect(&This.m_Rect, pt.x, pt.y);
    This.m_ptCenter.x   += pt.x;
    This.m_ptCenter.y   += pt.y;

    ShowCursor(FALSE);
    return 0;
}

/*
    put here cleanup routines to be executed when the program is terminated.
*/
static void Trak_exit(void)
{
    ShowCursor(TRUE);
}

static void Trak_read(int player, int *deltax, int *deltay)
{
    if (player != 0)
    {
        *deltax = *deltay = 0;
        return;
    }

    GetCursorPos(&This.m_Pos);

    if (This.m_Rect.right < This.m_Pos.x)
    {
        This.m_Large.x++;
        This.m_Pos.x -= RECT_WIDTH(This.m_Rect);
        SetCursorPos(This.m_Pos.x, This.m_Pos.y);
    }
    else
    if (This.m_Pos.x < This.m_Rect.left)
    {
        This.m_Large.x--;
        This.m_Pos.x += RECT_WIDTH(This.m_Rect);
        SetCursorPos(This.m_Pos.x, This.m_Pos.y);
    }

    if (This.m_Rect.bottom < This.m_Pos.y)
    {
        This.m_Large.y++;
        This.m_Pos.y -= RECT_HEIGHT(This.m_Rect);
        SetCursorPos(This.m_Pos.x, This.m_Pos.y);
    }
    else
    if (This.m_Pos.y < This.m_Rect.top)
    {
        This.m_Large.y--;
        This.m_Pos.y += RECT_HEIGHT(This.m_Rect);
        SetCursorPos(This.m_Pos.x, This.m_Pos.y);
    }

    *deltax = ((This.m_Large.x * RECT_WIDTH(This.m_Rect))  + This.m_Pos.x - This.m_ptCenter.x);
    *deltay = ((This.m_Large.y * RECT_HEIGHT(This.m_Rect)) + This.m_Pos.y - This.m_ptCenter.y);

    This.m_Large.x = 0;
    This.m_Large.y = 0;
    SetCursorPos(This.m_ptCenter.x, This.m_ptCenter.y);
}

static int Trak_pressed(int trakcode)
{
    switch (trakcode)
    {
        case OSD_TRAK_LEFT:
        case OSD_TRAK_RIGHT:
        case OSD_TRAK_UP:
        case OSD_TRAK_DOWN:
            return 0;
            break;

        case OSD_TRAK_FIRE1:
            return (This.m_bLButton == TRUE);
            break;

        case OSD_TRAK_FIRE2:
            return (This.m_bRButton == TRUE);
            break;

        case OSD_TRAK_FIRE3:
            return (This.m_bMButton == TRUE);
            break;

        case OSD_TRAK_FIRE:
            return ((This.m_bLButton == TRUE) ||
                    (This.m_bMButton == TRUE) ||
                    (This.m_bRButton == TRUE));
            break;

        default:
            return 0;
            break;
    }
}

/***************************************************************************
    Internal functions
 ***************************************************************************/

static BOOL Trak_OnMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    switch (Msg)
    {
        PEEK_MESSAGE(hWnd, WM_MOVE,         OnMove);
        PEEK_MESSAGE(hWnd, WM_LBUTTONDOWN,  OnLButtonDown);
        PEEK_MESSAGE(hWnd, WM_LBUTTONUP,    OnLButtonUp);
        PEEK_MESSAGE(hWnd, WM_RBUTTONDOWN,  OnRButtonDown);
        PEEK_MESSAGE(hWnd, WM_RBUTTONUP,    OnRButtonUp);
        PEEK_MESSAGE(hWnd, WM_MBUTTONDOWN,  OnMButtonDown);
        PEEK_MESSAGE(hWnd, WM_MBUTTONUP,    OnMButtonUp);
    }
    return FALSE;
}

static void OnMove(HWND hWnd, int x, int y)
{
    This.m_ptCenter.x   = TRAK_CENTER_X + x;
    This.m_ptCenter.y   = TRAK_CENTER_Y + y;
    This.m_Rect.left    = TRAK_MIN_X + x;
    This.m_Rect.right   = TRAK_MAX_X + x;
    This.m_Rect.top     = TRAK_MIN_Y + y;
    This.m_Rect.bottom  = TRAK_MAX_Y + y;
}

static void OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    This.m_bLButton = TRUE;
}

static void OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
    This.m_bLButton = FALSE;
}

static void OnRButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    This.m_bRButton = TRUE;
}

static void OnRButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
    This.m_bRButton = FALSE;
}

static void OnMButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    This.m_bMButton = TRUE;
}

static void OnMButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
    This.m_bMButton = FALSE;
}
