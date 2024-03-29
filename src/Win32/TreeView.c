/***************************************************************************

   M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
   Win32 Portions Copyright (C) 1997-98 Michael Soderstrom and Chris Kirmse
    
   This file is part of MAME32, and may only be used, modified and
   distributed under the terms of the MAME license, in "readme.txt".
   By continuing to use, modify or distribute this file you indicate
   that you have read the license and understand and accept it fully.

***************************************************************************/
 
/***************************************************************************

  TreeView.c

  TreeView support routines - MSH 11/19/1998

***************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <windowsx.h>
#include "TreeView.h"
#include "driver.h"
#include "resource.h"
#include "Screenshot.h"
#include "Properties.h"
#include "Options.h"

#include <stdio.h>  /* for sprintf */
#include <stdlib.h> /* For malloc and free */
#include <commctrl.h>

/////
#if _MSC_VER < 1200

//#if 1
#define LPTV_ITEMW              LPTVITEMW
#define LPTV_ITEMA              LPTVITEMA
#define TV_ITEMW                TVITEMW
#define TV_ITEMA                TVITEMA
//#else
//#define tagTVITEMA             _TV_ITEMA
//#define    TVITEMA              TV_ITEMA
//#define  LPTVITEMA            LPTV_ITEMA
//#define tagTVITEMW             _TV_ITEMW
//#define    TVITEMW              TV_ITEMW
//#define  LPTVITEMW            LPTV_ITEMW
//#endif

#define LPTV_ITEM               LPTVITEM
#define TV_ITEM                 TVITEM
#define TVITEM TVITEMA

typedef struct tagTVITEMA {
    UINT      mask;
    HTREEITEM hItem;
    UINT      state;
    UINT      stateMask;
    LPSTR     pszText;
    int       cchTextMax;
    int       iImage;
    int       iSelectedImage;
    int       cChildren;
    LPARAM    lParam;
} TVITEMA, *LPTVITEMA;

typedef struct tagTVITEMW {
    UINT      mask;
    HTREEITEM hItem;
    UINT      state;
    UINT      stateMask;
    LPWSTR    pszText;
    int       cchTextMax;
    int       iImage;
    int       iSelectedImage;
    int       cChildren;
    LPARAM    lParam;
} TVITEMW, *LPTVITEMW;



#define _WIN32_IE 0x300
#if (_WIN32_IE >= 0x0300)
#define LPTV_INSERTSTRUCTA      LPTVINSERTSTRUCTA
#define LPTV_INSERTSTRUCTW      LPTVINSERTSTRUCTW
#define TV_INSERTSTRUCTA        TVINSERTSTRUCTA
#define TV_INSERTSTRUCTW        TVINSERTSTRUCTW
#else
#define tagTVINSERTSTRUCTA     _TV_INSERTSTRUCTA
#define    TVINSERTSTRUCTA      TV_INSERTSTRUCTA
#define  LPTVINSERTSTRUCTA    LPTV_INSERTSTRUCTA
#define tagTVINSERTSTRUCTW     _TV_INSERTSTRUCTW
#define    TVINSERTSTRUCTW      TV_INSERTSTRUCTW
#define  LPTVINSERTSTRUCTW    LPTV_INSERTSTRUCTW
#endif

#define TV_INSERTSTRUCT         TVINSERTSTRUCT
#define LPTV_INSERTSTRUCT       LPTVINSERTSTRUCT


#define TVINSERTSTRUCTA_V1_SIZE CCSIZEOF_STRUCT(TVINSERTSTRUCTA, item)
#define TVINSERTSTRUCTW_V1_SIZE CCSIZEOF_STRUCT(TVINSERTSTRUCTW, item)

typedef struct tagTVINSERTSTRUCTA {
    HTREEITEM hParent;
    HTREEITEM hInsertAfter;
#if (_WIN32_IE >= 0x0400)
    union
    {
        TVITEMEXA itemex;
        TV_ITEMA  item;
    } DUMMYUNIONNAME;
#else
    TV_ITEMA item;
#endif
} TVINSERTSTRUCTA, *LPTVINSERTSTRUCTA;

typedef struct tagTVINSERTSTRUCTW {
    HTREEITEM hParent;
    HTREEITEM hInsertAfter;
#if (_WIN32_IE >= 0x0400)
    union
    {
        TVITEMEXW itemex;
        TV_ITEMW  item;
    } DUMMYUNIONNAME;
#else
    TV_ITEMW item;
#endif
} TVINSERTSTRUCTW, *LPTVINSERTSTRUCTW;

#ifdef UNICODE
#define  TVINSERTSTRUCT         TVINSERTSTRUCTW
#define  LPTVINSERTSTRUCT       LPTVINSERTSTRUCTW
#define TVINSERTSTRUCT_V1_SIZE TVINSERTSTRUCTW_V1_SIZE
#else
#define  TVINSERTSTRUCT         TVINSERTSTRUCTA
#define  LPTVINSERTSTRUCT       LPTVINSERTSTRUCTA
#define TVINSERTSTRUCT_V1_SIZE TVINSERTSTRUCTA_V1_SIZE
#endif

#define TVM_INSERTITEMA         (TV_FIRST + 0)
#define TVM_INSERTITEMW         (TV_FIRST + 50)
#ifdef UNICODE
#define  TVM_INSERTITEM         TVM_INSERTITEMW
#else
#define  TVM_INSERTITEM         TVM_INSERTITEMA
#endif
#endif

////


/***************************************************************************
    public structures
 ***************************************************************************/

enum {
    ICON_FOLDER_OPEN = 0,
    ICON_FOLDER,
    ICON_FOLDER_AVAILABLE,
    ICON_FOLDER_CUSTOM,
    ICON_FOLDER_MANUFACTURER,
    ICON_FOLDER_UNAVAILABLE,
    ICON_FOLDER_YEAR,
    ICON_MANUFACTURER,
    ICON_WORKING,
    ICON_NONWORKING,
    ICON_YEAR,
    ICON_STEREO,
    ICON_NEOGEO
};

/* Name used for user-defined custom icons */
/* external *.ico file to look for. */
static char treeIconNames[][15] = {
    "foldopen",
    "folder",
    "foldavail",
    "custom",
    "foldmanu",
    "foldunav",
    "foldyear",
    "manufact",
    "working",
    "nonwork",
    "year",
    "sound",
    "neo-geo"    
};

typedef struct {
    LPSTR       m_lpTitle;      /* Folder Title */
    FOLDERTYPE  m_nFolderType;  /* Folder Type */
    UINT        m_nFolderId;    /* ID */
    UINT        m_nParent;      /* Parent Folder ID */
    DWORD       m_dwFlags;      /* Flags - Customizable and Filters */
    UINT        m_nIconId;      /* Icon index into the ImageList */
} FOLDERDATA, *LPFOLDERDATA;

FOLDERDATA folderData[] =
{
    {"All Games",   IS_ROOT,  FOLDER_ALLGAMES,    FOLDER_NONE,  0,        ICON_FOLDER},
    {"Available",   IS_ROOT,  FOLDER_AVAILABLE,   FOLDER_NONE,  0,        ICON_FOLDER_AVAILABLE},
#if defined(LMR)
    {"Unavailable", IS_ROOT,  FOLDER_UNAVAILABLE, FOLDER_NONE,  0,        ICON_FOLDER_UNAVAILABLE},
#endif
    {"Neo-Geo",     IS_ROOT,  FOLDER_NEOGEO,      FOLDER_NONE,  0,        ICON_NEOGEO},
    {"Manufacturer",IS_ROOT,  FOLDER_MANUFACTURER,FOLDER_NONE,  0,        ICON_FOLDER_MANUFACTURER},
    {"Year",        IS_ROOT,  FOLDER_YEAR,        FOLDER_NONE,  0,        ICON_FOLDER_YEAR},
    {"Working",     IS_ROOT,  FOLDER_WORKING,     FOLDER_NONE,  0,        ICON_WORKING},
    {"Non-Working", IS_ROOT,  FOLDER_NONWORKING,  FOLDER_NONE,  0,        ICON_NONWORKING},
    {"Custom",      IS_ROOT,  FOLDER_CUSTOM,      FOLDER_NONE,  F_CUSTOM, ICON_FOLDER_CUSTOM},
    {"Played",      IS_FOLDER,FOLDER_PLAYED,      FOLDER_CUSTOM,F_CUSTOM, ICON_FOLDER_CUSTOM},
    {"Favorites",   IS_FOLDER,FOLDER_FAVORITE,    FOLDER_CUSTOM,F_CUSTOM, ICON_FOLDER_CUSTOM},
    {"Originals",   IS_ROOT,  FOLDER_ORIGINAL,    FOLDER_NONE,  0,        ICON_FOLDER},
    {"Clones",      IS_ROOT,  FOLDER_CLONES,      FOLDER_NONE,  0,        ICON_FOLDER},
    {"Raster",      IS_ROOT,  FOLDER_RASTER,      FOLDER_NONE,  0,        ICON_FOLDER},
    {"Vector",      IS_ROOT,  FOLDER_VECTOR,      FOLDER_NONE,  0,        ICON_FOLDER},
    {"Trackball",   IS_ROOT,  FOLDER_TRACKBALL,   FOLDER_NONE,  0,        ICON_FOLDER},
    {"Stereo",      IS_ROOT,  FOLDER_STEREO,      FOLDER_NONE,  0,        ICON_STEREO}
};

#define NUM_FOLDERS (sizeof(folderData) / sizeof(FOLDERDATA))

/***************************************************************************
    private variables
 ***************************************************************************/

static TREEFOLDER **treeFolders = 0;
static UINT         numFolders  = 0;        /* Number of folder in the folder array */
static UINT         folderArrayLength = 0;  /* Size of the folder array */
static LPTREEFOLDER lpCurrentFolder = 0;    /* Currently selected folder */
static UINT         nCurrentFolder = 0;     /* Current folder ID */
static WNDPROC      g_lpTreeWndProc = 0;    /* for subclassing the TreeView */
static HIMAGELIST   hTreeSmall = 0;         /* TreeView Image list of icons */

/***************************************************************************
    private function prototypes
 ***************************************************************************/

static BOOL         CreateTreeIcons(HWND hWnd);
static char *       FixString(char *s);
static char *       LicenseManufacturer(char *s);
static BOOL         AddFolder(LPTREEFOLDER lpFolder);
static BOOL         RemoveFolder(LPTREEFOLDER lpFolder);
static LPTREEFOLDER NewFolder(LPSTR lpTitle, FOLDERTYPE nFolderType,
                              UINT nFolderId, int nParent, UINT nIconId,
                              DWORD dwFlags, UINT nBits);
static void         DeleteFolder(LPTREEFOLDER lpFolder);
static void         BuildTreeFolders(HWND hWnd);

static LRESULT CALLBACK TreeWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/***************************************************************************
    public functions
 ***************************************************************************/

/* De-allocate all folder memory */
void FreeFolders(void)
{
    int             i = 0;
    LPFOLDERDATA    fData = 0;

    if (treeFolders != 0)
    {
        for (i = numFolders - 1; i >= 0; i--)
        {
            DeleteFolder(treeFolders[i]);
            treeFolders[i] = 0;
            numFolders--;
        }
        free(treeFolders);
        treeFolders = 0;
    }
    numFolders = 0;
}

/* Reset folder filters */
void ResetFilters(void)
{
    int             i = 0;
    LPFOLDERDATA    fData = 0;

    if (treeFolders != 0)
    {
        for (i = 0; i < (int)numFolders; i++)
        {
            treeFolders[i]->m_dwFlags &= ~F_MASK;
            SetFolderFlags(treeFolders[i]->m_lpTitle, 0);
        }
    }
}

/* Can be called to re-initialize the array of treeFolders */
BOOL InitFolders(UINT nGames)
{
    int             i = 0;
    DWORD           dwFolderFlags;
    LPFOLDERDATA    fData = 0;

    if (treeFolders != 0)
    {
        for (i = numFolders - 1; i >= 0; i--)
        {
            DeleteFolder(treeFolders[i]);
            treeFolders[i] = 0;
            numFolders--;
        }
    }
    numFolders = 0;
    if (folderArrayLength == 0)
    {
        folderArrayLength = 200;
        treeFolders = (TREEFOLDER **)malloc(sizeof(TREEFOLDER **) * folderArrayLength);
        if (!treeFolders)
        {
            folderArrayLength = 0;
            return 0;
        }
        else
        {
            memset(treeFolders,'\0', sizeof(TREEFOLDER **) * folderArrayLength);
        }
    }
    for (i = 0; i < NUM_FOLDERS; i++)
    {
        fData = &folderData[i];
        /* OR in the saved folder flags */
        dwFolderFlags = fData->m_dwFlags | GetFolderFlags(fData->m_lpTitle);
        /* create the folder */
        treeFolders[numFolders] = NewFolder(fData->m_lpTitle, fData->m_nFolderType,
                                            fData->m_nFolderId, fData->m_nParent,
                                            fData->m_nIconId, dwFolderFlags, nGames);
        if (treeFolders[numFolders])
            numFolders++;
    }

    /* insure we have a current folder */
    SetCurrentFolder(treeFolders[0]);

    InitGames(nGames);

    return TRUE;
}

void InitTree(HWND hWnd, UINT nGames)
{
    if (numFolders == 0)
        InitFolders(nGames);

    CreateTreeIcons( hWnd );
    BuildTreeFolders( hWnd );
}

void SetCurrentFolder(LPTREEFOLDER lpFolder)
{
    lpCurrentFolder = (lpFolder == 0) ? treeFolders[0] : lpFolder;
    nCurrentFolder = (lpCurrentFolder) ? lpCurrentFolder->m_nFolderId : 0;
}

LPTREEFOLDER GetCurrentFolder()
{
    return lpCurrentFolder;
}

UINT GetCurrentFolderID()
{
    return nCurrentFolder;
}

LPTREEFOLDER GetFolder(UINT nFolder)
{
    return (nFolder < numFolders) ? treeFolders[nFolder] : (LPTREEFOLDER)0;
}

LPTREEFOLDER GetFolderByID(UINT nID)
{
    UINT i;

    for (i = 0; i < numFolders; i++)
    {
        if (treeFolders[i]->m_nFolderId == nID)
        {
            return treeFolders[i];
        }
    }

    return (LPTREEFOLDER)0;
}

void AddGame(LPTREEFOLDER lpFolder, UINT nGame)
{
    SetBit(lpFolder->m_lpGameBits, nGame);
}

void RemoveGame(LPTREEFOLDER lpFolder, UINT nGame)
{
    ClearBit(lpFolder->m_lpGameBits, nGame);
}

int FindGame(LPTREEFOLDER lpFolder, int nGame)
{
    return FindBit(lpFolder->m_lpGameBits, nGame, TRUE);
}

/* #define BUILD_LIST */

/* Called to re-associate games with folders */
void InitGames(UINT nGames)
{
    static BOOL firstTime = TRUE;
    UINT    i, jj;
    UINT    tmpNumFolders;
    BOOL *  done = NULL;
    game_data_type * gameData = GetGameData();
    UINT    nFolderId = FOLDER_END;
#ifdef BUILD_LIST
    FILE *fp = fopen("manufac.txt","wt");
    int once = 1;
#endif

    if (! numFolders)
        return;

    tmpNumFolders = numFolders;

    for (i = 0; i < tmpNumFolders; i++)
    {
        LPTREEFOLDER lpFolder = treeFolders[i];
        switch(lpFolder->m_nFolderId)
        {
        case FOLDER_ALLGAMES:
            // Clear the bitmask
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                AddGame(lpFolder, jj);
            }
            break;

        case FOLDER_AVAILABLE:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (GetHasRoms(jj) == TRUE)
                    AddGame(lpFolder, jj);
            }
            break;

        case FOLDER_UNAVAILABLE:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (GetHasRoms(jj) == FALSE)
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_NEOGEO:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (!gameData[jj].in_list || gameData[jj].neogeo_clone)
                    AddGame(lpFolder, jj);
            }
            break;

        case FOLDER_YEAR:
            if (!firstTime)
                break;

            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            done = (LPBOOL)malloc(sizeof(BOOL) * nGames);
            memset(done, '\0', sizeof(BOOL) * nGames);
            for (jj = 0; jj < nGames; jj++)
            {
                LPTREEFOLDER lpTemp = 0;
                UINT j;
                DWORD dwFolderFlags;
                char cTmp[40];

                if (done[jj])
                    continue;

                strcpy(cTmp,FixString((char *)drivers[jj]->year));
                if (cTmp[4] == '?')
                    cTmp[4] = '\0';
                dwFolderFlags = GetFolderFlags(cTmp);
                lpTemp = NewFolder(cTmp, IS_FOLDER, nFolderId++,
                                   FOLDER_YEAR, ICON_YEAR, 0, nGames);
                AddFolder(lpTemp);
                done[jj] = TRUE;
                
                for (j = 0; j < nGames; j++)
                {
                    if (strncmp(cTmp, FixString((char *)drivers[j]->year), 4) == 0)
                    {
                        done[j] = TRUE;
                        AddGame(lpTemp, j);
                    }
                }
            }
            free(done);
            break;

        case FOLDER_MANUFACTURER:
            if (!firstTime)
                break;

            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            done = (LPBOOL)malloc(sizeof(BOOL) * (nGames + 1));
            memset(done, '\0', sizeof(BOOL) * (nGames + 1));

            for (jj = 0; jj < nGames + 1; jj++)
            {
                LPTREEFOLDER lpTemp = 0;
                UINT j;
                DWORD dwFolderFlags;
                char cTmp[40];

#ifdef BUILD_LIST
                if (jj < nGames)
                    fprintf(fp, "STRING\t%s\n", (char *)drivers[jj]->manufacturer);
#endif
                if (done[jj])
                    continue;

                if (jj == nGames)
                    strcpy(cTmp, "Romstar");
                else
                    strcpy(cTmp,FixString((char *)drivers[jj]->manufacturer));  
                                
                if (cTmp[0] == '\0')
                    continue;

#ifdef BUILD_LIST
                fprintf(fp, "FOLDER\t%s\n", cTmp);
#endif
                dwFolderFlags = GetFolderFlags(cTmp);

                lpTemp = NewFolder(cTmp, IS_FOLDER, nFolderId++,
                                   FOLDER_MANUFACTURER, ICON_MANUFACTURER, 0, nGames);
                AddFolder(lpTemp);
                done[jj] = TRUE;
                
                for (j = 0; j < nGames; j++)
                {
                    char *tmp = LicenseManufacturer((char *)drivers[j]->manufacturer);
#ifdef BUILD_LIST
                    if (tmp != NULL && once)
                        fprintf(fp, "LICENSE\t%s\n", tmp);
                    if (once)
                        fprintf(fp, "NORMAL\t%s\n", FixString((char *)drivers[j]->manufacturer));
#endif
                    if (tmp != NULL && strncmp(cTmp, tmp, 5) == 0)
                    {
                        AddGame(lpTemp, j);
                    }

                    if (strncmp(cTmp, FixString((char *)drivers[j]->manufacturer), 5) == 0)
                    {
                        done[j] = TRUE;
                        AddGame(lpTemp, j);
                    }
                }
#ifdef BUILD_LIST
                once = 0;
#endif
            }
#ifdef BUILD_LIST
            fclose(fp);
#endif         
            free(done);
            break;

        case FOLDER_WORKING:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (!(drivers[jj]->flags & GAME_NOT_WORKING))
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_NONWORKING:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                /* Mark wrong colors as non-working */
                if (drivers[jj]->flags & GAME_NOT_WORKING)
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_ORIGINAL:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (!drivers[jj]->clone_of || !gameData[jj].in_list)
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_CLONES:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (drivers[jj]->clone_of && gameData[jj].in_list)
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_RASTER:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if ((drivers[jj]->drv->video_attributes  & VIDEO_TYPE_VECTOR) == 0)
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_VECTOR:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (drivers[jj]->drv->video_attributes  & VIDEO_TYPE_VECTOR)
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_TRACKBALL:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (GameUsesTrackball(jj))
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_PLAYED:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (GetPlayCount(jj))
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_FAVORITE:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (GetIsFavorite(jj))
                    AddGame(lpFolder, jj);
            }
            break;
        case FOLDER_STEREO:
            SetAllBits( lpFolder->m_lpGameBits, FALSE);
            for (jj = 0; jj < nGames; jj++)
            {
                if (drivers[jj]->drv->sound_attributes & SOUND_SUPPORTS_STEREO)
                    AddGame(lpFolder, jj);
            }
            break;
        }
    }
    firstTime = FALSE;
}

/* Subclass the Treeview Header */
void Tree_Initialize(HWND hWnd)
{
    /* this will subclass the listview (where WM_DRAWITEM gets sent for
       the header control) */
    g_lpTreeWndProc = (WNDPROC)GetWindowLong(hWnd, GWL_WNDPROC);
    SetWindowLong(hWnd, GWL_WNDPROC, (LONG)TreeWndProc);
}

/* Used to build the GameList */
BOOL GameFiltered(int nGame, DWORD dwMask)
{
    BOOL vector;
    game_data_type *gameData = GetGameData();

    /* Are there filters set on this folder? */
    if ((dwMask & F_MASK) == 0)
        return FALSE;

    /* Filter out clones? */
    if (dwMask & F_CLONES && drivers[nGame]->clone_of &&
        gameData[nGame].in_list)
        return TRUE;

    /* Filter non working games */
    if (dwMask & F_NONWORKING && drivers[nGame]->flags & GAME_NOT_WORKING)
        return TRUE;

    /* Filter neo-geo games */
    if (dwMask & F_NEOGEO &&
        (!gameData[nGame].in_list || gameData[nGame].neogeo_clone))
        return TRUE;

    /* Filter unavailable games */
    if (dwMask & F_UNAVAILABLE && GetHasRoms(nGame) == FALSE)
        return TRUE;

    vector = drivers[nGame]->drv->video_attributes  & VIDEO_TYPE_VECTOR;

    /* Filter vector games */
    if (dwMask & F_VECTOR && vector) 
        return TRUE;

    /* Filter Raster games */
    if (dwMask & F_RASTER && !vector)
        return TRUE;

    /* FIlter original games */
    if (dwMask & F_ORIGINALS &&
        (!drivers[nGame]->clone_of || !gameData[nGame].in_list))
        return TRUE;

    /* Filter working games */
    if (dwMask & F_WORKING && !(drivers[nGame]->flags & GAME_NOT_WORKING))
        return TRUE;

#if defined(LMR)
    /* filter available games */
    if (dwMask & F_AVAILABLE && GetHasRoms(nGame) == TRUE)
        return TRUE;
#endif

    return FALSE;
}

LRESULT CALLBACK ResetDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    BOOL resetFilters = FALSE;
    BOOL resetGames = FALSE;
    BOOL resetUI = FALSE;
    BOOL resetDefaults = FALSE;

    switch (Msg)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_HELP:
        /* User clicked the ? from the upper right on a control */
        WinHelp(((LPHELPINFO) lParam)->hItemHandle, "mame32.hlp", HELP_WM_HELP, GetHelpIDs());
        break;

    case WM_CONTEXTMENU: 
        WinHelp((HWND) wParam, "mame32.hlp", HELP_CONTEXTMENU, GetHelpIDs());
        break; 

    case WM_COMMAND :
        switch (GET_WM_COMMAND_ID(wParam, lParam))
        {
        case IDOK :
            resetFilters = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_FILTERS));
            resetGames = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_GAMES));
            resetDefaults = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_DEFAULT));
            resetUI = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_UI));
            if (resetFilters || resetGames || resetUI || resetDefaults)
            {
                char temp[200];
                strcpy(temp, "MAME32 will now reset the selected\n");
                strcat(temp, "items to the original, installation\n");
                strcat(temp, "settings then exit.\n\n");
                strcat(temp, "The new settings will take effect\n");
                strcat(temp, "the next time MAME32 is run.\n\n");
                strcat(temp, "Do you wish to continue?");
                if (MessageBox(hDlg, temp, "Restore Settings", IDOK) == IDOK)
                {
                    if (resetFilters)
                        ResetFilters();
                    
                    if (resetUI)
                        ResetGUI();
                    
                    if (resetDefaults)
                        ResetGameDefaults();
                    
                    if (resetGames)
                        ResetAllGameOptions();
                    
                    EndDialog(hDlg, 1);
                    return TRUE;
                }
            }
            /* Fall through if no options were selected 
             * or the user hit cancel in the popup dialog.
             */
        case IDCANCEL :
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    return 0;
}

LRESULT CALLBACK StartupDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_INITDIALOG:
        Button_SetCheck(GetDlgItem(hDlg, IDC_START_GAME_CHECK),     GetGameCheck());
        Button_SetCheck(GetDlgItem(hDlg, IDC_START_VERSION_WARN),   GetVersionCheck());
        Button_SetCheck(GetDlgItem(hDlg, IDC_START_MMX_CHECK),      GetMMXCheck());
        return TRUE;

    case WM_HELP:
        /* User clicked the ? from the upper right on a control */
        WinHelp(((LPHELPINFO) lParam)->hItemHandle, "mame32.hlp", HELP_WM_HELP, GetHelpIDs());
        break;

    case WM_CONTEXTMENU: 
        WinHelp((HWND) wParam, "mame32.hlp", HELP_CONTEXTMENU, GetHelpIDs());
        break; 

    case WM_COMMAND :
        switch (GET_WM_COMMAND_ID(wParam, lParam))
        {
        case IDOK :
            SetGameCheck(Button_GetCheck(GetDlgItem(hDlg, IDC_START_GAME_CHECK)));
            SetVersionCheck(Button_GetCheck(GetDlgItem(hDlg, IDC_START_VERSION_WARN)));
            SetMMXCheck(Button_GetCheck(GetDlgItem(hDlg, IDC_START_MMX_CHECK)));
            /* Fall through */

        case IDCANCEL :
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    return 0;
}

/***************************************************************************
    private functions
 ***************************************************************************/

/* Add a folder to the list.  Does not allocate */
static BOOL AddFolder(LPTREEFOLDER lpFolder)
{
    UINT i = 0;

    if (!lpFolder)
        return FALSE;

    if (numFolders + 1 >= folderArrayLength)
    {
        TREEFOLDER ** tmpFolders = treeFolders;

        i = folderArrayLength;

        folderArrayLength += 10;

        treeFolders = (TREEFOLDER **)malloc(sizeof(TREEFOLDER **) * folderArrayLength);
        if (!treeFolders)
        {
            folderArrayLength = i;
            treeFolders = tmpFolders;
            return FALSE;
        }
        for (i = 0; i < numFolders; i++)
        {
            treeFolders[i] = tmpFolders[i];
        }
        free(tmpFolders);
        for (i = numFolders; i < folderArrayLength; i++)
        {
            treeFolders[i] = 0;
        }
    }

    treeFolders[numFolders] = lpFolder;
    numFolders++;

    return TRUE;
}

/* Remove a folder from the list, but do NOT delete it */
static BOOL RemoveFolder(LPTREEFOLDER lpFolder)
{
    int     found = -1;
    UINT    i = 0;

    /* Find the folder */
    for (i = 0; i < numFolders && found == -1; i++)
    {
        if (lpFolder == treeFolders[i])
        {
            found = i;
        }
    }
    if (found != -1)
    {
        numFolders--;
        treeFolders[i] = 0; /* In case we removed the last folder */

        /* Move folders up in the array if needed */
        for (i = (UINT)found; (UINT)found < numFolders; i++)
            treeFolders[i] = treeFolders[i+1];
    }
    return (found != -1) ? TRUE : FALSE;
}

/* Allocate and initialize a NEW TREEFOLDER */
static LPTREEFOLDER NewFolder(LPSTR lpTitle, FOLDERTYPE nFolderType,
                       UINT nFolderId, int nParent, UINT nIconId,
                       DWORD dwFlags, UINT nBits)
{
    LPTREEFOLDER lpFolder = (LPTREEFOLDER)malloc(sizeof(TREEFOLDER));
    int numBits = (nBits < 8) ? 8 : nBits;

    if (lpFolder)
    {
        memset(lpFolder, '\0', sizeof (TREEFOLDER));
        if (lpFolder->m_lpTitle = (LPSTR)malloc(strlen(lpTitle) + 1))
        {
            strcpy((char *)lpFolder->m_lpTitle,lpTitle);
            lpFolder->m_lpGameBits = 0;
        }
        else
        {
            free(lpFolder);
            return (LPTREEFOLDER)0;
        }
        if ((lpFolder->m_lpGameBits = NewBits(numBits)) == NULL)
        {
            free(lpFolder->m_lpTitle);
            free(lpFolder);
            return (LPTREEFOLDER)0;
        }
        lpFolder->m_nFolderId = nFolderId;
        lpFolder->m_nFolderType = nFolderType;
        lpFolder->m_nParent = nParent;
        lpFolder->m_nIconId = nIconId;
        lpFolder->m_dwFlags = dwFlags;
    }
    return lpFolder;
}

/* Deallocate the passed in LPTREEFOLDER */
static void DeleteFolder(LPTREEFOLDER lpFolder)
{
    if (lpFolder)
    {
        if (lpFolder->m_lpGameBits)
        {
            DeleteBits(lpFolder->m_lpGameBits);
            lpFolder->m_lpGameBits = 0;
        }
        free(lpFolder->m_lpTitle);
        lpFolder->m_lpTitle = 0;
        free(lpFolder);
        lpFolder = 0;
    }
}

/* Can be called to reinitialize the tree control */
static void BuildTreeFolders( HWND hWnd )
{
    UINT            i;
    TVINSERTSTRUCT  tvs;
    TVITEM          tvi;

    tvs.hInsertAfter = TVI_SORT;

    if (TreeView_GetCount(hWnd) > 0)
    {
        TreeView_DeleteAllItems(hWnd);
    }

    for (i = 0; i < numFolders; i++)
    {
        LPTREEFOLDER lpFolder = treeFolders[i];

        if (lpFolder->m_nFolderType == IS_ROOT)
        {
            HTREEITEM   hti;
            UINT        jj;

            tvi.mask    = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
            tvs.hParent = TVI_ROOT;
            tvi.pszText = lpFolder->m_lpTitle;
            tvi.lParam  = (LPARAM)lpFolder;
            tvi.iImage  = lpFolder->m_nIconId;
            tvi.iSelectedImage = 0;

            tvs.item = tvi;

            // Add root branch
            hti =   TreeView_InsertItem( hWnd, &tvs );

            if (i == 0 || lpFolder->m_nFolderId == GetSavedFolderID())
                TreeView_SelectItem( hWnd, hti );

            for (jj = 0; jj < numFolders; jj++)
            {
                LPTREEFOLDER tmp = treeFolders[jj];

                if (tmp->m_nParent != FOLDER_NONE &&
                    tmp->m_nParent == lpFolder->m_nFolderId)
                {
                    HTREEITEM shti;

                    tvi.mask    = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
                    tvs.hParent = hti;
                    tvi.iImage  = tmp->m_nIconId;
                    tvi.iSelectedImage = 0;
                    tvi.pszText = tmp->m_lpTitle;
                    tvi.lParam  = (LPARAM)tmp;
    
                    tvs.item    = tvi;
                    // Add it to this tree branch
                    shti = TreeView_InsertItem( hWnd, &tvs );
                    if (tmp->m_nFolderId == GetSavedFolderID())
                        TreeView_SelectItem( hWnd, shti );
                }
            }
        }
    }
}

/* Make a reasonable name out of the one found in the driver array */
static char * FixString(char *s)
{
    static char tmp[40];
    char *      ptr;

    if (*s == '?' || *s == '<')
        strcpy(tmp,"<unknown>");
    else
    {
        int i = 0;

        ptr = s;
        while(*ptr)
        {
            if ((*ptr == ' ') &&
                (ptr[1] == '(' || ptr[1] == '/') || (ptr[1] == '+'))
                break;
            
            if (*ptr == '[')
                ptr++;
            else if (*ptr == ']' || *ptr == '/')
                break;
            else
                tmp[i++] = *ptr++;
            
        }
        tmp[i] = '\0';
    }
    return tmp;
}

/* Make a reasonable name out of the one found in the driver array */
static char * LicenseManufacturer(char *s)
{
    static char tmp[40];
    char *      ptr, *t;

    t = tmp;

    if ((ptr = (char *)strchr(s,'(')) != NULL)
    {
        ptr++;
        if (strncmp(ptr, "licensed from ", 14) == 0)
            ptr += 14;
        
        while (*ptr != ')')
        {
            if (*ptr == ' ' && strncmp(ptr, " license", 8) == 0)
                break;

            *t++ = *ptr++;
        }
        
        *t = '\0';
        return tmp;
    }
    return NULL;
}

/* create iconlist and Treeview control */
static BOOL CreateTreeIcons(HWND hWnd)
{
    HICON   hIcon;
    INT     i;
    HINSTANCE hInst = GetModuleHandle(0);

    hTreeSmall = ImageList_Create (16, 16, ILC_COLORDDB | ILC_MASK, 11, 11);

    for (i = IDI_FOLDER_OPEN; i <= IDI_NEOGEO; i++)
    {
        if ((hIcon = LoadIconFromFile(treeIconNames[i - IDI_FOLDER_OPEN])) == 0)
            hIcon = LoadIcon (hInst, MAKEINTRESOURCE(i));

        if (ImageList_AddIcon (hTreeSmall, hIcon) == -1)
            return FALSE;
    }

    // Be sure that all the small icons were added.
    if (ImageList_GetImageCount (hTreeSmall) < 11)
        return FALSE;

    // Associate the image lists with the list view control.
    TreeView_SetImageList (hWnd, hTreeSmall, LVSIL_NORMAL);
  
    return TRUE;
}

static BOOL TreeCtrlOnPaint(HWND hWnd, UINT uMsg,  WPARAM wParam,  LPARAM lParam)
{

    PAINTSTRUCT ps;
    HDC         hDC = GetDC(hWnd);
    RECT        rcClip, rcClient;
    HDC         memDC;
    HBITMAP     bitmap;
    HBITMAP     hOldBitmap;

    if (hBitmap == 0)
        return 1;

    BeginPaint(hWnd, &ps);

    GetClipBox(hDC, &rcClip );
    GetClientRect(hWnd, &rcClient);
    
    // Create a compatible memory DC
    memDC = CreateCompatibleDC( hDC );

    // Select a compatible bitmap into the memory DC
    bitmap = CreateCompatibleBitmap( hDC, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top );
    hOldBitmap = SelectObject( memDC, bitmap );
    
    // First let the control do its default drawing.
    CallWindowProc(g_lpTreeWndProc, hWnd, uMsg, (WPARAM)memDC, 0);
    // DefWindowProc( hWnd, WM_PAINT, (WPARAM)memDC, 0 );

    // Draw bitmap in the background if one has been set
    if( hBitmap != NULL )
    {
        HPALETTE    hPAL;        
        HDC maskDC;
        HBITMAP maskBitmap;
        HDC tempDC;
        HDC imageDC;
        HBITMAP bmpImage;
        HBITMAP hOldBmpBitmap;
        HBITMAP hOldMaskBitmap;
        HBITMAP hOldHBitmap;
        int i, j;
        RECT rcRoot;

        // Now create a mask
        maskDC = CreateCompatibleDC(hDC);   
        
        // Create monochrome bitmap for the mask
        maskBitmap = CreateBitmap( rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, 
                        1, 1, NULL );
        hOldMaskBitmap = SelectObject( maskDC, maskBitmap );
        SetBkColor( memDC, GetSysColor( COLOR_WINDOW ) );

        // Create the mask from the memory DC
        BitBlt( maskDC, 0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, memDC, 
                    rcClient.left, rcClient.top, SRCCOPY );


        tempDC = CreateCompatibleDC(hDC);
        hOldHBitmap = SelectObject(tempDC, hBitmap );

        imageDC = CreateCompatibleDC( hDC );
        bmpImage = CreateCompatibleBitmap( hDC, rcClient.right - rcClient.left, 
                        rcClient.bottom - rcClient.top );
        hOldBmpBitmap = SelectObject(imageDC, bmpImage );

        hPAL = (! hPALbg) ? CreateHalftonePalette(hDC) : hPALbg;

        if( GetDeviceCaps(hDC, RASTERCAPS) & RC_PALETTE && hPAL != NULL )
        {
            SelectPalette(hDC, hPAL, FALSE );
            RealizePalette(hDC);
            SelectPalette(imageDC, hPAL, FALSE );
        }
        
        // Get x and y offset
        TreeView_GetItemRect( hWnd, TreeView_GetRoot(hWnd), &rcRoot, FALSE );
        rcRoot.left = -GetScrollPos( hWnd, SB_HORZ );

        // Draw bitmap in tiled manner to imageDC
        for( i = rcRoot.left; i < rcClient.right; i += bmDesc.bmWidth )
            for( j = rcRoot.top; j < rcClient.bottom; j += bmDesc.bmHeight )
                BitBlt(imageDC,  i, j, bmDesc.bmWidth, bmDesc.bmHeight, tempDC, 
                            0, 0, SRCCOPY );

        // Set the background in memDC to black. Using SRCPAINT with black and any other
        // color results in the other color, thus making black the transparent color
        SetBkColor(memDC, RGB(0,0,0));
        SetTextColor(memDC, RGB(255,255,255));
        BitBlt(memDC, rcClip.left, rcClip.top, rcClip.right - rcClip.left,
                rcClip.bottom - rcClip.top,
                maskDC, rcClip.left, rcClip.top, SRCAND);

        // Set the foreground to black. See comment above.
        SetBkColor(imageDC, RGB(255,255,255));
        SetTextColor(imageDC, RGB(0,0,0));
        BitBlt(imageDC, rcClip.left, rcClip.top, rcClip.right - rcClip.left, 
                rcClip.bottom - rcClip.top, maskDC, 
                rcClip.left, rcClip.top, SRCAND);

        // Combine the foreground with the background
        BitBlt(imageDC, rcClip.left, rcClip.top, rcClip.right - rcClip.left,
                rcClip.bottom - rcClip.top, 
                memDC, rcClip.left, rcClip.top, SRCPAINT);
        // Draw the final image to the screen
        
        BitBlt(hDC, rcClip.left, rcClip.top, rcClip.right - rcClip.left,
                rcClip.bottom - rcClip.top, 
                imageDC, rcClip.left, rcClip.top, SRCCOPY );
        
        SelectObject(maskDC,  hOldMaskBitmap );
        SelectObject(tempDC,  hOldHBitmap );
        SelectObject(imageDC, hOldBmpBitmap );

        DeleteDC(maskDC);
        DeleteDC(imageDC);
        DeleteDC(tempDC);
        DeleteObject(bmpImage);
        DeleteObject(maskBitmap);
    }
    else
    {
        BitBlt(hDC,  rcClip.left, rcClip.top, rcClip.right - rcClip.left, 
                rcClip.bottom - rcClip.top,
                memDC, rcClip.left, rcClip.top, SRCCOPY );
    }
    SelectObject(memDC, hOldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
    ReleaseDC(hWnd, hDC);

    return 0;
}

/* Header code - Directional Arrows */
static LRESULT CALLBACK TreeWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_ERASEBKGND:
        if (hBitmap)
            return TRUE;
        break;

    case WM_PAINT:
        if (TreeCtrlOnPaint( hWnd, uMsg, wParam, lParam) == 0)
            return 0;
        break;
    }

    /* message not handled */
    return CallWindowProc(g_lpTreeWndProc, hWnd, uMsg, wParam, lParam);
}

/*
 * Filter code - should be moved to filter.c/filter.h
 * Added 01/09/99 - MSH <mhaaland@hypertech.com>
 */

/***************************************************************************
    private structures
 ***************************************************************************/

typedef struct {
    DWORD m_dwFolderID;     /* Folder ID */
    DWORD m_dwUnset;        /* Excluded filters */
    DWORD m_dwSet;          /* Implied filters */
} FILTER_RECORD, * LPFILTER_RECORD;

typedef struct {
    DWORD m_dwFilterType;   /* Filter value */
    DWORD m_dwCtrlID;       /* Control ID that represents it */
} FILTER_ITEM, *LPFILTER_ITEM;

/* List of folders with implied and excluded filters */
static FILTER_RECORD filterRecord[NUM_FOLDERS] = {
    {FOLDER_ALLGAMES,    0,             0               },
#if defined(LMR)
    {FOLDER_AVAILABLE,   F_AVAILABLE,   F_UNAVAILABLE   },
    {FOLDER_UNAVAILABLE, F_UNAVAILABLE, F_AVAILABLE     },
#else
    {FOLDER_AVAILABLE,   F_AVAILABLE,   0               },
#endif
    {FOLDER_NEOGEO,      F_NEOGEO,      0               },
    {FOLDER_MANUFACTURER,0,             0               },
    {FOLDER_YEAR,        0,             0               },
    {FOLDER_WORKING,     F_WORKING,     F_NONWORKING    },
    {FOLDER_NONWORKING,  F_NONWORKING,  F_WORKING       },
    {FOLDER_CUSTOM,      0,             0               },
    {FOLDER_PLAYED,      0,             0               },
    {FOLDER_FAVORITE,    0,             0               },
    {FOLDER_ORIGINAL,    F_ORIGINALS,   F_CLONES        },
    {FOLDER_CLONES,      F_CLONES,      F_ORIGINALS     },
    {FOLDER_RASTER,      F_RASTER,      F_VECTOR        },
    {FOLDER_VECTOR,      F_VECTOR,      F_RASTER        },
    {FOLDER_TRACKBALL,   0,             0               },
    {FOLDER_STEREO,      0,             0               }
};

#define NUM_EXCLUSIONS  9

/* Pairs of filters that exclude each other */
DWORD filterExclusion[NUM_EXCLUSIONS] = {
    IDC_FILTER_CLONES,      IDC_FILTER_ORIGINALS,
    IDC_FILTER_NONWORKING,  IDC_FILTER_WORKING,
    IDC_FILTER_UNAVAILABLE, IDC_FILTER_AVAILABLE,
    IDC_FILTER_RASTER,      IDC_FILTER_VECTOR
};

/* list of filter/control Id pairs */
FILTER_ITEM filterList[F_NUM_FILTERS] = {
    {F_NEOGEO,       IDC_FILTER_NEOGEO},
    {F_CLONES,       IDC_FILTER_CLONES},
    {F_NONWORKING,   IDC_FILTER_NONWORKING},
    {F_UNAVAILABLE,  IDC_FILTER_UNAVAILABLE},
    {F_RASTER,       IDC_FILTER_RASTER},
    {F_VECTOR,       IDC_FILTER_VECTOR},
    {F_ORIGINALS,    IDC_FILTER_ORIGINALS},
    {F_WORKING,      IDC_FILTER_WORKING},
    {F_AVAILABLE,    IDC_FILTER_AVAILABLE}
};

/***************************************************************************
    private functions prototypes
 ***************************************************************************/

static void             DisableFilters(HWND hWnd, LPFILTER_RECORD lpFilterRecord, LPFILTER_ITEM lpFilterItem, DWORD dwFlags);
static DWORD            ValidateFilters(LPFILTER_RECORD lpFilterRecord, DWORD dwFlags);
static void             EnableFilters(HWND hWnd, DWORD dwCtrlID);
static LPFILTER_RECORD  FindFilter(DWORD folderID);

/***************************************************************************
    public functions
 ***************************************************************************/

LRESULT CALLBACK FilterDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    static DWORD            dwFilters;
    static LPFILTER_RECORD  lpFilterRecord;
    int                     i;

    switch (Msg)
    {
    case WM_INITDIALOG:
        dwFilters = 0;
        /* Use global lpCurrentFolder */
        if (lpCurrentFolder != NULL)
        {
            char tmp[80];

            /* Display current folder name in dialog titlebar */
            sprintf(tmp,"Filters for %s Folder", lpCurrentFolder->m_lpTitle);
            SetWindowText(hDlg, tmp);

            /* Mask out non filter flags */
            dwFilters = lpCurrentFolder->m_dwFlags & F_MASK;

            /* Find the matching filter record if it exists */
            lpFilterRecord = FindFilter(lpCurrentFolder->m_nFolderId);

            /* initialize and disable appropriate controls */
            for (i = 0; i < F_NUM_FILTERS; i++)
            {
                DisableFilters(hDlg, lpFilterRecord, &filterList[i], dwFilters);
            }
        }            
        return TRUE;

    case WM_HELP:
        /* User clicked the ? from the upper right on a control */
        WinHelp(((LPHELPINFO) lParam)->hItemHandle, "mame32.hlp", HELP_WM_HELP, GetHelpIDs());
        break;

    case WM_CONTEXTMENU: 
        WinHelp((HWND) wParam, "mame32.hlp", HELP_CONTEXTMENU, GetHelpIDs());
        break; 

    case WM_COMMAND :
        {
            WORD wID         = GET_WM_COMMAND_ID(wParam, lParam);
            HWND hWndCtrl    = GET_WM_COMMAND_HWND(wParam, lParam);
            WORD wNotifyCode = GET_WM_COMMAND_CMD(wParam, lParam);
            
            switch (wID)
            {
            case IDOK :
                dwFilters = 0;

                /* see which buttons are checked */
                for (i = 0; i < F_NUM_FILTERS; i++)
                {
                    if (Button_GetCheck(GetDlgItem(hDlg, filterList[i].m_dwCtrlID)))
                        dwFilters |= filterList[i].m_dwFilterType;
                }

                /* Mask out invalid filters */
                dwFilters = ValidateFilters(lpFilterRecord, dwFilters);

                /* Keep non filter flags */
                lpCurrentFolder->m_dwFlags &= ~F_MASK;

                /* or in the set filters */
                lpCurrentFolder->m_dwFlags |= dwFilters;

                /* Save the filters to the Registry */
                SetFolderFlags(lpCurrentFolder->m_lpTitle, dwFilters);
                EndDialog(hDlg, 1);
                return TRUE;
                
            case IDCANCEL :
                EndDialog(hDlg, 0);
                return TRUE;

            default:
                /* Handle unchecking mutually exclusive filters */
                if (wNotifyCode == BN_CLICKED)
                    EnableFilters(hDlg, wID);
            }
        }
        break;
    }
    return 0;
}

/***************************************************************************
    private functions
 ***************************************************************************/

/* Initialize controls */
static void DisableFilters(HWND hWnd, LPFILTER_RECORD lpFilterRecord, LPFILTER_ITEM lpFilterItem, DWORD dwFlags)
{
    HWND  hWndCtrl = GetDlgItem(hWnd, lpFilterItem->m_dwCtrlID);
    DWORD dwFilterType = lpFilterItem->m_dwFilterType;

    /* Check the appropriate control */
    if (dwFilterType & dwFlags)
        Button_SetCheck(hWndCtrl, MF_CHECKED);

    /* No special rules for this folder? */
    if (lpFilterRecord == (LPFILTER_RECORD)0)
        return;

    /* If this is an excluded filter */
    if (lpFilterRecord->m_dwUnset & dwFilterType)
    {
        /* uncheck it and disable the control */
        Button_SetCheck(hWndCtrl, MF_UNCHECKED);
        EnableWindow(hWndCtrl, FALSE);
    }

    /* If this is an implied filter, check it and disable the control */
    if (lpFilterRecord->m_dwSet & dwFilterType)
    {
        Button_SetCheck(hWndCtrl, MF_CHECKED);
        EnableWindow(hWndCtrl, FALSE);
    }
}

/* find a FILTER_RECORD by folderID */
static LPFILTER_RECORD FindFilter(DWORD folderID)
{
    int i;

    for (i = 0; i < NUM_FOLDERS; i++)
    {
        if (filterRecord[i].m_dwFolderID == folderID)
            return &filterRecord[i];
    }
    return (LPFILTER_RECORD)0;
}

/* Handle disabling mutually exclusive controls */
static void EnableFilters(HWND hWnd, DWORD dwCtrlID)
{
    int     i;
    DWORD   id;

    for (i = 0; i < NUM_EXCLUSIONS; i++)
    {
        /* is this control in the list? */
        if (filterExclusion[i] == dwCtrlID)
        {
            /* found the control id */
            break;
        }
    }

    /* if the control was found */
    if (i < NUM_EXCLUSIONS)
    {
        /* find the opposing control id */
        if (i % 2)
            id = filterExclusion[i - 1];
        else
            id = filterExclusion[i + 1];

        /* Uncheck the other control */
        Button_SetCheck(GetDlgItem(hWnd, id), MF_UNCHECKED);
    }
}

/* Validate filter setting, mask out inappropriate filters for this folder */
static DWORD ValidateFilters(LPFILTER_RECORD lpFilterRecord, DWORD dwFlags)
{
    DWORD dwFilters;

    if (lpFilterRecord != (LPFILTER_RECORD)0)
    {
        /* Mask out implied and excluded filters */
        dwFilters = lpFilterRecord->m_dwSet | lpFilterRecord->m_dwUnset;
        return dwFlags & ~dwFilters;
    }

    /* No special cases - all filters apply */
    return dwFlags;
}

/* End of source file */