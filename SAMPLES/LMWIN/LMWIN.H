/*
 * 
 * lmwin.h
 * 
 * This is the C source include file for lmwin.c. It defines the
 * constants, macros, data types, and function prototypes used by
 * the lmwin application.
 * 
 */

/* ------------------------------------------------------------------------ */

/* constants/macros */

#define IDM_ABOUT     100

#define APP_NAME      "LmWin"
#define MENU_NAME     "LmWinMenu"
#define CLASS_NAME    "LmWinClass"
#define WINDOW_NAME   "Sample LAN Manager/Windows Application"
#define ABOUT_BOX_DLG "AboutBox"

/* note: highly order-dependent to vastrWkstaInfo[]! */
#define WKI_computername     0
#define WKI_username         1
#define WKI_langroup         2
#define WKI_logon_domain     3
#define WKI_oth_domains      4
#define WKI_root             5
#define WKI_charwait         6
#define WKI_chartime         7
#define WKI_charcount        8
#define WKI_numcharbuf       9
#define WKI_sizcharbuf      10
#define WKI_numworkbuf      11
#define WKI_sizworkbuf      12
#define WKI_numdgrambuf     13
#define WKI_keepconn        14
#define WKI_keepsearch      15
#define WKI_maxcmds         16
#define WKI_numservices     17
#define WKI_mailslots       18

/* ------------------------------------------------------------------------ */

/* data types */

typedef struct _WKSTAINFO
{
    int iMsg;
    char *pszName;
    char *pszFmt;
} WKSTAINFO;
typedef WKSTAINFO *PWKSTAINFO;
typedef WKSTAINFO NEAR *NPWKSTAINFO;
typedef WKSTAINFO FAR *LPWKSTAINFO;

/* an X,Y coordinate (similar to the POINT struct) */
typedef struct _XY
{
    short int x;
    short int y;
} XY;
typedef XY *PXY;
typedef XY NEAR *NPXY;
typedef XY FAR *LPXY;

/* ------------------------------------------------------------------------ */

/* function prototypes */

int PASCAL WinMain(HANDLE hInstance, HANDLE hPrevInstance, LPSTR lpCmdLine, 
    int nCmdShow);
BOOL InitApplication(HANDLE hInstance);
BOOL InitInstance(HANDLE hInstance, int nCmdShow);
LONG FAR PASCAL WndProc(HWND hwnd, unsigned usMsg, WORD wParam, LONG lParam);
BOOL FAR PASCAL About(HWND hDlg, unsigned usMessage, WORD wParam, LONG lParam);
void ErrorMsgBox(HWND hWnd, LPSTR lpszMsg);
void WkstaInfo(HWND hWnd, HDC hdc);
void WkstaLine(HDC hdc, int iMsgNumber, ...);

/* ------------------------------------------------------------------------ */
