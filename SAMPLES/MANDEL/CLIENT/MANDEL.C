/****************************************************************************

    MANDEL.C --

    Main code for the Windows Mandelbrot Set distributed drawing program.

    Copyright (C) 1990 Microsoft Corporation.

    This code sample is provided for demonstration purposes only.
    Microsoft makes no warranty, either express or implied,
    as to its usability in any given situation.

****************************************************************************/

#include <sys\types.h>
#include <sys\stat.h>
#include <direct.h>
#include <dos.h>
#include <string.h>
#include <ctype.h>

#define  INCL_NETERRORS
#define  INCL_NETSHARE
#include <lan.h>                    /* LAN Manager header files */

#include "windows.h"		    /* Required for all Windows applications */
#include "mandel.h"		    /* Specific to this program		     */
#include "calc.h"                   /* Defines a complex point */
#include "rmprot.h"                 /* CALCBUF, PRECTL */
#include "remote.h"                 /* For remote data transfer */
#include "debug.h"                  /* Message */


#define MENUNAME            "MandelMenu"
#define CLASSNAME           "MandelClass"
#define ABOUTBOX            "AboutBox"
#define SAVEBOX             "SAVEBOX"


#define POLL_TIME           100
#define LINES               4


#define UNREFERENCED(h)     (void)h


char szTitle[] = "Mandelbrot Fun";
char szBitmap[] = "Mandel";

CPOINT  cptUL = { (double) -2.05, (double) 1.4 };
double  dPrec = (double) .01;


void PaintLine( HWND, svr_table *, HDC, int);
COLORREF MapColor( DWORD, DWORD );
void DrawRect( HWND, PRECT, BOOL, HDC);
long get_drivemap(void);
void set_dlgitems(HWND, char *, long);
BOOL update_curdir(HWND, char *, long);
char * getdcwd( int, char *, unsigned);
void set_filename( HWND );
BOOL munge_path( char *);

HANDLE hInst;			    /* current instance			     */

int iLines = LINES;


/*
 *
 *  FUNCTION: WinMain(HANDLE, HANDLE, LPSTR, int)
 *
 *  PURPOSE: Calls initialization function, processes message loop
 *
 *  COMMENTS:
 *
 *      Windows recognizes this function by name as the initial entry point
 *      for the program.  This function calls the application initialization
 *      routine, if no other instance of the program is running, and always
 *      calls the instance initialization routine.  It then executes a message
 *      retrieval and dispatch loop that is the top-level control structure
 *      for the remainder of execution.  The loop is terminated when a WM_QUIT
 *      message is received, at which time this function exits the application
 *      instance by returning the value passed by PostQuitMessage().
 *
 *      If this function must abort before entering the message loop, it
 *      returns the conventional value NULL.
 *
 */

int PASCAL WinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow)
HANDLE hInstance;			     /* current instance	     */
HANDLE hPrevInstance;			     /* previous instance	     */
LPSTR lpCmdLine;			     /* command line		     */
int nCmdShow;				     /* show-window type (open/icon) */
{
    MSG msg;				     /* message			     */

    UNREFERENCED(lpCmdLine);

    if (!hPrevInstance)			 /* Other instances of app running? */
	if (!InitApplication(hInstance)) /* Initialize shared things */
	    return (FALSE);		 /* Exits if unable to initialize     */

    /* Perform initializations that apply to a specific instance */

    if (!InitInstance(hInstance, nCmdShow))
        return (FALSE);

    /* Acquire and dispatch messages until a WM_QUIT message is received. */

    while (GetMessage(&msg,	   /* message structure			     */
	    NULL,		   /* handle of window receiving the message */
	    NULL,		   /* lowest message to examine		     */
	    NULL))		   /* highest message to examine	     */
    {
	TranslateMessage(&msg);	   /* Translates virtual key codes	     */
	DispatchMessage(&msg);	   /* Dispatches message to window	     */
    }

    return (msg.wParam);	   /* Returns the value from PostQuitMessage */
}



/*
 *   FUNCTION: InitApplication(HANDLE)
 *
 *   PURPOSE: Initializes window data and registers window class
 *
 *   COMMENTS:
 *
 *       This function is called at initialization time only if no other
 *       instances of the application are running.  This function performs
 *       initialization tasks that can be done once for any number of running
 *       instances.
 *
 *       In this case, we initialize a window class by filling out a data
 *       structure of type WNDCLASS and calling the Windows RegisterClass()
 *       function.  Since all instances of this application use the same window
 *       class, we only need to do this when the first instance is initialized.
 */

BOOL InitApplication(hInstance)
HANDLE hInstance;			       /* current instance	     */
{
    WNDCLASS  wc;

    /* Fill in window class structure with parameters that describe the       */
    /* main window.                                                           */

    wc.style = NULL;                    /* Class style(s).                    */
    wc.lpfnWndProc = MainWndProc;       /* Function to retrieve messages for  */
                                        /* windows of this class.             */
    wc.cbClsExtra = 0;                  /* No per-class extra data.           */
    wc.cbWndExtra = 0;                  /* No per-window extra data.          */
    wc.hInstance = hInstance;           /* Application that owns the class.   */
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH); 
    wc.lpszMenuName =  MENUNAME;   /* Name of menu resource in .RC file. */
    wc.lpszClassName = CLASSNAME; /* Name used in call to CreateWindow. */

    /* Register the window class and return success/failure code. */

    return (RegisterClass(&wc));

}



/*
 *   FUNCTION:  InitInstance(HANDLE, int)
 *
 *   PURPOSE:  Saves instance handle and creates main window.
 *
 *   COMMENTS:
 *
 *       This function is called at initialization time for every instance of
 *       this application.  This function performs initialization tasks that
 *       cannot be shared by multiple instances.
 *
 *       In this case, we save the instance handle in a static variable and
 *       create and display the main program window.
 */

BOOL InitInstance(hInstance, nCmdShow)
    HANDLE          hInstance;          /* Current instance identifier.       */
    int             nCmdShow;           /* Param for first ShowWindow() call. */
{
    HWND            hWnd;               /* Main window handle.                */
    RECT            rc;

    /* Save the instance handle in static variable, which will be used in  */
    /* many subsequence calls from this application to Windows.            */

    hInst = hInstance;

    /* Create a main window for this application instance.  */

    hWnd = CreateWindow(
        CLASSNAME,                      /* See RegisterClass() call.          */
        szTitle,                        /* Text for window title bar.         */
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_BORDER | WS_MINIMIZEBOX,
        CW_USEDEFAULT,                  /* Default horizontal position.       */
        CW_USEDEFAULT,                  /* Default vertical position.         */
        WIDTH,                          /* Default width.                     */
        HEIGHT,                         /* Default height.                    */
        NULL,                           /* Overlapped windows have no parent. */
        NULL,                           /* Use the window class menu.         */
        hInstance,                      /* This instance owns this window.    */
        NULL                            /* Pointer not needed.                */
    );

    /* If window could not be created, return "failure" */

    if (!hWnd)
        return (FALSE);

    /* Make the window visible; update its client area; and return "success" */

    ShowWindow(hWnd, nCmdShow);  /* Show the window                        */
    UpdateWindow(hWnd);          /* Sends WM_PAINT message                 */

    // Initialize the remote connection stuff
    if (!InitRemote(hWnd))
    {
        return FALSE;
    }

    rc.top = rc.left = 0;
    rc.bottom = HEIGHT-1;
    rc.right = WIDTH-1;

    SetNewCalc( cptUL, dPrec, rc);

    return (TRUE);               /* Returns the value from PostQuitMessage */

}



/*
 *   FUNCTION: MainWndProc(HWND, unsigned, WORD, LONG)
 *
 *   PURPOSE:  Processes messages
 *
 *   MESSAGES:
 *
 *	WM_COMMAND    - application menu (About dialog box)
 *	WM_DESTROY    - destroy window
 *
 *   COMMENTS:
 *
 *	To process the IDM_ABOUT message, call MakeProcInstance() to get
 *      the current instance address of the About() function, then call
 *      DialogBox to create the box according to the information in the
 *	GENERIC.RC file and turn control over to the About() function.
 *	When it returns, free the intance address.
 */

long FAR PASCAL MainWndProc(hWnd, message, wParam, lParam)
HWND hWnd;				  /* window handle		     */
unsigned message;			  /* type of message		     */
WORD wParam;				  /* additional information	     */
LONG lParam;				  /* additional information	     */
{
    FARPROC lpProcAbout;		  /* pointer to the "About" function */
    PAINTSTRUCT ps;
    HDC hdc;
    static HDC  hdcMem;
    static HBITMAP hbmMem;
    static int width;
    static int height;
    RECT        rc;
    static BOOL fRectDefined = FALSE;
    static BOOL fButtonDown = FALSE;
    static RECT rcZoom;
    static POINT pSelected;
    POINT       pMove;
    int         iWidthNew;
    int         iHeightNew;
    static int  miOldLines;
    double      scaling;

    switch (message)
    {

        case WM_CREATE:
            hdc = GetDC(hWnd);
            hdcMem = CreateCompatibleDC(hdc);
            GetWindowRect(hWnd, &rc);
            width = rc.right - rc.left;
            height = rc.bottom - rc.top;
            hbmMem = CreateCompatibleBitmap(hdc, width, height);
            SelectObject(hdcMem, hbmMem);

            ReleaseDC(hWnd,hdc);

            rc.left = rc.top = 0;
            rc.right = width+1;
            rc.bottom = height + 1;
            FillRect(hdcMem, &rc, GetStockObject(WHITE_BRUSH));

            SetTimer(hWnd, 1, POLL_TIME, NULL);  // set timer for polls

            CheckMenuItem(GetMenu(hWnd), IDM_LOCAL,
                                fLocalWork ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(GetMenu(hWnd), IDM_REMOTE,
                                fRemoteWork ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(GetMenu(hWnd), IDM_4LINES, MF_CHECKED);
            miOldLines = IDM_4LINES;
            break;

        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            BitBlt(hdc, ps.rcPaint.left,
                   ps.rcPaint.top,
                   ps.rcPaint.right - ps.rcPaint.left,
                   ps.rcPaint.bottom - ps.rcPaint.top,
                   hdcMem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            EndPaint(hWnd, &ps);
            break;

	case WM_COMMAND:	   /* message: command from application menu */
            switch(wParam)
	    {

            case IDM_ABOUT:
		lpProcAbout = MakeProcInstance(About, hInst);

		DialogBox(hInst,		 /* current instance	     */
		    ABOUTBOX,			 /* resource to use	     */
		    hWnd,			 /* parent handle	     */
		    lpProcAbout);		 /* About() instance address */

		FreeProcInstance(lpProcAbout);
		break;

            case IDM_SAVEAS:
		lpProcAbout = MakeProcInstance(SaveAsDlgProc, hInst);

		DialogBox(hInst,		 /* current instance	     */
		    SAVEBOX,			 /* resource to use	     */
		    hWnd,			 /* parent handle	     */
		    lpProcAbout);		 /* About() instance address */

		FreeProcInstance(lpProcAbout);
		break;

            case IDM_ZOOMIN:            // zoom in on selected rectangle
                // if no rectangle, don't zoom in
                if (!fRectDefined)
                    break;

                // calculate new upper-left
                cptUL.real += (rcZoom.left * dPrec);
                cptUL.imag -= (rcZoom.top * dPrec);

      	        iWidthNew = (rcZoom.right - rcZoom.left + 1);
	        iHeightNew = (rcZoom.bottom - rcZoom.top + 1);
	        scaling =
	          ( (double) ((iWidthNew > iHeightNew) ? iWidthNew : iHeightNew)
		   / (double)width);

	        dPrec *= scaling;

                rc.left = rc.top = 0;
                rc.bottom = height - 1;
                rc.right = width - 1;
                SetNewCalc( cptUL, dPrec, rc);
                fRectDefined = FALSE;
                DoSomeWork(hWnd, FALSE);
                break;

            case IDM_REDRAW:
                rc.left = rc.top = 0;
                rc.right = width+1;
                rc.bottom = height + 1;
                FillRect(hdcMem, &rc, GetStockObject(WHITE_BRUSH));
                InvalidateRect(hWnd, NULL, TRUE);

                rc.left = rc.top = 0;
                rc.bottom = height - 1;
                rc.right = width - 1;
                SetNewCalc( cptUL, dPrec, rc);
                fRectDefined = FALSE;
                DoSomeWork(hWnd, FALSE);
                break;

            case IDM_TOP:
                cptUL.real = (double) -2.05;
                cptUL.imag = (double) 1.4;
                dPrec = .01;

                rc.left = rc.top = 0;
                rc.bottom = height - 1;
                rc.right = width - 1;
                SetNewCalc( cptUL, dPrec, rc);
                fRectDefined = FALSE;
                DoSomeWork(hWnd, FALSE);
                break;

            case IDM_LOCAL:
                fLocalWork = !fLocalWork;
                CheckMenuItem(GetMenu(hWnd), IDM_LOCAL,
                                fLocalWork ? MF_CHECKED : MF_UNCHECKED);
                break;

            case IDM_REMOTE:
                fRemoteWork = !fRemoteWork;
                CheckMenuItem(GetMenu(hWnd), IDM_REMOTE,
                                fRemoteWork ? MF_CHECKED : MF_UNCHECKED);
                if (fRemoteWork)
                    RetryConnections();
                break;

            case IDM_1LINE:
            case IDM_2LINES:
            case IDM_4LINES:
            case IDM_8LINES:
            case IDM_16LINES:
            case IDM_32LINES:

                CheckMenuItem(GetMenu(hWnd), miOldLines, MF_UNCHECKED);
                miOldLines = wParam;
                switch(wParam)
		{
		
                case IDM_1LINE:
                    iLines = 1;
                    break;
                case IDM_2LINES:
                    iLines = 2;
                    break;
                case IDM_4LINES:
                    iLines = 4;
                    break;
                case IDM_8LINES:
                    iLines = 8;
                    break;
                case IDM_16LINES:
                    iLines = 16;
                    break;
                case IDM_32LINES:
                    iLines = 32;
                }

                CheckMenuItem(GetMenu(hWnd), miOldLines, MF_CHECKED);
                break;


	    default:			    /* Lets Windows process it	     */
		return (DefWindowProc(hWnd, message, wParam, lParam));
            }
            break;

	case WM_DESTROY:		  /* message: window being destroyed */
	    PostQuitMessage(0);
            DeleteDC(hdcMem);
            DeleteObject(hbmMem);
	    break;

        case WM_DOSOMEWORK:
            // do another slice of calculation work
            DoSomeWork(hWnd, FALSE);
            break;

        case WM_PAINTLINE:
            // The shared buffer contains a line of data; draw it
            PaintLine(hWnd, (svr_table *)wParam, hdcMem, height);
            break;

        case WM_TIMER:
            // timer means we should do another slice of work
            DoSomeWork(hWnd, TRUE);
            break;

        case WM_LBUTTONDOWN:
            // left button down; start to define a zoom rectangle

            if (fRectDefined)
                DrawRect(hWnd, &rcZoom, FALSE, hdcMem); // undraw old rectangle

            // initialize rectangle
            rcZoom.left = rcZoom.right = pSelected.x = LOWORD(lParam);
            rcZoom.top = rcZoom.bottom = pSelected.y = HIWORD(lParam);

            // draw the new rectangle
            DrawRect(hWnd, &rcZoom, TRUE, hdcMem);

            fRectDefined = TRUE;
            fButtonDown = TRUE;
            SetCapture(hWnd);       // capture all mouse events
            break;

        case WM_MOUSEMOVE:

            // mouse move -- if the button is down, change the rect
            if (!fButtonDown)
                break;

            DrawRect(hWnd, &rcZoom, FALSE, hdcMem); // undraw old rect

            pMove.x = LOWORD(lParam);
            pMove.y = HIWORD(lParam);

            // update the selection rectangle
            if (pMove.x <= pSelected.x)
                rcZoom.left = pMove.x;
            if (pMove.x >= pSelected.x)
                rcZoom.right = pMove.x;
            if (pMove.y <= pSelected.y)
                rcZoom.top = pMove.y;
            if (pMove.y >= pSelected.y)
                rcZoom.bottom = pMove.y;

            DrawRect(hWnd, &rcZoom, TRUE, hdcMem);  // draw new rect
            break;

        case WM_LBUTTONUP:
            // button up; end selection
            fButtonDown = FALSE;
            ReleaseCapture();
            break;

	default:			  /* Passes it on if unproccessed    */
	    return (DefWindowProc(hWnd, message, wParam, lParam));
    }
    return (NULL);
}



/*
 *   FUNCTION: About(HWND, unsigned, WORD, LONG)
 *
 *   PURPOSE:  Processes messages for "About" dialog box
 *
 *   MESSAGES:
 *
 *	WM_INITDIALOG - initialize dialog box
 *	WM_COMMAND    - Input received
 *
 *   COMMENTS:
 *
 *	No initialization is needed for this particular dialog box, but TRUE
 *	must be returned to Windows.
 *
 *	Wait for user to click on "Ok" button, then close the dialog box.
 */

BOOL FAR PASCAL About(hDlg, message, wParam, lParam)
HWND hDlg;                                /* window handle of the dialog box */
unsigned message;                         /* type of message                 */
WORD wParam;                              /* message-specific information    */
LONG lParam;
{
    char    buf[UNCLEN+20];
    int     i;
    int     cServerCount;

    UNREFERENCED(lParam);

    switch (message)
    {
	case WM_INITDIALOG:		   /* message: initialize dialog box */

            // initialize listbox
            SendDlgItemMessage(hDlg, LBID_SERVERS, LB_RESETCONTENT, 0, 0L);

            cServerCount = GetServerCount();

            for (i = 0; i < cServerCount; i++)
            {
                GetServerName(i, buf);
                SendDlgItemMessage(hDlg, LBID_SERVERS, LB_INSERTSTRING,
                        -1, (long)(char far *) buf);
            }
	    return (TRUE);

	case WM_COMMAND:		      /* message: received a command */
	    if (wParam == IDOK                /* "OK" box selected?	     */
                || wParam == IDCANCEL)        /* System menu close command? */
	    {
		EndDialog(hDlg, TRUE);	      /* Exits the dialog box	     */
		return (TRUE);
	    }
	    break;
    }
    return (FALSE);			      /* Didn't process a message    */
}



/*
 *   FUNCTION: SaveAsDlgProc(HWND, unsigned, WORD, LONG)
 *
 *   PURPOSE:  Processes messages for "Save As" dialog box
 *
 *   MESSAGES:
 *
 *	WM_INITDIALOG - initialize dialog box
 *	WM_COMMAND    - Input received
 *
 *   COMMENTS:
 *
 *	Wait for user to click on "Ok" button, then close the dialog box.
 */

BOOL FAR PASCAL SaveAsDlgProc(hDlg, message, wParam, lParam)
HWND hDlg;                                /* window handle of the dialog box */
unsigned message;                         /* type of message                 */
WORD wParam;                              /* message-specific information    */
LONG lParam;
{

    static long drivemap = 0L;
    static char curdir[MAXPATHLEN];

    switch (message)
    {

	case WM_INITDIALOG:		   /* message: initialize dialog box */

            // initialize dialog items
            drivemap = get_drivemap();

            // start out at cwd
            getcwd(curdir, MAXPATHLEN);

            set_dlgitems(hDlg, curdir, drivemap);

	    return (TRUE);

	case WM_COMMAND:		      /* message: received a command */
            switch( wParam)
	    {

            case ID_CANCEL:
                EndDialog(hDlg, TRUE);

            case ID_OK:
                if (update_curdir(hDlg, curdir, drivemap))
                    EndDialog(hDlg, TRUE);

            case ID_LISTBOX:
                Message("Message from listbox");
                switch(HIWORD(lParam))
		{

                case LBN_SELCHANGE:     // clicked on an object
                    Message("Click on a listbox item");
                    set_filename(hDlg);
                    break;

                case LBN_DBLCLK:        // double-click on an object
                    Message("Double click on a listbox item");
                    if (update_curdir(hDlg, curdir, drivemap))
                        EndDialog(hDlg, TRUE);
                    break;
                }
	    }
            return TRUE;
    }
    return (FALSE);			      /* Didn't process a message    */
}


void
set_filename(HWND       hdlg)
{

    char    filename[13];

    SendDlgItemMessage(hdlg, ID_LISTBOX, LB_GETTEXT,
                        (WORD)SendDlgItemMessage(hdlg, ID_LISTBOX, LB_GETCURSEL,
                                                  0, 0L),
                        (DWORD)(char far *) filename);

    if (filename[0] == '[') {
        filename[0] = filename[1];
        filename[1] = ':';
        filename[2] = '\0';
    }

    SetDlgItemText(hdlg, ID_FILENAME, filename);

}



void
set_dlgitems(   HWND        hdlg,
                char *      curdir,
                long        drivemap)
{

    static char     drive[] = "[A:]";
    char            driveletter;
    char            searchpath[MAXPATHLEN];
    struct find_t   fi;
    unsigned        res;

    SetDlgItemText(hdlg, ID_FILENAME, "");

    SendDlgItemMessage(hdlg, ID_LISTBOX, WM_SETREDRAW, FALSE, 0L);
    SendDlgItemMessage(hdlg, ID_LISTBOX, LB_RESETCONTENT, 0, 0L);

    // do the drives

    driveletter = 'A';

    while (drivemap != 0L) {
        if (drivemap & 0x1) {
            drive[1] = driveletter;
            SendDlgItemMessage(hdlg, ID_LISTBOX, LB_ADDSTRING, 0,
                                 (DWORD)(char far *) drive);
        }
        driveletter++;
        drivemap >>= 1;
    }

    // now do the directories

    if ((curdir[0] == '\\') && (strchr(curdir+2, '\\') == NULL))
    {
        // this is a \\server, so get the visible shares
        unsigned short avail;
        unsigned short read;
        char * buf = NULL;
        struct share_info_0 * si;
        HANDLE h;
        int i;


        res = NetShareEnum(curdir, 0, buf, 0, &read, &avail);
        if ((res != 0) && (res != ERROR_MORE_DATA))
            avail = 0;

        h = LocalAlloc(LMEM_FIXED, avail * sizeof(struct share_info_0));
        if (h != NULL)
        {
            buf = LocalLock(h);

            res = NetShareEnum(curdir, 0, buf,
                                avail * sizeof(struct share_info_0),
                                &read, &avail);

            if ((res != 0) && (res != ERROR_MORE_DATA))
                read = 0;

            si = (struct share_info_0 *) buf;

            for (i = 0; i < (int)read; ++i)
            {
                char * dollar;

                // don't display admin shares, which end with $
                dollar = strchr(si[i].shi0_netname, '$');
                if ((dollar != NULL) && (dollar[1] == '\0'))
                    continue;

                SendDlgItemMessage(hdlg, ID_LISTBOX, LB_INSERTSTRING, 0,
                                   (DWORD)(char far *)si[i].shi0_netname);
            }

            LocalUnlock(h);
            LocalFree(h);
        }
    }
    else
    {
        strcpy(searchpath, curdir);

        // make sure we have a trailing \
        if (strlen(searchpath) > 3)     // more than just c:\
            strcat(searchpath, "\\");  // so add the trailing \

        strcat(searchpath, "*.*");

        res = _dos_findfirst(searchpath, _A_SUBDIR, &fi);

        while (res == 0)
        {
            // add it if it's a subdirectory
            if ((fi.attrib & _A_SUBDIR) == _A_SUBDIR)
                SendDlgItemMessage(hdlg, ID_LISTBOX, LB_INSERTSTRING, 0,
                                    (DWORD)(char far *)fi.name);

            res = _dos_findnext(&fi);

        }
    }

    // set the current directory
    SetDlgItemText(hdlg, ID_DIRECT, curdir);
    SendDlgItemMessage(hdlg, ID_LISTBOX, WM_SETREDRAW, TRUE, 0L);
}



BOOL
update_curdir(HWND      hdlg,
              char *    curdir,
              long      drivemap)
{

    char    filename[MAXPATHLEN];
    char    newcurdir[MAXPATHLEN];
    unsigned    num;
    struct stat statbuf;

    // get the filename
    GetDlgItemText(hdlg, ID_FILENAME, filename, sizeof(filename));

    /*
     * Now examine it.
     *
     * If it's UNC or absolute with drive, it becomes our new curdir
     * If it's absolute with no drive, we carry over the drive
     * If it's relative, we carry over the whole curdir and append
     */

    Message("update_curdir: filename == %s", filename);

    // if it's not real, forget it
    if (strlen(filename) == 0)
    {
        return FALSE;
    }

    // if it's UNC, use it
    if ((filename[0] == '\\') && (filename[1] == '\\'))
    {
        Message("it's UNC!");
        strcpy(newcurdir, filename);
    }

    // not UNC, is it absolute, no-drive?
    else if ( filename[0] == '\\')
    {
        Message("Absolute, no drive!");

        // if our curdir is UNC, forget it
        if (curdir[0] == '\\')
            return FALSE;

        strncpy(newcurdir, curdir, 2);
        strcpy(&(newcurdir[2]), filename);
    }

    // does it have a drive?
    else if (isalpha(*filename) && (filename[1] == ':'))
    {
        // absolute?
        if (filename[2] == '\\') {
            Message("Absolute, with drive");
            strcpy(newcurdir, filename);
        }

        else    // relative
        {
            Message("Relative, with drive");
            if (getdcwd(toupper(filename[0]) - 'A' + 1, newcurdir,
                         MAXPATHLEN) == NULL)
            {
                Message("getdcwd failed!");
                return FALSE;
            }

            if (strlen(filename) > 2)
            {
                if (strlen(newcurdir) > 3)
                    strcat(newcurdir, "\\");
                strcat(newcurdir, filename+2);
            }
        }

    }

    // relative, no drive -- only option left
    else {
        Message("Relative: no drive");

        strcpy(newcurdir, curdir);
        if (strlen(newcurdir) > 3)
            strcat(newcurdir, "\\");
        strcat(newcurdir, filename);
    }

    Message("update_curdir: newcurdir == %s", newcurdir);

    if (!munge_path(newcurdir))
        return FALSE;

    Message("update_curdir: after munging, == %s", newcurdir);


    // now we have a full path to either a file or a directory.
    // Make sure it's legal.

    // is it just a root directory of a drive?
    if ((strlen(newcurdir) == 3) && (newcurdir[0] != '\\'))
    {
        statbuf.st_mode = S_IFDIR;
    }

    // no, is it \\server?
    else if ((newcurdir[0] == '\\') &&
              (strchr(newcurdir+2, '\\') == NULL))
    {
        unsigned short read;
        unsigned short avail;
        unsigned res;

        res = NetShareEnum(newcurdir, 0, NULL, 0, &read, &avail);
        Message("NetShareEnum returned %hu", res);
        if ((res !=0) && (res != ERROR_MORE_DATA))
            return FALSE;

        strcpy(curdir, newcurdir);
        set_dlgitems(hdlg, curdir, drivemap);
        return FALSE;
    }

    // no, check it out more carefully
    else if (stat(newcurdir, &statbuf) != 0)
    {
        // Okay, so this doesn't exist. See if the subdirectory exists.
        char    temp;
        char *  tempptr;

        tempptr = strrchr(newcurdir, '\\');
        if (tempptr == NULL)
            return FALSE;

        // if "c:\", cut off after the \
        if (tempptr == newcurdir + 2)
        {
            tempptr++;
        }

        temp = *tempptr;
        *tempptr = '\0';

        if (stat(newcurdir, &statbuf) != 0)
            return FALSE;

        // make it our new current dir
        _dos_setdrive(toupper(newcurdir[0]) - 'A' + 1, &num);
        chdir(newcurdir);

        // subdirectory exists, so take it as a filename
        *tempptr = temp;        // fix up newcurdir the way it was

        strcpy(curdir, newcurdir);
        return TRUE;
    }


    // it already exists; if an absolute dir, cd into it
    if ((statbuf.st_mode & S_IFDIR) == S_IFDIR)
    {
        if (newcurdir[0] != '\\')
        {
            _dos_setdrive(toupper(newcurdir[0]) - 'A' + 1, &num);
            chdir(newcurdir);
        }
        strcpy(curdir, newcurdir);
        set_dlgitems(hdlg, curdir, drivemap);
        return FALSE;
    }

    // it already exists, and is a file; overwrite it
    strcpy(curdir, newcurdir);
    return TRUE;
}



long
get_drivemap( void )
{
    unsigned curdrive;
    unsigned testdrive;
    unsigned num;
    unsigned test;
    long map = 0L;

    _dos_getdrive(&curdrive);

    for (testdrive = 1; testdrive < 27; ++testdrive)
    {
        _dos_setdrive(testdrive, &num);
        _dos_getdrive(&test);
        if (test == testdrive)
            map |= 1 << (testdrive-1);
    }

    _dos_setdrive(curdrive, &num);

    return map;
}



char * getdcwd( int drive, char * path, unsigned sz)
{
    int old;
    unsigned num;

    _dos_getdrive(&old);

    _dos_setdrive(drive, &num);

    getcwd(path, sz);

    _dos_setdrive(old, &num);

    return path;
}



BOOL
munge_path( char * path)
{
    char    pathcopy[MAXPATHLEN];
    char *  backslash;
    char *  token;
    int     tokencount;
    char    temp;
    char *  start;


    Message("MungePath: munging %s", path);

    // It's either absd or UNC

    if (path[0] == '\\')
    {
        pathcopy[0] = pathcopy[1] = '\\';
        pathcopy[2] = '\0';
        start = &(path[2]);
    }
    else {
        temp = path[3];
        path[3] = '\0';
        strcpy(pathcopy, path);
        path[3] = temp;
        start = &(path[3]);
    }


    tokencount = 0;
    // now we have either "\\" or "c:\", and start points to the rest

    while ((token = strtok(start, "\\")) != NULL)
    {
        start = NULL;

        if (!strcmp(token, "."))
            continue;

        else if (!strcmp(token, ".."))
        {
            if (tokencount == 0)
                continue;

            tokencount--;
            backslash = strrchr(pathcopy, '\\');
            if (tokencount == 0)
                backslash++;

            *backslash = '\0';
        }

        else  {     // a normal component
            if (tokencount > 0)
                strcat(pathcopy, "\\");
            strcat(pathcopy, token);
            tokencount++;
        }
    }

    // if it's just \\, bag out
    if ((tokencount == 0) && (pathcopy[0] == '\\'))
        return FALSE;

    strcpy(path, pathcopy);
    return TRUE;
}



/*
 *  DoSomeWork --
 *
 *  This function does our work for us. It does it in little pieces, and
 *  will schedule itself as it sees fit.
 */

void
DoSomeWork( HWND    hwnd,
            BOOL    fTimer)
{

    static WORD   wIteration = 0;

    if (fTimer) {
        wIteration++;

        // on every nth tick, we send out a poll
        if (wIteration == 120) {       // tune this?
            wIteration = 0;
            PollForServers();
            RetryConnections();
            return;
        }

        // on the half-poll, we check for responses
        if ((wIteration == 2) || (wIteration == 10)) {
            CheckPoll();
            return;
        }

    }

    if (CheckPipeStatus(hwnd, fTimer, wIteration))
        SendMessage(hwnd, WM_DOSOMEWORK, 0, 0L);

    return;
}



/*
 *  DrawRect --
 *
 *  This function draws (or undraws) the zoom rectangle.
 */

void
DrawRect( HWND      hwnd,
          PRECT     prc,
          BOOL      fDrawIt,
          HDC       hdcBM)
{

    HDC hdc;
    DWORD   dwRop;

    hdc = GetDC(hwnd);

    if (fDrawIt)
        dwRop = NOTSRCCOPY;
    else
        dwRop = SRCCOPY;


    // top side
    BitBlt(hdc, prc->left, prc->top, (prc->right - prc->left) + 1,
                1, hdcBM, prc->left, prc->top, dwRop);

    // bottom side
    BitBlt(hdc, prc->left, prc->bottom, (prc->right - prc->left) + 1,
                1, hdcBM, prc->left, prc->bottom, dwRop);

    // left side
    BitBlt(hdc,prc->left, prc->top, 1, (prc->bottom - prc->top) + 1,
                hdcBM, prc->left, prc->top, dwRop);

    // right side
    BitBlt(hdc,prc->right, prc->top, 1, (prc->bottom - prc->top) + 1,
                hdcBM, prc->right, prc->top, dwRop);

    ReleaseDC(hwnd, hdc);
}



/*
 *  PaintLine --
 *
 *  This function paints a buffer of data into the bitmap.
 */

void
PaintLine(  HWND    hwnd,
            svr_table * pst,
            HDC     hdcBM,
            int     cHeight)
{

    PDWORD  pdwDrawData;
    int     y;
    int     x;
    DWORD   dwThreshold;
    RECT    rc;
    WORD    lines = pst->cLines;


    // picture ID had better match, or else we skip it
    if (CheckDrawingID(pst->cPicture))
    {
        // figure out our threshold
        dwThreshold = QueryThreshold();

        // get a pointer to the draw buffer
        pdwDrawData = GetDrawBuffer();
        if (pdwDrawData == NULL) {
            ReturnDrawBuffer();
            return;
        }

        // starting x coordinate
        x = (int) pst->dwLine;

        // now loop through the rectangle
        while (lines-- > 0)
        {
            // bottom to top, since that's the order of the data in the buffer
            y = (int) cHeight-1;


            while (y >= 0) {
                // draw a pixel
                SetPixel(hdcBM, x,y, MapColor(*pdwDrawData, dwThreshold));

                // now increment buffer pointer and y coord
                y--;
                pdwDrawData++;
            }
            x++;        // increment X coordinate
        }

        // figure out the rectangle to invalidate
        rc.top = 0;
        rc.bottom = cHeight;
        rc.left = (int)(pst->dwLine);
        rc.right = (int)(pst->dwLine) + pst->cLines;

        FreeDrawBuffer();

        // and invalidate it on the screen so we redraw it
        InvalidateRect(hwnd, &rc, FALSE);
    }

    // free this for someone else to use
    ReturnDrawBuffer();

    // and change the pipe state, if necessary
    if (pst->iStatus == SS_PAINTING)
        pst->iStatus = SS_IDLE;

}


#define CLR_BLACK       RGB(0,0,0)
#define CLR_DARKBLUE    RGB(0,0,127)
#define CLR_BLUE        RGB(0,0,255)
#define CLR_CYAN        RGB(0,255,255)
#define CLR_DARKGREEN   RGB(0,127,0)
#define CLR_GREEN       RGB(0,255,0)
#define CLR_YELLOW      RGB(255,255,0)
#define CLR_RED         RGB(255,0,0)
#define CLR_DARKRED     RGB(127,0,0)
#define CLR_WHITE       RGB(255,255,255)
#define CLR_PALEGRAY    RGB(194,194,194)
#define CLR_DARKGRAY    RGB(127,127,127)


static COLORREF	ColorMapTable[] = {
    CLR_DARKBLUE,
    CLR_BLUE,
    CLR_CYAN,
    CLR_DARKGREEN,
    CLR_GREEN,
    CLR_YELLOW,
    CLR_RED,
    CLR_DARKRED,
    CLR_WHITE,
    CLR_PALEGRAY,
    CLR_DARKGRAY};


/*
 *  MapColor --
 *
 *  This function maps an iteration count into a corresponding RGB color.
 */

COLORREF
MapColor(DWORD  dwIter,
         DWORD  dwThreshold)
{

    // if it's beyond the threshold, call it black
    if (dwIter >= dwThreshold)
	return CLR_BLACK;

    // get a modulus based on the number of colors
    dwIter = (dwIter / 3) % 11;

    // and return the appropriate color
    return ColorMapTable[dwIter];

}
