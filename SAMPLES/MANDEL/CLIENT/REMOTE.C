/*
 *  REMOTE.C --
 *
 *  Code to do the remote calculations for the Windows Mandelbrot Set
 *  distributed drawing program.
 *
 *  Copyright (C) 1990 Microsoft Corporation.
 *
 *  Information coming into this module (via API calls) is based on
 *  upper-left being (0,0) (the Windows standard). We translate that
 *  to PM standard (lower-left is (0,0) ) before we ship it out onto
 *  the net, and we do reverse translations accordingly.
 *
 *  Two things are going on here. First, mailslots are used to find
 *  server computers. A mailslot packet is broadcast and any available
 *  servers reply. The names of the computers that reply are then placed
 *  in a server table.
 *
 *  Second, work is sent to the servers available. A named pipe is
 *  connected to each. This is used to send out calcbuf requests, and
 *  read back the iteration data. This data is then passed back to the
 *  main window procedure (by means of a WM_PAINTLINE message) which
 *  draws the picture.
 *
 *  A word about the shared buffer: multiple buffers could be used, but
 *  a single one is used to keep the RAM complexity down. The buffer is
 *  requested in this code, and then released after the data has been
 *  drawn (in PaintLine() in mandel.c). So long as the painting is done
 *  quickly, this is very efficient.
 *
 *  This code sample is provided for demonstration purposes only.
 *  Microsoft makes no warranty, either express or implied,
 *  as to its usability in any given situation.
 */


#define  INCL_NETERRORS
#define  INCL_NETMAILSLOT
#define  INCL_NETWKSTA
#include <lan.h>                    /* LAN Manager header files */
#include <nmpipe.h>

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <share.h>
#include <io.h>

#include <windows.h>

#include "mandel.h"
#include "calc.h"
#include "remote.h"
#include "rmprot.h"
#include "debug.h"


/*
 * Tuning paramters
 */

#define SVR_TABLE_SZ        20
#define MAX_PIPENAME_SZ     CCHMAXPATH

#define PIPEREAD_BUFSIZE    (HEIGHT * sizeof(long) * iLines)
#define MAX_BUFSIZE         (HEIGHT * sizeof(long) * MAXLINES)


/*
 * Manifests, to keep everything neat.
 */

char szErrTitle[] = "Mandelbrot Initialization";
char szErrWksta[] = "Could not get workstation info, err == %hu.";
char szErrMailslot[] = "Could not create a mailslot, err == %hu.";
char szErrLogon[] = "You are not logged on.";


static char szLocal[] = "Local machine";

// Size of a buffer needed for wksta_info_10

#define WKI10_SZ    (sizeof(struct wksta_info_10) + CNLEN + UNLEN \
                        + (6 * DNLEN) + 8)

extern int errno;           // errno from c runtime
extern int _doserrno;       // dos error code

/*
 * Data structures
 */

static svr_table    SvrTable[SVR_TABLE_SZ]; // the table
static int          SvrTableSz = 0;         // # of objects in it


// The local wksta name
static char         szWkstaName[CNLEN+1];
static char         szDomain[6][DNLEN+1];
static short        cDomains = 0;


// The mailslot for replies to polls for servers
static unsigned     hMailslot;


// Do we do local work?
BOOL    fLocalWork = TRUE;
BOOL    fRemoteWork = FALSE;


// Picture information
static int      cPictureID = 0;     // picture id, in case we reset in the middle
static CPOINT   cptLL;              // upper-left
static double   dPrecision;         // precision of draw
static RECTL    rclPicture;         // rectangle defining client window
static DWORD    dwCurrentLine;      // next line to be drawn
static DWORD    dwThreshold;        // threshold for iterations


/*
 * function prototypes for local procs
 */

DWORD CalcThreshold( double );



/*
 *  InitRemote --
 *
 *  This function initializes everything for our remote connections.
 *  It gets the local wksta name (making sure the wksta is started)
 *  and it creates the mailslot with which to collect replies to our poll.
 *
 *  RETURNS
 *
 *      TRUE        - initialization succeeded
 *      FALSE       - initialization failed, can't go on
 */


BOOL
InitRemote( HWND hwnd )
{
    char buf[WKI10_SZ];
    char buf2[6 * (DNLEN+1)];
    struct wksta_info_10 * wki = (struct wksta_info_10 *) buf;
    unsigned short  res;
    unsigned short  avail;
    char    charbuf[128];

    Message("InitRemote: start");

    // read the wksta name
    res = NetWkstaGetInfo(NULL, 10, buf, sizeof(buf), &avail);

    if (res)
    {
        Message("InitRemote: NetWkstaGetInfo failed: %hu", res);
        wsprintf(charbuf, szErrWksta, res);
        MessageBox(hwnd, charbuf, szErrTitle, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    if (wki->wki10_username[0] == '\0')
    {
        Message("InitRemote: not logged on");
        MessageBox(hwnd, szErrLogon, szErrTitle, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    // and remember it
    wsprintf(szWkstaName, "%s", wki->wki10_computername);

    // now do the domains
    wsprintf(szDomain[cDomains++], "%s", wki->wki10_langroup);
    if (wki->wki10_logon_domain != NULL)
        sprintf(szDomain[cDomains++], "%Fs", wki->wki10_logon_domain);

    if (wki->wki10_oth_domains != NULL)
    {
        char *  tok;

        sprintf(buf2, "%Fs", wki->wki10_oth_domains);
        tok = strtok(buf2, " ");
        while ( tok != NULL)
        {
            strcpy(szDomain[cDomains++], tok);
            tok = strtok(NULL, " ");
        }
    }



    // now create our mailslot
    res = DosMakeMailslot(MAILSLOT_REPLY, sizeof(ECHOPACKET),
                           SVR_TABLE_SZ * sizeof(ECHOPACKET),
                           &hMailslot);

    // without this, we're doomed
    if (res)
    {
        Message("InitRemote: dosMakeMailslot failed: %hu", res);
        wsprintf(charbuf, szErrMailslot, res);
        MessageBox(hwnd, charbuf, szErrTitle, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }


    // set up our local entry
    SvrTableSz++;
    strcpy(SvrTable[0].name, szLocal);
    SvrTable[0].iStatus = SS_LOCAL;


    PollForServers();

    // good, we succeeded
    return TRUE;
}



/*
 *  PollForServers --
 *
 *  This functions sends out a broadcast request for Mandelbrot
 *  servers to identify themselves.
 *
 *  This is only done if there is room in the server table for more servers.
 */

void PollForServers( void )
{

    static char        szMsName[PATHLEN] = "";  // our mailslot name
    ECHOPACKET  ep;                      // a packet we write
    unsigned short     res;
    int  i;


    // Only do this if there is room

    if (SvrTableSz < SVR_TABLE_SZ)
    {
        // This is the echopacket sent
        ep.wType = TYPE_ECHOPACKET;
        strcpy(ep.szServerName, "\\\\");
        strcat(ep.szServerName, szWkstaName);

        for (i = 0; i < (int)cDomains; i++)
        {
            // This is the mailslot written to
            strcpy(szMsName, "\\\\");
            strcat(szMsName, szDomain[i]);
            strcat(szMsName, MAILSLOT_REQUEST);

            // Broadcast a request
            res = DosWriteMailslot(szMsName, (char far *) &ep, sizeof(ep),
                         5,2,0L);

            Message("PollForServers: %s returns %hu", szMsName, res);
        }
    }
}


/*
 *  CheckPoll --
 *
 *  This function checks our mailslot for replies. If it finds any, it
 *  adds the new server names to the server table.
 */

void CheckPoll( void )
{

    unsigned            res = 0;
    static ECHOPACKET   ep;
    unsigned short      cbReturned;
    unsigned short      cbNext;
    unsigned short      usPriority;
    int                 i;


    while (res == 0)
    {
        // Try to read a reply
        res = DosReadMailslot(hMailslot,
                                (char far *) &ep,
                                &cbReturned,
                                &cbNext,
                                &usPriority,
                                0L);

        // If failed, return
        if (res)
            return;

        // Add server to the list
        for (i = 0; i < SvrTableSz; ++i)
        {
            if (!strcmp(SvrTable[i].name, ep.szServerName))
                break;
        }

        // If no match, add it
        if (i == SvrTableSz)
	{
            SvrTableSz++;

            strcpy(SvrTable[i].name, ep.szServerName);  // copy the name

            Message("Adding new server: %s", SvrTable[i].name);

            SvrTable[i].iStatus = SS_DISCONN;           // set new status
            SvrTable[i].fTried = FALSE;
        }

        // Loop around and try to read the rest
    }

    // No more to read

}


/*
 *  CheckPipeStatus --
 *
 *  This function does a check of all of the pipes looking for work.
 *
 *  If it finds a disconnected pipe, it tries to connect it.
 *  If it finds an idle pipe, and there is work to be done, it assigns
 *      a line, and writes out the request.
 *  If it finds a read-pending pipe, it checks if the read has completed.
 *      If it has, it is read and a message is sent so the read data can
 *      be processed.
 *
 *  RETURNS
 *      TRUE        - we did a piece of work
 *      FALSE       - we could not find any work to do.
 */

BOOL CheckPipeStatus( HWND hwnd,
                      BOOL fTimer,
                      WORD wIteration)
{

    static int  iWork = 0;
    int         iLast;
    unsigned short res;
    char        buf[PATHLEN+1];
    int         handle;
    CALCBUF     cb;
    unsigned short usRead;
    unsigned short avail;
    unsigned short state;
    char *      drawbuf;
    char        smallbuf[4];
    HCURSOR     hcursor;


    // If no pipes, forget it
    if (SvrTableSz == 0)
        return FALSE;

    // Move on from where we left off

    iLast = iWork;

    while ( TRUE )
    {
        iWork++;

        if (iWork == SvrTableSz)
            iWork = 0;

        // Check the status
        switch(SvrTable[iWork].iStatus) {

        case SS_PAINTING:
            break;

        case SS_DISCONN:
            // Disconnected; try to connect it

            // Only do this on timed polls
            if ( SvrTable[iWork].fTried == TRUE )
                break;

            if (!fRemoteWork)
                break;

            SvrTable[iWork].fTried = TRUE;

            // Assemble the path name
            strcpy(buf, SvrTable[iWork].name);
            strcat(buf, PIPE_SERVER);

            hcursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
            ShowCursor(TRUE);

            handle = sopen(buf, O_RDWR | O_BINARY, SH_DENYWR);

            ShowCursor(FALSE);
            SetCursor(hcursor);

            if (handle != -1)   // Success!
            {
                SvrTable[iWork].hfPipe = handle;
                SvrTable[iWork].iStatus = SS_IDLE;
                return TRUE;
            }

            Message("Open failed: %hu", _doserrno);

            return TRUE;    // This might have taken a while


        case SS_IDLE:
            // Idle; assign it a piece of work
            if ((long)dwCurrentLine > rclPicture.xRight)
                break;

            if (!fRemoteWork) {
                close(SvrTable[iWork].hfPipe);
                SvrTable[iWork].iStatus = SS_DISCONN;
                return TRUE;
            }

            cb.rclDraw.xLeft = dwCurrentLine;
            cb.rclDraw.xRight = dwCurrentLine + iLines - 1;
            cb.rclDraw.yTop = rclPicture.yTop;
            cb.rclDraw.yBottom = rclPicture.yBottom;
            cb.dblPrecision = dPrecision;
            cb.dwThreshold = dwThreshold;
            cb.cptLL = cptLL;

            // Write out the request on the pipe

            res = write(SvrTable[iWork].hfPipe, (void *) &cb, sizeof(cb));
            if (res < sizeof(cb)) {
                close(SvrTable[iWork].hfPipe);
                SvrTable[iWork].iStatus = SS_DISCONN;
                return TRUE;
            }

            // Okay, it succeeded

            SvrTable[iWork].dwLine = dwCurrentLine;
            dwCurrentLine += iLines;
            SvrTable[iWork].iStatus = SS_READPENDING;
            SvrTable[iWork].cPicture = cPictureID;
            SvrTable[iWork].cLines = iLines;

            return TRUE;

        case SS_LOCAL:
            // Do a chunk of work locally

            if (!fLocalWork)
                break;

            if ((long)dwCurrentLine > rclPicture.xRight)
                break;

            if (!TakeDrawBuffer())
                break;

            cb.rclDraw.xLeft = dwCurrentLine;
            cb.rclDraw.xRight = dwCurrentLine+ iLines-1;
            cb.rclDraw.yTop = rclPicture.yTop;
            cb.rclDraw.yBottom = rclPicture.yBottom;

            MandelCalc(&cptLL, &(cb.rclDraw), dPrecision, dwThreshold, NULL);

            SvrTable[iWork].cPicture = cPictureID;
            SvrTable[iWork].dwLine = dwCurrentLine;
            SvrTable[iWork].cLines = iLines;

            PostMessage(hwnd, WM_PAINTLINE,
                        (WORD)(svr_table near *)&(SvrTable[iWork]),
                        0L);
            dwCurrentLine += iLines;
            return TRUE;


        case SS_READPENDING:
            // Read pending; see if it's completed


            // If we switched pictures, we're outta sync; skip it
            if (SvrTable[iWork].cPicture < cPictureID) {
                close(SvrTable[iWork].hfPipe);
                SvrTable[iWork].iStatus = SS_DISCONN;
                return TRUE;
            }

            res = DosPeekNmPipe( SvrTable[iWork].hfPipe,
                                 smallbuf,
                                 sizeof(smallbuf),
                                 &usRead,
                                 (PAVAILDATA)&avail,
                                 &state);

            if ( (res) || (state != NP_CONNECTED)) {
                close(SvrTable[iWork].hfPipe);
                SvrTable[iWork].iStatus = SS_DISCONN;
                dwCurrentLine = SvrTable[iWork].dwLine; // back up
                return TRUE;
            }

            // If it's not all there yet, forget it
            if (avail < sizeof(CALCBUF))
                break;

            // If can't get the shared buffer, try again later
            if (!TakeDrawBuffer())
                break;

            // Get a locked pointer to the buffer
            drawbuf = (char *) GetDrawBuffer();
            if (drawbuf == NULL) {
                ReturnDrawBuffer();
                break;
            }

            // Do the read
            res = read(SvrTable[iWork].hfPipe, drawbuf,
                        (HEIGHT * sizeof(long) * SvrTable[iWork].cLines));

            // If picture has changed, forget about it
            if (cPictureID != SvrTable[iWork].cPicture)
            {
                FreeDrawBuffer();
                ReturnDrawBuffer();
                SvrTable[iWork].iStatus = SS_IDLE;
                return TRUE;
            }

            if (res == -1) {
                FreeDrawBuffer();
                ReturnDrawBuffer();
                close(SvrTable[iWork].hfPipe);
                SvrTable[iWork].iStatus = SS_DISCONN;
                dwCurrentLine = SvrTable[iWork].dwLine; // back up
                return TRUE;
            }

            // Read succeeded; post a message so it will be painted

            PostMessage(hwnd, WM_PAINTLINE,
                        (WORD)(svr_table near *)&(SvrTable[iWork]), 0L);
            SvrTable[iWork].iStatus = SS_PAINTING;
            FreeDrawBuffer();
            return TRUE;
        }


        // If we made the full loop, we're done
        if (iWork == iLast) {
            return FALSE;
        }
    }
}


/*
 *  SetNewCalc --
 *
 *  This sets up new information for a drawing and
 *  updates the drawing ID so any calculations in progress will not
 *  be mixed in.
 */

void SetNewCalc( CPOINT cptUL, double dPrec, RECT rc)
{

    /*
     *  First, the base point. We need to translate from upper left to
     *  lower left.
     */

    cptLL.real = cptUL.real;
    cptLL.imag = cptUL.imag - (dPrec * (rc.bottom - rc.top));

    // Now the precision
    dPrecision = dPrec;

    // The rectangle. Once again, translate.
    rclPicture.xLeft = (long) rc.left;
    rclPicture.xRight = (long) rc.right;
    rclPicture.yBottom = (long) rc.top;
    rclPicture.yTop = (long) rc.bottom;

    // Current line, start of drawing
    dwCurrentLine = rclPicture.xLeft;

    dwThreshold = CalcThreshold(dPrecision);

    // Picture id incremented, to prevent confusion
    cPictureID++;
}


/*
 *  CheckDrawing --
 *
 *  Just a sanity check here -- a function to check to make sure that we're
 *  on the right drawing
 */

BOOL
CheckDrawingID( int id)
{
    return (id == cPictureID) ? TRUE : FALSE;
}


/*
 *  RetryConnections ---
 *
 *  This function resets the fTried bit on each disconnected pipe
 *  so that we try a reconnect.
 */

void
RetryConnections( void )
{
    int     i;

    for (i = 0; i < SvrTableSz; ++i)
        SvrTable[i].fTried = FALSE;
}


/*
 *  TakeDrawBuffer/ GetDrawBuffer/ FreeDrawBuffer / ReturnDrawBuffer
 *
 *  These functions hide a handle to a buffer of memory.
 *
 *  TakeDrawBuffer ensures only one pipe read at a time.
 *  GetDrawBuffer locks the handle and returns a pointer.
 *  FreeDrawBuffer unlocks the handle.
 *  ReturnDrawBuffer unlocks the handle and lets another pipe read go.
 */

static BOOL fBufferTaken = FALSE;
static HANDLE hSharedBuf = NULL;


BOOL
TakeDrawBuffer( void )
{

    if (fBufferTaken)
    {
        Message("TakeDrawBuffer: conflict");
        return FALSE;
    }

    if (hSharedBuf == NULL)
    {
        hSharedBuf = LocalAlloc(LMEM_MOVEABLE, MAX_BUFSIZE);
        if (hSharedBuf == NULL)
            return FALSE;
    }
    fBufferTaken = TRUE;
    return TRUE;
}



PDWORD
GetDrawBuffer( void )
{

    if (hSharedBuf == NULL)
        return NULL;

    return (PDWORD) LocalLock(hSharedBuf);
}



void
FreeDrawBuffer( void )
{
    LocalUnlock(hSharedBuf);
}


void
ReturnDrawBuffer( void )
{
    fBufferTaken = FALSE;
}



/*
 *  CalcThreshold --
 *
 *  We need an iteration threshold beyond which we give up. We want it to
 *  increase the farther we zoom in. This code generates a threshold value
 *  based on the precision of drawing.
 *
 *  RETURNS
 *
 *      threshold calculated based on precision
 */


DWORD   CalcThreshold(double precision)
{
    DWORD   thres = 25;
    double  multiplier = (double) 100;

    /* for every 100, multiply by 2 */
    while ( (precision *= multiplier) < (double)1)
        thres *= 2;

    return thres;
}



/*
 *  QueryThreshold --
 *
 *  Callback for finding out what the current drawing's threshold is.
 */

DWORD QueryThreshold( void )
{
    return dwThreshold;
}


/*
 *
 *  GetServerCount --
 *
 *  Returns the number of servers in the table.
 */

int
GetServerCount( void )
{
    return (int) SvrTableSz-1;
}


/*
 *  GetServerName --
 *
 *  Copied the name of the ith server into the buffer provided.
 */

void
GetServerName( int      i,
               char *   buf)
{

    if (i >= SvrTableSz-1)
        buf[0] = '\0';
    else if (SvrTable[i+1].iStatus != SS_DISCONN)
        strcpy(buf, SvrTable[i+1].name);
    else {
        strcpy(buf, "(");
        strcat(buf, SvrTable[i+1].name);
        strcat(buf, ")");
    }
}
