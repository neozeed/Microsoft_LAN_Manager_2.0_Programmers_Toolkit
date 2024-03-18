/****************************************************************************

    MANDEL.H -- Constants and function definitions for MANDEL.C 

    Copyright (C) 1990 Microsoft Corporation.

****************************************************************************/

#define IDM_ABOUT 100
#define IDM_ZOOMIN 101
#define IDM_LOCAL  102
#define IDM_REMOTE 103
#define IDM_SAVEAS 104
#define IDM_ZOOMOUT 105
#define IDM_TOP    106
#define IDM_REDRAW 107

#define IDM_1LINE       200
#define IDM_2LINES      201
#define IDM_4LINES      202
#define IDM_8LINES      203
#define IDM_16LINES     204
#define IDM_32LINES     205

#define ID_FILENAME     300
#define ID_DIRECT       301
#define ID_LISTBOX      302
#define ID_CANCEL       303
#define ID_OK           304

#define WM_DOSOMEWORK           (WM_USER+0)
#define WM_PAINTLINE            (WM_USER+1)

#define LBID_SERVERS    102

#define WIDTH           300
#define HEIGHT          300

#define MAXLINES        32

#ifndef MAXPATHLEN
#define MAXPATHLEN      260
#endif

extern int iLines;

int PASCAL WinMain(HANDLE, HANDLE, LPSTR, int);
BOOL InitApplication(HANDLE);
BOOL InitInstance(HANDLE, int);
long FAR PASCAL MainWndProc(HWND, unsigned, WORD, LONG);
BOOL FAR PASCAL About(HWND, unsigned, WORD, LONG);
BOOL FAR PASCAL SaveAsDlgProc(HWND, unsigned, WORD, LONG);
void DoSomeWork(HWND, BOOL);
