/***************************************************************************

    M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
    Win32 Portions Copyright (C) 1997-98 Michael Soderstrom and Chris Kirmse
    
    This file is part of MAME32, and may only be used, modified and
    distributed under the terms of the MAME license, in "readme.txt".
    By continuing to use, modify or distribute this file you indicate
    that you have read the license and understand and accept it fully.

 ***************************************************************************/

#ifndef __AUDIT_H__
#define __AUDIT_H__

void AuditDialog(HWND hParent);

/* For property sheet Game Audit tab */
void InitGameAudit(int gameIndex);
LRESULT CALLBACK GameAuditDialogProc(HWND hDlg,UINT Msg,WPARAM wParam,LPARAM lParam);

#endif
