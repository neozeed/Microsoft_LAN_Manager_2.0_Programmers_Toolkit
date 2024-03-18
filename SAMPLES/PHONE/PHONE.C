/*--------------------------------------------------------------
   PHONE.C -- PHONE UTILITY
 
   Copyright (C) 1990 Microsoft Corporation 
  
   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, as to
   its usability in any given situation.
  --------------------------------------------------------------*/

#define INCL_WIN           // OS2 header files
#define INCL_GPI
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMEMMGR
#define INCL_DOSSIGNALS
#define INCL_DOSNMPIPES
#define INCL_DOSERRORS
#include <os2.h>

#include <stdio.h>         // C header files
#include <stddef.h>
#include <stdlib.h>
#include <process.h>
#include <ctype.h>
#include <conio.h>
#include <string.h>
#include <malloc.h>

#define  INCL_NETCONFIG    // LAN Manager header files
#define  INCL_NETERRORS
#include <lan.h>

#include "phone.h"
#include "easyfont.h"
#include "mail.h"


#define BUFFER(x,y) (*(pBuffer + y * xMax + x)) // LOCAL buffer manipulation
#define RBUFFER(x,y) (*(pBufferR + y * xMax + x)) // Remote buffer manipulation


MRESULT EXPENTRY ClientWndProc (HWND, USHORT, MPARAM, MPARAM) ;
MRESULT EXPENTRY AboutDlgProc  (HWND, USHORT, MPARAM, MPARAM) ;
MRESULT EXPENTRY HelpDlgProc  (HWND, USHORT, MPARAM, MPARAM) ;
MRESULT EXPENTRY CallDlgProc   (HWND, USHORT, MPARAM, MPARAM) ;

                // Initiate help structure. The help file for phone is
                // phone.hlp
HELPINIT helpinit = {
   sizeof (HELPINIT),
   0L,
   NULL,
   (PHELPTABLE)MAKELONG (MAIN_HELPTABLE, 0xFFFF),
   (HMODULE)NULL,
   (HMODULE)NULL,
   0,
   0,
   "Phone: Help",
   CMIC_HIDE_PANEL_ID,
   "phone.hlp"
};

// Functions
VOID PASCAL FAR SignalHandler(USHORT, USHORT);
VOID GetOut(HWND,  HMQ, HAB);
BOOL FillDirectory( HWND hwnd);
VOID DisableAll(VOID);

// Functions defined in Phone2.c
extern VOID FAR  ErrorReport( PSZ, USHORT);
extern INT  NetCall(HWND, PCHAR);
extern INT  NetAns(VOID);
extern VOID Hangup(VOID);
extern SHORT PASCAL FAR  Scroll(INT,INT,  CHAR FAR *);
extern VOID FAR  TalkThread(PVOID);
extern VOID FAR  ConnThread(PVOID);
extern VOID NetClose(VOID);


                  /* GLOBAL DATA AREA */

//  This constant determines the position of the counter character
//  in the network read semaphore name.  This character will be
//  incremented as nessessary to produce a unique semaphore name.

#define PSEM_COUNT ((sizeof( acSemNameP ) / sizeof( acSemNameP[0] )) - 2)
#define TSEM_COUNT ((sizeof( acSemNameT ) / sizeof( acSemNameT[0] )) - 2)
#define CSEM_COUNT ((sizeof( acSemNameC ) / sizeof( acSemNameC[0] )) - 2)
#define BSEM_COUNT ((sizeof( acSemNameB ) / sizeof( acSemNameB[0] )) - 2)

                                         // Semaphore handles and names
HSYSSEM hSemPipe;                        // Used for pipe usage control
HSYSSEM hSemThread;                      // Used for Talk thread control
HSYSSEM hSemConn;                        // Used to control Conn thread
HSYSSEM hSemBlockConn;                   // Used to control Conn thread

static  CHAR  acSemNameP[]=    "\\SEM\\PIPE.SEM0";    // Pipe control
static  CHAR  acSemNameT[]=    "\\SEM\\THREAD.SEM0";  // Thread (Talk) control
static  CHAR  acSemNameC[]=    "\\SEM\\CONN.SEM0";    // Conn Thread Control
static  CHAR  acSemNameB[]=    "\\SEM\\BLOCK.SEM0";   // Conn Thread Control



CHAR    szName[PHNUMSIZE];          // Phone Number
CHAR    szPipeName[PIPENMSIZE];     // Pipe name
HPIPE hPipe;                        // Pipe Handle
CHAR szRemoteGuy[PHNUMSIZE];        // Phone number of Callee
BOOL fPlugSent=FALSE;               // PLUGIN has been sent
BOOL fConnectionMade=FALSE;         // Set to TRUE when connection has been made
extern PHMSG *pPhWriteBuf;          // All characters will be buffered
                                    // in this buffer until time-out happens
static USHORT  usPhWriteBufIndex =0;// It will index into Write Buffer
extern USHORT  usPhWriteBufMax;     // Size of Write Buffer

BOOL fHelpOnlyAllowed = FALSE;      // Allow user to look at help.


// Talk Thread Defintions

VOID *pTalkStack;
INT  tidTalk= 0;
                            // Conn Thread Definitions
VOID *pConnStack;
INT  tidConn= 0;
                            // PM specific constants
CHAR szClientClass[] = "Phone" ;   // Class
CHAR szTitle[] = "Phone";          // Title of program
HAB  hab ;                         // Anchor block  handle
HWND hwndClient;                   // Client window handle
HWND hwndHelp;                     // Help window handle

int main (void)
{
    static ULONG flFrameFlags = FCF_TITLEBAR      | FCF_SYSMENU  |
                                FCF_SIZEBORDER    | FCF_MINMAX   |
                                FCF_SHELLPOSITION | FCF_TASKLIST |
                                FCF_MENU          | FCF_ACCELTABLE |
                                FCF_ICON ;
    HMQ          hmq ;                      // Message queue Handle
    HWND         hwndFrame;                 // Frame handle
    QMSG         qmsg ;                     // QMSG structure
    USHORT       rc;                        // Return Code
    USHORT       i;                         // Index
    INT  iParameterLen;                     // Parameter length
    CHAR szParameterBuffer[128];            // Parameter buffer
    USHORT  usAction;                       // Action taken
    CHAR    szMessage[80];                  // Error buffer

    // Initialize message queues

    hab = WinInitialize (0) ;
    hmq = WinCreateMsgQueue (hab, 100) ;
    // Register window
    WinRegisterClass (hab, szClientClass, ClientWndProc, 0L, 0) ;
    // Create window
    hwndFrame = WinCreateStdWindow (HWND_DESKTOP, WS_VISIBLE,
                                    &flFrameFlags, szClientClass, NULL,
                                    0L,(HMODULE)NULL,
                                    ID_RESOURCE, &hwndClient) ;
    // Set the name on PM title bar
    if (!WinSetWindowText(hwndFrame, szTitle))
         DosBeep(600,175);

    // Help functions
    hwndHelp = WinCreateHelpInstance (hab, &helpinit);
    WinAssociateHelpInstance (hwndHelp, hwndFrame);


    // Some Initiation chores

    // Set up the signal handler

    rc = DosSetSigHandler(SignalHandler,    // Name of signal handler
                          NULL,             // Previous Function
                          NULL,             // Previous Action
                          SIGA_ACCEPT,      // Function will accept
                          SIG_KILLPROCESS) ;// KILL_PROCESS signal

    // Kill ourself so we don't have a problem later.
    if (rc)
    {
        WinMessageBox(HWND_DESKTOP,
                      hwndClient,
                      "Unable to install signal handler.",
                      szClientClass,
                      0, MB_APPLMODAL | MB_ICONHAND | MB_OK);
        GetOut(hwndFrame, hmq, hab);
    }

                                        // Create UniQue Semaphores
                                        // for thread and pipe control
    do                                  // Pipe Semaphore
    {
        acSemNameP[PSEM_COUNT] += 1;
        rc = DosCreateSem( CSEM_PUBLIC, &hSemPipe, acSemNameP );
    } while( rc == ERROR_ALREADY_EXISTS);

    do                                  // Thread (Talk) Semaphore
    {
        acSemNameT[TSEM_COUNT] += 1;
        rc = DosCreateSem( CSEM_PUBLIC, &hSemThread, acSemNameT );
    } while( rc == ERROR_ALREADY_EXISTS );

    do                                  // Thread (Conn) Semaphore
    {
        acSemNameC[CSEM_COUNT] += 1;
        rc = DosCreateSem( CSEM_PUBLIC, &hSemConn, acSemNameC );
    } while( rc == ERROR_ALREADY_EXISTS);

    do                                  // Thread (Conn) Semaphore
                                        // Block the Conn thread
    {
        acSemNameB[CSEM_COUNT] += 1;
        rc = DosCreateSem( CSEM_PUBLIC, &hSemBlockConn, acSemNameB);
    } while( rc == ERROR_ALREADY_EXISTS );



                                          // Clear the semaphores
    rc = DosSemClear( hSemPipe );
    if (rc)
    {
        ErrorReport("Error in DosSemClear", rc);
        GetOut(hwndFrame, hmq, hab);
    }
    rc = DosSemClear( hSemThread );
    if (rc)
    {
        ErrorReport("Error in DosSemClear", rc);
        GetOut(hwndFrame, hmq, hab);
    }
    rc = DosSemClear( hSemConn );
    if (rc)
    {
        ErrorReport("Error in DosSemClear", rc);
        GetOut(hwndFrame, hmq, hab);
    }
    rc = DosSemClear( hSemBlockConn );
    if (rc)
    {
        ErrorReport("Error in DosSemClear", rc);
        GetOut(hwndFrame, hmq, hab);
    }


    // Get our phone number  from lanman.ini
    rc = NetConfigGet(COMPONENTNAME,
                      PHONENUMBER,
                      szParameterBuffer,
                      (USHORT) sizeof(szParameterBuffer),
                      &iParameterLen);


    if (rc)
    {
       ErrorReport("Please Check LanMan.ini. Incorrect [phone]  section",rc);
       DisableAll();
    }
    else
    {
                           // Convert to upper case and save it
       for (i = 0; (i < strlen (szParameterBuffer)) && (i <PHNUMSIZE ); i++ )
              szName[i] = (CHAR )toupper(szParameterBuffer[i] ) ;
       szName[PHNUMSIZE -1] = '\0';
    }

    // Get server name  from lanman.ini

    rc = NetConfigGet(COMPONENTNAME,
                      SERVERNAME,
                      szParameterBuffer,
                      (USHORT) sizeof(szParameterBuffer),
                      &iParameterLen);


    if (rc)
    {
        ErrorReport("Please Check LanMan.ini. Incorrect [phone] section",rc);
        DisableAll();
    }
    else
    {
        strcpy(szPipeName, "\\\\");
                           // Now, put in the server name
        for (i = 0; (i < strlen (szParameterBuffer)) && (i < CNLEN); i++ )
              szPipeName[2+i] = (CHAR) toupper(   szParameterBuffer[i] ) ;
        strcat(szPipeName, PHONELINE);
    }


    /*  We will try to register ourselves with the PBX service
     *  which is running on a server. Registeration is done by
     *  connecting to '\PIPE\PLINE' and sending a PLUGIN message.
     *  If connection is valid, then we will spawn off a thread
     *  to listen to connection-oriented messages in future.
     */

    rc = DosOpen(szPipeName,  // Pipe name
                 &hPipe,      // Handle to pipe
                 &usAction,   // Action taken
                 0,           // Size of file
                 FILE_NORMAL, // Pipe attribute
                 FILE_OPEN,   // Open if pipe exists
                 OPEN_ACCESS_READWRITE | // Access mode
                 OPEN_SHARE_DENYNONE,
                 0L);
    if (rc)
    {
        sprintf(szMessage, "%s %s",
                "Error connecting to server",
                szParameterBuffer);
        ErrorReport(szMessage,rc);
        DisableAll();
    }

    // Set the pipe in Blocked Message mode.
    rc = DosSetNmPHandState(hPipe, NP_WAIT| NP_READMODE_MESSAGE);
    if (rc)
    {
        ErrorReport("DosSetNmpHandState ", rc);
        DisableAll();
    }

    // Spawn off a Connect Thread which will monitor all connection
    // oriented messages such as CALL, HANGUP.

    pConnStack = malloc(STACKSIZE);

    if ((pConnStack != NULL) && (!fHelpOnlyAllowed))
    {
                                           //  Spawn the thread here

        tidConn = _beginthread(ConnThread,
                               pConnStack,
                               STACKSIZE,
                               NULL);
        if (tidConn == -1)
        {
            ErrorReport("BeginThread", rc);
            free(pConnStack);                // Free up memory
            GetOut(hwndFrame, hmq, hab);     //
        }
    }

    // process PM messages

    while (WinGetMsg (hab, &qmsg, NULL, 0, 0))
               WinDispatchMsg (hab, &qmsg) ;


    DosSemSet(hSemConn);                   // Stop the Conn Thread

    GetOut(hwndFrame, hmq, hab);
    return 0;
}


// This routine will disable all menu items except Help so that the
// user can configure the system correctly.

VOID DisableAll(VOID)
{

    WinSendMsg(hwndClient, WM_DISABLE, 0,0);

    fHelpOnlyAllowed = TRUE;        // Allow the user to bring
                                    // up help or exit.
}


VOID GetOut(HWND hwndFrame, HMQ hmq, HAB hab)
{

    USHORT rc;                      // Return Code
    USHORT usBytesW;                // Bytes written
    PHMSG  phMessage;               // Phone message

    DosSemSet(hSemThread);          // Tell threads to kill themselves
                                    // Give them some time
    DosSleep(2 * 1000L);

    if (fPlugSent)                  // If PLUGIN has been sent from CONN thread
    {
        // Seize the pipe
 
        DosSemRequest(hSemPipe, PIPE_SEM_WAIT);

        // Send a UNPLUG message

        phMessage.bMType = UNPLUG ;
        strcpy(phMessage.msg.pszPhoneNumber, szName);

        rc = DosWrite(hPipe,
                      (PVOID) &phMessage,
                      sizeof(PHMSG),
                      &usBytesW);
        if (rc)
            ErrorReport("Error sending UNPLUG message: DosWrite", rc);

        DosSleep(5L);
        // Release the pipe
        DosSemClear(hSemPipe);

    }

                                    //Close the pipe to server
    NetClose( );

    WinAssociateHelpInstance (NULL, hwndFrame);
    WinDestroyHelpInstance (hwndHelp);

    WinDestroyWindow (hwndFrame) ;
    WinDestroyMsgQueue (hmq) ;
    WinTerminate (hab) ;

    DosExit(EXIT_PROCESS, 0);
}


// This routine will get the metrics for the current logical font.

VOID GetCharXY (HPS hps, SHORT *pcxChar, SHORT *pcyChar, SHORT *pcyDesc)
{
    FONTMETRICS fm ;

    GpiQueryFontMetrics (hps, (LONG) sizeof fm, &fm) ;
    *pcxChar = (SHORT) fm.lAveCharWidth ;
    *pcyChar = (SHORT) fm.lMaxBaselineExt ;
    *pcyDesc = (SHORT) fm.lMaxDescender ;
}


/*
 * ClientWndProc procedure is receives messages from PM. It processes
 * the messages sent by keyboard, mouse, or Talk thread.
 */

MRESULT EXPENTRY ClientWndProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    static COLOR colBackground = 0xFFFFFFL;   // Background color: White
    static COLOR colBar        = 0x000000FFL; // Bar color: Blue
    static SHORT cxClient, cyClient; // Window coordinates
    static SHORT cySize;             // Window size along Y coordinate
    static USHORT  cLocalBar, cRemoteBar; // Size of bars
    static CHAR    szLocal[] = "LOCAL";
    static CHAR    szRemote[] = "REMOTE";
    RECTL          rcl, newrcl ;         // Paint rectangle structures
    RECTL          rclLocal, rclRemote;  // Local/Remote window rectangles
    static HWND  hwndMenu ;              // Menu handle
    USHORT rc;                           // Return code

    // Editing variables
    static BOOL  fInsertMode = FALSE ;   // Insert mode off/on
    static CHAR  *pBuffer ;              // Pointer to character buffer
    static SHORT cxChar, cyChar, cyDesc; // Character size
    static SHORT xCursor, yCursor;       // Current cursor position
    static SHORT xMax,  yMax ;           // Maximum cursor positions
    BOOL         fProcessed ;            // Messsage processing flag
    BOOL fDoubleMessage = FALSE;         // Avoid-processing-twice flag
    HPS          hps ;                   // Presentation space handle
    POINTL       ptl ;                   // Point coordinat structure
    SHORT        sRep, s ;               // Indices

    // Remote                            // Following are used for
    static CHAR *pBufferR;               // remote window.
    static SHORT xCursorR, yCursorR;
    static BOOL  fInsertModeR = FALSE;
    BOOL         fProcessedR;            // Messsage processing flag (Remote)

    CHARMESSAGE *pcChar;                 // Pointer
    PHMSG phWriteBuf;                    // Buffer for transmission
    USHORT usBytesW;                     // Bytes Written


    switch (msg)
    {
        case WM_CREATE:
            hwndMenu = WinWindowFromID (
                            WinQueryWindow (hwnd, QW_PARENT, FALSE),
                            FID_MENU) ;

            hps = WinGetPS (hwnd) ;
            EzfQueryFonts (hps) ;

            if (!EzfCreateLogFont (hps, LCID_FIXEDFONT, FONTFACE_COUR,
                                         FONTSIZE_10, 0))
            {
                WinReleasePS (hps) ;

                WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
                      "Cannot find a fixed-pitch font.  Load the Courier "
                      "fonts from the Control Panel and try again.",
                      szClientClass, 0, MB_OK | MB_ICONEXCLAMATION) ;

                return 0 ;
            }

            GpiSetCharSet (hps, LCID_FIXEDFONT) ;

            GetCharXY (hps, &cxChar, &cyChar, &cyDesc) ;

            GpiSetCharSet (hps, LCID_DEFAULT) ;
            GpiDeleteSetId (hps, LCID_FIXEDFONT) ;
            WinReleasePS (hps) ;

            return 0 ;

        case WM_INITMENU:
            switch (SHORT1FROMMP (mp1))
            {
                return 0 ;
            }
            break ;


        case WM_COMMAND:
            switch (COMMANDMSG(&msg)->cmd)
            {
                case IDM_CALL:
                    rc = WinDlgBox (HWND_DESKTOP, hwnd, CallDlgProc,
                                    (HMODULE) NULL, IDD_CALL, NULL);
                    if (rc==DISABLE) {    // Connection established

                        WinSendMsg (hwndMenu, MM_SETITEMATTR,
                                    MPFROM2SHORT (IDM_CALL, TRUE),
                                    MPFROM2SHORT (MIA_DISABLED,
                                    MIA_DISABLED)) ;
                    }
                    else
                        WinInvalidateRect (hwnd, NULL, FALSE) ;
                    return 0 ;


                case IDM_HANGUP:
                    Hangup();
                                    // Clean out the Screen

                    WinPostMsg( hwndClient,    WM_SIZE,
                                MPFROMHWND( hwndClient),
                                MPFROM2SHORT(cxClient, cyClient));

                    WinSendMsg (hwndMenu, MM_SETITEMATTR,
                                MPFROM2SHORT (IDM_CALL, TRUE),
                                MPFROM2SHORT (MIA_DISABLED, 
                                0)) ;

                    return 0;

                case IDM_EXIT:
                    WinSendMsg (hwnd, WM_CLOSE, 0L, 0L) ;
                    return 0;

                case IDM_RESUME:
                    return 0;

                case IDM_INFO:
                    WinSendMsg (hwndHelp, HM_EXT_HELP, 0L, 0L);
                    break;

                case IDM_ABOUT:
                    WinDlgBox(HWND_DESKTOP, hwnd,
                              AboutDlgProc,
                              (HMODULE) NULL,
                              IDD_ABOUT, NULL);
                    break;

            }
            break;

        case WM_ANSWER:
            rc = NetAns();

            if ((rc==ERROR) || (rc == NO))
            {
                DosSemClear(hSemBlockConn); // Unblock Conn Thread
                            WinInvalidateRect(hwnd, NULL, FALSE);
                            return 0;
            }

            WinSendMsg (hwndMenu, MM_SETITEMATTR,
                        MPFROM2SHORT (IDM_CALL, TRUE),
                        MPFROM2SHORT (MIA_DISABLED, MIA_DISABLED)) ;

            return 0 ;

            break;
        case WM_DISABLE:  // Used when error is encountered
                          // in lanman.ini or connecting to server

            WinSendMsg (hwndMenu, MM_SETITEMATTR,
                        MPFROM2SHORT (IDM_CALL, TRUE),
                        MPFROM2SHORT (MIA_DISABLED, MIA_DISABLED)) ;

            WinSendMsg (hwndMenu, MM_SETITEMATTR,
                        MPFROM2SHORT (IDM_HANGUP, TRUE),
                        MPFROM2SHORT (MIA_DISABLED, MIA_DISABLED)) ;

        case WM_SIZE:

            cxClient = SHORT1FROMMP(mp2);
            cyClient = SHORT2FROMMP(mp2);
                                               /* Bar Thickness */
            cLocalBar = cRemoteBar = (cyChar );

            WinInvalidateRect(hwnd, NULL, FALSE);

            cySize = cyClient /2 - cRemoteBar;

            xMax = cxClient / cxChar ;
            yMax = cySize / cyChar  ;
                                               /* Local */
            if (pBuffer != NULL)
                free (pBuffer) ;

            if (NULL == (pBuffer = malloc (xMax * yMax + 1)))
            {
                WinMessageBox (HWND_DESKTOP, hwnd,
                        "Cannot allocate memory for text buffer.\n"
                         "Try a smaller window.", szClientClass, 0,
                         MB_OK | MB_ICONEXCLAMATION) ;

                xMax = yMax = 0 ;
            }
            else
            {
                for (s = 0 ; s < yMax ; s++)
                    for (rc=0; rc < (USHORT) xMax; rc++)
                    {
                        BUFFER (rc, s) = ' ' ;
                    }
                    xCursor = 0 ;
                    yCursor = 0 ;
                }

                if (hwnd == WinQueryFocus (HWND_DESKTOP, FALSE))
                {
                    WinDestroyCursor (hwnd) ;

                    WinCreateCursor (hwnd, 0, cyClient -cyChar -cLocalBar,
                                     cxChar, cyChar,
                                     CURSOR_SOLID | CURSOR_FLASH, NULL) ;

                    WinShowCursor (hwnd, xMax > 0 && yMax > 0) ;
                }
                                                    /* Remote */
                if (pBufferR != NULL)
                    free (pBufferR) ;

                if (NULL == (pBufferR = malloc (xMax * yMax + 1)))
                {
                    WinMessageBox (HWND_DESKTOP, hwnd,
                         "Cannot allocate memory for text buffer.\n"
                         "Try a smaller window.", szClientClass, 0,
                         MB_OK | MB_ICONEXCLAMATION) ;

                    xMax = yMax = 0 ;
                }
                else
                {                    
                    for (s = 0 ; s <  yMax; s++)
                        for (rc=0; rc <  (USHORT) xMax; rc++)
                            RBUFFER(rc, s) = ' ' ;

                    xCursorR = 0 ;
                    yCursorR = 0 ;
                }

                return 0;

        case WM_SETFOCUS:
            if (SHORT1FROMMP (mp2))
            {
                WinCreateCursor (hwnd, cxChar * xCursor,
                                 cyClient - cLocalBar -
                                 cyChar * (1 + yCursor),
                                 cxChar, cyChar,
                                 CURSOR_SOLID | CURSOR_FLASH, NULL) ;

                WinShowCursor (hwnd, xMax > 0 && yMax > 0) ;
            }
            else
                WinDestroyCursor (hwnd) ;
            return 0 ;

        case WM_CHAR:

            if (xMax == 0 || yMax == 0)
                return 0 ;

            if (SHORT1FROMMP(mp1) & KC_KEYUP)
                return 0 ;

            if (SHORT1FROMMP(mp1) & KC_INVALIDCHAR)
                return 0 ;

            if (SHORT1FROMMP(mp1) & KC_INVALIDCOMP)
            {
                xCursor = (xCursor + 1) % xMax ;     // Advance cursor
                if (xCursor ==0)
                {
                    if (yCursor <yMax -1)
                        yCursor = (yCursor + 1);
                    else
                    {
                        yCursor = Scroll(xMax, yMax,  pBuffer);
                        WinInvalidateRect (hwnd, NULL, FALSE) ;
                    }
                }

                WinAlarm (HWND_DESKTOP, WA_ERROR) ;  // And beep
            }
                                   // Do not process the control and alt keys
            if (SHORT2FROMMP(mp2) && KC_VIRTUALKEY)
            {
                switch (SHORT2FROMMP(mp2))
                {
                    case VK_CTRL:
                    case VK_ALT:
                        return WinDefWindowProc(hwnd, msg, mp1, mp2);
                        break;
                }
            }

            if (!fConnectionMade)  // No body to talk to
            {
                WinMessageBox (HWND_DESKTOP, hwnd,
                         "No connection",
                         szClientClass, 0,
                         MB_OK | MB_ICONEXCLAMATION) ;
                return 0;
            }

            for (sRep = 0 ; sRep < (SHORT) CHAR3FROMMP(mp1) ; sRep++)
            {
                fProcessed = FALSE ;

                ptl.x = xCursor * cxChar ;
                ptl.y = cyClient -cLocalBar -
                           cyChar * (yCursor + 1) + cyDesc ;

                           /*---------------------------
                              Process some virtual keys
                             ---------------------------*/

                if (SHORT1FROMMP(mp1) & KC_VIRTUALKEY)
                {
                    fProcessed = TRUE ;

                    switch (SHORT2FROMMP(mp2))
                    {
                           /*---------------
                              Backspace key
                             ---------------*/

                        case VK_BACKSPACE:
                            if (xCursor > 0)
                            {
                                 WinSendMsg (hwnd, WM_CHAR,
                                             MPFROM2SHORT (KC_VIRTUALKEY, 1),
                                             MPFROM2SHORT (0, VK_LEFT)) ;

                                 WinSendMsg (hwnd, WM_CHAR,
                                             MPFROM2SHORT (KC_VIRTUALKEY, 1),
                                             MPFROM2SHORT (0, VK_DELETE)) ;

                                 fDoubleMessage = TRUE;
                            }

                            break ;

                           /*---------
                              Tab key
                             ---------*/

                        case VK_TAB:
                            s = min((8 -xCursor % 8),(xMax - xCursor));

                            WinSendMsg (hwnd, WM_CHAR, 
                                        MPFROM2SHORT (KC_CHAR, s),
                                        MPFROM2SHORT ((USHORT) ' ', 0)) ;
                            fDoubleMessage = TRUE;
                            break ;

                           /*-------------------------
                              Backtab (Shift-Tab) key
                            -------------------------*/

                        case VK_BACKTAB:
                            if (xCursor > 0)
                            {
                                s = (xCursor - 1) % 8 + 1 ;

                                WinSendMsg (hwnd, WM_CHAR,
                                            MPFROM2SHORT (KC_VIRTUALKEY, s),
                                            MPFROM2SHORT (0, VK_LEFT)) ;
                                fDoubleMessage = TRUE;
                             }
                             break ;

                            /*------------------------
                               Newline and Enter keys
                              ------------------------*/

                        case VK_NEWLINE:
                        case VK_ENTER:
                            xCursor = 0 ;

                            if (yCursor <yMax -1)
                                yCursor = (yCursor + 1);
                            else
                            {
                                yCursor = Scroll(xMax, yMax,  pBuffer);
                                WinInvalidateRect (hwnd, NULL, FALSE) ;
                            }
                            break ;

                        default:
                            fProcessed = FALSE ;
                            break ;
                    }
                }

                            /*------------------------
                               Process character keys
                              ------------------------*/

                if (!fProcessed && SHORT1FROMMP(mp1) & KC_CHAR)
                {
                                              // Shift line if fInsertMode
                    if (fInsertMode)
                        for (s = xMax - 1 ; s > xCursor ; s--)
                            BUFFER(s, yCursor) = BUFFER( (s - 1), yCursor) ;

                                              // Store character in buffer

                    BUFFER(xCursor, yCursor) = (CHAR) SHORT1FROMMP(mp2) ;

                                              // Display char or new line

                    WinShowCursor (hwnd, FALSE) ;
                    hps = WinGetPS (hwnd) ;

                    EzfCreateLogFont (hps, LCID_FIXEDFONT,
                                      FONTFACE_COUR, FONTSIZE_10, 0) ;
                    GpiSetCharSet (hps, LCID_FIXEDFONT) ;
                    GpiSetBackMix (hps, BM_OVERPAINT) ;

                    if (fInsertMode)
                        GpiCharStringAt (hps, &ptl,
                                         (LONG) (xMax - xCursor),
                                         & BUFFER(xCursor, yCursor)) ;
                    else
                        GpiCharStringAt (hps, &ptl, 1L,
                                         (CHAR *) & SHORT1FROMMP(mp2)) ;

                    GpiSetCharSet (hps, LCID_DEFAULT) ;
                    GpiDeleteSetId (hps, LCID_FIXEDFONT) ;
                    WinReleasePS (hps) ;
                    WinShowCursor (hwnd, TRUE) ;

                                              // Increment cursor

                    if (!(SHORT1FROMMP(mp1) & KC_DEADKEY))
                        if (0 == (xCursor = (xCursor + 1) % xMax))
                        {
                            if (yCursor <yMax -1)
                                yCursor = (yCursor + 1);
                            else
                            {
                                yCursor = Scroll(xMax, yMax,  pBuffer);
                                WinInvalidateRect (hwnd, NULL, FALSE) ;
                            }
                        }

                    fProcessed = TRUE ;
                }

                            /*--------------------------------
                               Process remaining virtual keys
                              --------------------------------*/

                if (!fProcessed && SHORT1FROMMP(mp1) & KC_VIRTUALKEY)
                {
                    fProcessed = TRUE ;

                    switch (SHORT2FROMMP(mp2))
                    {
                            /*----------------------
                               Cursor movement keys
                              ----------------------*/

                        case VK_LEFT:
                            xCursor = (xCursor - 1 + xMax) % xMax ;

                            if (xCursor == xMax -1)
                            {
                                if (yCursor  != 0)
                                    yCursor = yCursor - 1;
                                else
                                {
                                    xCursor = 0;
                                    yCursor = 0;
                                }
                            }

                            break ;

                        case VK_RIGHT:

                            xCursor = (xCursor + 1) % xMax ;

                            if (xCursor == 0)
                            {
                                if (yCursor  < yMax - 2)
                                    yCursor = yCursor + 1;
                                else
                                {
                                    yCursor = Scroll( (int) xMax,
                                                      (int) yMax,
                                                      (CHAR FAR *)pBuffer);
                                    WinInvalidateRect (hwnd, NULL, FALSE) ;
                                }
                            }
                            break ;

                        case VK_UP:
                            yCursor = max ((yCursor - 1), 0) ;
                            break ;

                        case VK_DOWN:
                            yCursor =min((yCursor + 1),(yMax - 1)) ;
                            break ;

                        case VK_PAGEUP:
                            yCursor = 0 ;
                            break ;

                        case VK_PAGEDOWN:
                            yCursor = yMax - 1 ;
                            break ;

                        case VK_HOME:
                            xCursor = 0 ;
                            break ;

                        case VK_END:
                            xCursor = xMax - 1 ;
                            break ;

                            /*------------
                               Insert key
                              ------------*/

                        case VK_INSERT:
                            fInsertMode = fInsertMode ? FALSE :TRUE ;
                            WinSetRect (hab, &rcl, 0, 0, cxClient, cyChar) ;
                            WinInvalidateRect (hwnd, &rcl, FALSE) ;
                            break ;

                            /*------------
                               Delete key
                              ------------*/

                        case VK_DELETE:
                            for (s = xCursor ; s < xMax - 1 ; s++)
                                BUFFER(s, yCursor) = BUFFER((s + 1), yCursor) ;

                            BUFFER(xMax, yCursor) = ' ' ;

                            WinShowCursor (hwnd, FALSE) ;
                            hps = WinGetPS (hwnd) ;
                            EzfCreateLogFont (hps, LCID_FIXEDFONT,
                                             FONTFACE_COUR, FONTSIZE_10, 0) ;
                            GpiSetCharSet (hps, LCID_FIXEDFONT) ;
                            GpiSetBackMix (hps, BM_OVERPAINT) ;

                            GpiCharStringAt (hps, &ptl,
                                             (LONG) (xMax - xCursor), 
                                             & BUFFER(xCursor, yCursor)) ;

                            GpiSetCharSet (hps, LCID_DEFAULT) ;
                            GpiDeleteSetId (hps, LCID_FIXEDFONT) ;
                            WinReleasePS (hps) ;
                            WinShowCursor (hwnd, TRUE) ;
                            break ;

                        default:
                            fProcessed = FALSE ;
                            break ;
                    }
                }
            }

            WinCreateCursor (hwnd, cxChar * xCursor,
                             cyClient -cLocalBar -cyChar * (1 + yCursor),
                             0, 0, CURSOR_SETPOS, NULL) ;

                    /* Now, we are going to send the WM_CHAR message
                     * to remote caller via Named Pipe. First we will
                     * seize the pipe and then write to the pipe
                     */


            if (!fDoubleMessage )
            {
                fDoubleMessage = FALSE;     // Clear flag
                // Put mp1 and mp2 parameters in buffer

                phWriteBuf.bMType = TALK;
                pcChar = (CHARMESSAGE *) &(phWriteBuf.msg.bMsg);
                pcChar->mp1 = mp1;
                pcChar->mp2 = mp2;

                if (usPhWriteBufIndex < usPhWriteBufMax-1)
                {   // Buffer not full, copy PM message in buffer

                    memcpy((VOID *) &(pPhWriteBuf[usPhWriteBufIndex]) ,
                           (VOID *)&phWriteBuf,
                           sizeof(PHMSG));
                    usPhWriteBufIndex++;
                }
                else
                {
                    // Copy the current PM message in the buffer
                    memcpy((VOID *)&(pPhWriteBuf[usPhWriteBufIndex]),
                           (VOID *)&phWriteBuf,
                           sizeof(PHMSG));

                    // Call WM_TIMER to send out the buffer

                    WinSendMsg(hwnd, WM_TIMER,
                               MPFROM2SHORT(ID_TIMER, 0),
                               0);

                }

            }

            return 0 ;

        case WM_TIMER:       // This message is received whenever
                             // either when write buffer is full
                             // or Conn thread generates a time out
                             // message.
                             // Here we send out the packet to PBX.
            if (usPhWriteBufIndex)   // If there is any data in buffer
            {
                // Seize the pipe
                rc = DosSemRequest(hSemPipe, 10*PIPE_SEM_WAIT);

                if (!rc)    // Got the pipe
                {
                    rc = DosWrite(hPipe,
                                  (PVOID)pPhWriteBuf,
                                  (usPhWriteBufIndex *sizeof(PHMSG)),
                                  &usBytesW);
                    usPhWriteBufIndex = 0;
                    // Release the pipe
                    DosSemClear(hSemPipe);

                    if ((rc) || (usBytesW==0))
                        ErrorReport("Pipe (Dos)Write", rc);

                }
                else if (rc==ERROR_SEM_TIMEOUT) {
                    /* At this time, we have not been able to
                     * seize the pipe, but we don't want to
                     * block indefinitely in Client Window
                     * either. So, we will tell the user
                     * and continue.
                     */
                    WinMessageBox (HWND_DESKTOP, hwnd,
                            "Unable to transmit character\n Retype",
                            szClientClass, 0,
                            MB_OK | MB_ICONEXCLAMATION) ;

                }
                else
                    ErrorReport("Client Window: DosSemRequest", rc);

            }
            return 0;
                                      // This message is generated from
                                      // Talk thread. Here we process
                                      // the PM message and display in
                                      // REMOTE window. The processing
                                      // is almost similar to WM_CHAR
        case WM_REMOTE:

            if (xMax == 0 || yMax == 0)
                return 0 ;

            if (SHORT1FROMMP(mp1) & KC_KEYUP)
                return 0 ;

            if (SHORT1FROMMP(mp1) & KC_INVALIDCHAR)
                return 0 ;

            if (SHORT1FROMMP(mp1) & KC_INVALIDCOMP)
            {                         // Advance cursor
                xCursorR   = (xCursorR   + 1) % xMax    ;
                if (xCursorR   ==0)
                {
                    if (yCursorR   <yMax   -1)
                        yCursorR   = (yCursorR   + 1);
                    else
                    {
                        yCursorR = Scroll(xMax, yMax,  pBufferR);
                        WinInvalidateRect (hwnd, NULL, FALSE) ;
                    }
                }

                WinAlarm (HWND_DESKTOP, WA_ERROR) ;     // And beep
            }

            for (sRep = 0 ; sRep < (SHORT )CHAR3FROMMP(mp1) ; sRep++)
            {
                fProcessedR = FALSE ;

                ptl.x = xCursorR   * cxChar ;
                ptl.y = cyClient/2 -cRemoteBar -
                            cyChar * (yCursorR + 1) + cyDesc ;

                            /*---------------------------
                               Process some virtual keys
                              ---------------------------*/

                if (SHORT1FROMMP(mp1) & KC_VIRTUALKEY)
                {
                    fProcessedR = TRUE ;

                    switch (SHORT2FROMMP(mp2))
                    {
                                /*---------------
                                   Backspace key
                                  ---------------*/

                        case VK_BACKSPACE:
                            if (xCursorR   > 0)
                            {
                                WinSendMsg (hwnd, WM_REMOTE,
                                            MPFROM2SHORT (KC_VIRTUALKEY, 1),
                                            MPFROM2SHORT (0, VK_LEFT)) ;

                                WinSendMsg (hwnd, WM_REMOTE,
                                            MPFROM2SHORT (KC_VIRTUALKEY, 1),
                                            MPFROM2SHORT (0, VK_DELETE)) ;
                            }
                            break ;

                                /*---------
                                   Tab key
                                  ---------*/

                        case VK_TAB:
                            s = min((8 - xCursorR%8),(xMax - xCursor)) ;

                            WinSendMsg (hwnd, WM_REMOTE,
                                        MPFROM2SHORT (KC_CHAR, s),
                                        MPFROM2SHORT ((USHORT) ' ', 0)) ;
                            break ;

                                /*-------------------------
                                   Backtab (Shift-Tab) key
                                  -------------------------*/

                        case VK_BACKTAB:
                            if (xCursorR   > 0)
                            {
                                s = (xCursorR   - 1) % 8 + 1 ;

                                WinSendMsg (hwnd, WM_REMOTE,
                                            MPFROM2SHORT (KC_VIRTUALKEY, s),
                                            MPFROM2SHORT (0, VK_LEFT)) ;
                            }
                            break ;

                                /*------------------------
                                   Newline and Enter keys
                                  ------------------------*/

                        case VK_NEWLINE:
                        case VK_ENTER:
                            xCursorR   = 0 ;

                            if (yCursorR   <yMax   -1)
                                yCursorR   = (yCursorR   + 1);
                            else {
                                yCursorR = Scroll(xMax, yMax,  pBufferR);
                                WinInvalidateRect (hwnd, NULL, FALSE) ;
                            }
                            break ;

                        default:
                            fProcessedR = FALSE ;
                            break ;
                    }
                }

                            /*------------------------
                               Process character keys
                              ------------------------*/

                if (!fProcessedR && SHORT1FROMMP(mp1) & KC_CHAR)
                {
                                             // Shift line if fInsertMode
                    if (fInsertModeR)
                        for (s = xMax - 1 ; s > xCursorR ; s--)
                            RBUFFER(s, yCursorR) =
                                      RBUFFER ( (s - 1), yCursorR) ;

                                             // Store character in RBUFFER

                    RBUFFER (xCursorR, yCursorR) = (CHAR) SHORT1FROMMP(mp2) ;

                                             // Display char or new line

                    hps = WinGetPS (hwnd) ;

                    EzfCreateLogFont (hps, LCID_FIXEDFONT,
                                      FONTFACE_COUR, FONTSIZE_10, 0) ;
                    GpiSetCharSet (hps, LCID_FIXEDFONT) ;
                    GpiSetBackMix (hps, BM_OVERPAINT) ;

                    if (fInsertModeR)
                        GpiCharStringAt (hps, &ptl, (LONG) (xMax - xCursorR),
                                            & RBUFFER (xCursorR, yCursorR)) ;
                    else
                        GpiCharStringAt (hps, &ptl, 1L,
                                            (CHAR *) & SHORT1FROMMP(mp2)) ;
                    GpiSetCharSet (hps, LCID_DEFAULT) ;
                    GpiDeleteSetId (hps, LCID_FIXEDFONT) ;
                    WinReleasePS (hps) ;

                                             // Increment cursor

                    if (!(SHORT1FROMMP(mp1) & KC_DEADKEY))
                        if (0 == (xCursorR   = (xCursorR   + 1) % xMax))
                        {
                            if (yCursorR < yMax - 1)
                                yCursorR = (yCursorR + 1);
                            else
                            {
                                yCursorR = Scroll(xMax, yMax,  pBufferR);
                                WinInvalidateRect (hwnd, NULL, FALSE) ;
                            }
                        }

                    fProcessedR = TRUE ;
                }

                            /*--------------------------------
                               Process remaining virtual keys
                              --------------------------------*/

                if (!fProcessedR && SHORT1FROMMP(mp1) & KC_VIRTUALKEY)
                {
                    fProcessedR = TRUE ;

                    switch (SHORT2FROMMP(mp2))
                    {
                                /*----------------------
                                   Cursor movement keys
                                  ----------------------*/

                        case VK_LEFT:

                            xCursorR = (xCursorR - 1 + xMax) % xMax ;

                            if (xCursorR == xMax -1)
                            {
                                if (yCursorR  != 0)
                                    yCursorR = yCursorR - 1;
                                else
                                {
                                    xCursorR = 0;
                                    yCursorR = 0;
                                }
                            }
                            break ;

                        case VK_RIGHT:

                            xCursorR = (xCursorR   + 1) % xMax ;

                            if (xCursorR == 0)
                            {
                                if (yCursorR < yMax   - 2)
                                    yCursorR = yCursorR + 1;
                                else
                                {
                                    yCursorR = Scroll(xMax, yMax, pBufferR);
                                    WinInvalidateRect (hwnd, NULL, FALSE) ;
                                }
                            }
                            break ;

                        case VK_UP:
                            yCursorR = max ((yCursorR   - 1), 0) ;
                            break ;

                        case VK_DOWN:
                            yCursorR = min((yCursorR   +1),(yMax   - 1)) ;
                            break ;

                        case VK_PAGEUP:
                            yCursorR = 0 ;
                            break ;

                        case VK_PAGEDOWN:
                            yCursorR = yMax   - 1 ;
                            break ;

                        case VK_HOME:
                            xCursorR   = 0 ;
                            break ;

                        case VK_END:
                            xCursorR   = xMax    - 1 ;
                            break ;

                                /*------------
                                   Insert key
                                  ------------*/

                        case VK_INSERT:
                            fInsertModeR = fInsertModeR ?FALSE :TRUE ;
                            WinSetRect (hab, &rcl, 0, 0,
                                        cxClient, cyChar) ;
                            WinInvalidateRect (hwnd, NULL, FALSE) ;
                            break ;

                                /*------------
                                   Delete key
                                  ------------*/

                        case VK_DELETE:
                            for (s = xCursorR   ; s < xMax - 1 ; s++)
                                RBUFFER (s, yCursorR) =
                                           RBUFFER ( (s + 1), yCursorR) ;

                            RBUFFER (xMax, yCursorR) = ' ' ;

                            hps = WinGetPS (hwnd) ;
                            EzfCreateLogFont (hps, LCID_FIXEDFONT,
                                              FONTFACE_COUR, FONTSIZE_10, 0);
                            GpiSetCharSet (hps, LCID_FIXEDFONT) ;
                            GpiSetBackMix (hps, BM_OVERPAINT) ;

                            GpiCharStringAt (hps, &ptl,
                                             (LONG) (xMax    - xCursorR),
                                             & RBUFFER (xCursorR, yCursorR));

                            GpiSetCharSet (hps, LCID_DEFAULT) ;
                            GpiDeleteSetId (hps, LCID_FIXEDFONT) ;
                            WinReleasePS (hps) ;
                            break ;

                        default:
                            fProcessedR = FALSE ;
                            break ;
                    }
                }
            }
            return 0 ;

        case WM_PAINT:
            hps = WinBeginPaint (hwnd, NULL, NULL) ;
            GpiSavePS (hps) ;
            GpiErase (hps) ;

            GpiCreateLogColorTable (hps, 0L, LCOLF_RGB, 0L, 0L, NULL) ;

            WinQueryWindowRect (hwnd, &rcl) ;

            WinFillRect (hps, &rcl, colBackground);

            /* now draw the color bars */

            newrcl.xLeft   = rcl.xLeft;
            newrcl.xRight  = rcl.xRight;
            newrcl.yTop    = rcl.yTop;
            newrcl.yBottom = newrcl.yTop - cLocalBar;

            rclLocal.xLeft   = rcl.xLeft;
            rclLocal.xRight  = rcl.xRight;
            rclLocal.yTop    = rcl.yTop - cLocalBar;
            rclLocal.yBottom = rclLocal.yTop - cySize;

            WinFillRect(hps, &newrcl, colBar);

            rc = WinDrawText(hps,
                             strlen(szLocal),
                             szLocal,
                             &newrcl,
                             colBackground,
                             colBar,
                             DT_TOP | DT_BOTTOM | DT_CENTER);

            newrcl.xLeft   = rcl.xLeft;
            newrcl.xRight  = rcl.xRight;
            newrcl.yTop    = rcl.yTop /2 ;
            newrcl.yBottom = newrcl.yTop - cRemoteBar;

            rclRemote.xLeft  = rcl.xLeft;
            rclRemote.xRight = rcl.xRight;
            rclRemote.yTop   = rcl.yBottom + cySize;
            rclRemote.yBottom  = rcl.yBottom;

            WinFillRect(hps, &newrcl, colBar);

            rc = WinDrawText(hps,
                              strlen(szRemote),
                              szRemote,
                              &newrcl,
                              colBackground,
                              colBar,
                              DT_TOP | DT_BOTTOM | DT_CENTER);


                                    // Define font and paint characters

            EzfCreateLogFont (hps, LCID_FIXEDFONT, FONTFACE_COUR,
                                  FONTSIZE_10, 0) ;
            GpiSetCharSet (hps, LCID_FIXEDFONT) ;

            if (xMax > 0 && yMax > 0)
            {
                for (s = 0 ; s < yMax ; s++)
                {
                    ptl.x = 0  ;
                    ptl.y = cyClient - cLocalBar - cyChar * (s + 1) + cyDesc ;

                    GpiCharStringAt (hps, &ptl, (LONG) xMax, & BUFFER (0, s)) ;
                }
            }
            if (xMax > 0 && yMax > 0)
            {
                for (s = 0 ; s < yMax ; s++)
                {
                    ptl.x = 0  ;
                    ptl.y = cyClient/2 - cRemoteBar -
                                 cyChar * (s + 1) + cyDesc ;

                    GpiCharStringAt (hps, &ptl, (LONG) xMax,
                                        & RBUFFER (0, s)) ;
                }
            }

            GpiSetCharSet (hps, LCID_DEFAULT) ;
            GpiDeleteSetId (hps, LCID_FIXEDFONT) ;

            GpiRestorePS (hps, -1L) ;
            WinEndPaint (hps) ;
            return 0 ;

        case WM_DESTROY:
            return 0 ;
    }
    return WinDefWindowProc (hwnd, msg, mp1, mp2) ;
}


// AboutDlgProc is called when user hits the About key on menu.

MRESULT EXPENTRY AboutDlgProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    switch (msg)
    {
        case WM_COMMAND:
            switch (COMMANDMSG(&msg)->cmd)
            {
                case DID_OK:
                case DID_CANCEL:
                    WinDismissDlg (hwnd, TRUE) ;
                    return 0 ;
            }
            break ;
    }
    return WinDefDlgProc (hwnd, msg, mp1, mp2) ;
}


// CallDlgProc is called when user selects CALL option on menu. It
// shows the current users registered with PBX and lets user establish
// connection to a remote person.

MRESULT EXPENTRY CallDlgProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    static CHAR  szBuffer[80], szMessage[80] ;
    USHORT       rc, i;
    CHAR         *pcResult;
    SHORT        sSelect;      // ListBox Selected
    switch (msg)
    {
        case WM_INITDLG:

            // Fill up the Directory List
            if (!FillDirectory(hwnd))
            {
                WinDismissDlg (hwnd, FALSE) ;
                return 0;
            }
            // Set limit on text length
            WinSendDlgItemMsg (hwnd, IDD_FILEEDIT,
                               EM_SETTEXTLIMIT,
                               MPFROM2SHORT (PHNUMSIZE, 0),
                               NULL) ;
            //Set focus on list Box
            WinSetFocus(HWND_DESKTOP,
                        WinWindowFromID(hwnd, IDD_NAMELIST));

            return 0 ;

        case WM_CONTROL:
            if (SHORT1FROMMP (mp1)==IDD_NAMELIST)
            {
                       // List box message
                       // Find out the list box number
                sSelect = (SHORT) WinSendDlgItemMsg(hwnd,
                                                    SHORT1FROMMP(mp1),
                                                    LM_QUERYSELECTION,
                                                    0L, 0L);
                       // Find out the text string
                WinSendDlgItemMsg(hwnd,
                                  SHORT1FROMMP(mp1),
                                  LM_QUERYITEMTEXT,
                                  MPFROM2SHORT (sSelect, sizeof szBuffer),
                                  MPFROMP (szBuffer));

            }
            switch (SHORT1FROMMP(mp1))            // Control ID
            {

                case IDD_NAMELIST:                // Directory
                    switch (SHORT2FROMMP(mp1))    // Notification code
                    {
                        case  LN_SELECT:              // User selected a name
                            WinSetDlgItemText( hwnd, IDD_FILEEDIT, szBuffer);
                            return 0;

                        case LN_ENTER:                // User hit enter key

                            WinSendMsg(hwnd,
                                       WM_COMMAND,
                                       MPFROM2SHORT(DID_OK,0),
                                       MPFROM2SHORT(CMDSRC_OTHER, FALSE));
                            return 0;

                    }
                    break;
            }

        case WM_COMMAND:
            switch (COMMANDMSG(&msg)->cmd)
            {
                case DID_OK:
                    pcResult = memset(szBuffer, 0x00, sizeof(szBuffer));

                    WinQueryDlgItemText (hwnd, IDD_FILEEDIT,
                                         sizeof szBuffer, szBuffer) ;

                    if (strlen(szBuffer)==0)
                    {                                    /* NULL username */
                        WinDismissDlg (hwnd, FALSE) ;
                        return 0 ;
                    }

                    // Convert to to upper case
                    for (i=0; i < CNLEN; i++ )
                         szBuffer[i] = (CHAR) toupper (szBuffer[i]);
                    szBuffer[CNLEN] = '\0';

                    // Cannot call yourself
                    if (!strcmp(szName, szBuffer))
                    {
                        WinMessageBox (HWND_DESKTOP, hwnd,
                                   "You can not call yourself",
                                    szClientClass, 0,
                                    MB_OK | MB_ICONEXCLAMATION) ;
                        WinDismissDlg (hwnd, FALSE) ;
                        return 0 ;
                    }

                    sprintf(szMessage, "%s%s", "Calling ", szBuffer);
                    WinSendDlgItemMsg (hwnd, IDD_FILEEDIT,
                                  EM_SETTEXTLIMIT,
                                  MPFROM2SHORT ( 30, 0),
                                  NULL) ;

                    WinSetDlgItemText(hwnd, IDD_FILEEDIT, szMessage);
                    WinSendDlgItemMsg (hwnd, IDD_FILEEDIT,
                                  EM_SETTEXTLIMIT,
                                  MPFROM2SHORT (PHNUMSIZE, 0),
                                  NULL) ;

                    szMessage[0]= '\0';


                    rc = NetCall(hwnd, szBuffer);

                    if (rc == YES)   // Connection established
                    {
                        // Disable Call options
                        WinDismissDlg (hwnd, DISABLE) ;
                        return 0;
                    }

                    WinDismissDlg (hwnd, FALSE) ;
                    return 0 ;


                case DID_CANCEL:
                    WinDismissDlg (hwnd, FALSE) ;
                    return 0 ;
            }
            break;
    }
    return WinDefDlgProc (hwnd, msg, mp1, mp2) ;
}


// FillDirectory makes a LISTQUERY call to router which supplies the name
// of all registered users.

BOOL FillDirectory( HWND hwnd)
{
    PHMSG  phMessage;           // Phone message
                                // Buffer for Phone list
    CHAR   chBuffer[sizeof(LISTMSG)+PHNUMSIZE*MAXLINES];
    CHAR   *szNameList;         // Pointer to names in phone list
    LISTMSG  *lsListMsg;        // LISTMSG pointer
    USHORT  usCnt = 0;          // Name list index
    USHORT  rc;                 // Return Code
    USHORT  usBytesW;           // Bytes writeen
    USHORT  usBytesR;           // Bytes read

                    // First clean the list box
    WinSendDlgItemMsg(hwnd, IDD_NAMELIST,
                      LM_DELETEALL, NULL,  NULL);

    // Now, we make a LISTQUERY call to router to get the list of phone
    // numbers of people who have registered.

    phMessage.bMType = LISTQUERY;
    strcpy(phMessage.msg.pszPhoneNumber, szName);

    // Seize the pipe
    DosSemRequest(hSemPipe, SEM_INDEFINITE_WAIT);
    rc = DosWrite(hPipe,
                  (PVOID) &phMessage,
                  sizeof (PHMSG),
                  &usBytesW);
    if (rc)
    {
        ErrorReport("ListQuery: DosWrite", rc);
        return FALSE;
    }

    // Read off the LISTQUERY message and get the phone list

    rc = DosRead(hPipe,
                 (PVOID) chBuffer,
                 sizeof(chBuffer),
                 &usBytesR);
    if (rc)
    {
        ErrorReport("ListQuery: DosRead", rc);
        return FALSE;
    }

    // Check Response field
    lsListMsg = (LISTMSG *) chBuffer;
    if (lsListMsg->usResponse != NO_ERROR)
    {
       ErrorReport("Error in getiing phone list",lsListMsg->usResponse);
       return FALSE;
    }

    // Load the names in list box
    while (usCnt < lsListMsg->usNumberCnt)
    {
        // Pointer to next name
        szNameList = (char *) ((char *) chBuffer + sizeof(LISTMSG) +
                              usCnt*PHNUMSIZE);
        //Insert the new name
        WinSendDlgItemMsg(hwnd,IDD_NAMELIST,
                          LM_INSERTITEM,
                          MPFROM2SHORT(LIT_SORTASCENDING, 0),   //SORTED
                          MPFROMP(szNameList));
        usCnt++;
    }


    // Release the pipe
    DosSemClear(hSemPipe);
    return TRUE;
}


// Signal handler for PHONE program. It is invoked when KILL_PROCESS
// is generated.

VOID PASCAL FAR SignalHandler(usSigArg, usSigNum)
USHORT usSigArg;    /* furnished by DosFlagProcess if appropriate */
USHORT usSigNum;    /* signal number being processed              */
{

    /* Here we post a WM_CLOSE to the client to shut itself down. We could
       also post a SYS_COMMAND/SC_CLOSE to the frame, but the close item in
       the system menu must be enabled for that to work, and it just posts
       a WM_CLOSE anyway. */

    WinPostMsg(hwndClient, WM_CLOSE, 0L, 0L);

    // Acknowledge receiving the signal.

    DosSetSigHandler ( NULL,
                       NULL,
                       NULL,
                       SIGA_ACKNOWLEDGE,
                       SIG_KILLPROCESS);
    return;

    usSigNum;
    usSigArg;   /* This is just for the compiler. */
}
