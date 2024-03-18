/****************************************************************************

    SERVER.C --

    Code for the server side of the Windows Mandelbrot Set distributed
    drawing program.

    Copyright (C) 1990 Microsoft Corporation.

    This code sample is provided for demonstration purposes only.
    Microsoft makes no warranty, either express or implied,
    as to its usability in any given situation.

****************************************************************************/

#define  INCL_DOS
#define  INCL_DOSERRORS
#define  INCL_DOSSIGNALS
#include <os2.h>

#define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <stddef.h>
#include <string.h>

#define  INCL_NETMAILSLOT
#define  INCL_NETSERVICE
#define  INCL_NETERRORS
#define  INCL_NETWKSTA
#include <lan.h>

#include "calc.h"
#include "remote.h"
#include "utils.h"

#define   MAX_BUFSIZE   (sizeof(ECHOPACKET))
#define   MAX_MAILSLOT	(sizeof(ECHOPACKET) * 5)

#define   STACKSIZE	4096
#define   THREADS       5


#define   SEM_TIMEOUT   30000L	 // timeout is 30 seconds on read

void InitEchoThread( void );
void far EchoThread( void *arg );
void far ServerThread( void *arg );

static HPIPE hPipeTable[THREADS];

static long lSemQuit = 0L;
static HSEM hsemQuit = (HSEM) &lSemQuit;

static long lSemCounter = 0L;
static HSEM hsemCounter = (HSEM) &lSemCounter;

static BOOL fQuit = FALSE;
static USHORT cThreads;
static unsigned hMailslot;
static unsigned usErrCode = 0;


extern VOID FAR PASCAL sig_handler(unsigned, unsigned);
void EndThread( void );
void ErrEnd( void );

void
main( int	argc,
      char *	argv[])
{

    USHORT  i;
    PFNSIGHANDLER pfnPrev;
    unsigned short prevact;
    unsigned err;
    struct service_status ss;

    DosError(0);

    DosSetSigHandler((PFNSIGHANDLER)sig_handler, &pfnPrev,
                        &prevact, SIGA_ACCEPT,
                        SERVICE_RCV_SIG_FLAG);

    // we don't need these file handles
    DosClose(0);
    DosClose(1);
    DosClose(2);

    DosSetSigHandler(NULL, &pfnPrev, &prevact, SIGA_IGNORE, 1);
    DosSetSigHandler(NULL, &pfnPrev, &prevact, SIGA_IGNORE, 3);
    DosSetSigHandler(NULL, &pfnPrev, &prevact, SIGA_IGNORE, 4);
    DosSetSigHandler(NULL, &pfnPrev, &prevact, SIGA_ERROR, 6);
    DosSetSigHandler(NULL, &pfnPrev, &prevact, SIGA_ERROR, 7);

    // start initialization

    ss.svcs_pid = 0;
    ss.svcs_code = 0;
    ss.svcs_status = SERVICE_NOT_PAUSABLE | SERVICE_UNINSTALLABLE
                        | SERVICE_INSTALL_PENDING;
    ss.svcs_code = 0;
    ss.svcs_text[0] = 0;

    err = NetServiceStatus( (char far *) &ss, sizeof(ss));

    if (err) {
        exit(err);
    }

    // command-line arguments

    // get everything running

    InitEchoThread();

    cThreads = 0;

    for (i = 0; i < (THREADS-1); ++i)
        _beginthread( ServerThread, NULL, STACKSIZE, NULL);


    // okay, now say we're installed

    ss.svcs_status = SERVICE_NOT_PAUSABLE | SERVICE_UNINSTALLABLE
                        | SERVICE_INSTALLED;

    err = NetServiceStatus( (char far *) &ss, sizeof(ss));

    if (err) {
        exit(err);
    }

    // wait around for somebody to uninstall us

    DosSemSet(hsemQuit);

    do {
        err = DosSemWait(hsemQuit, -1L);
    } while (err == ERROR_INTERRUPT);

    ss.svcs_status = SERVICE_NOT_PAUSABLE | SERVICE_UNINSTALLABLE
                        | SERVICE_UNINSTALL_PENDING;

    err = NetServiceStatus( (char far *) &ss, sizeof(ss));
    if (err) {
        exit(err);
    }


    fQuit = TRUE;

    // close all pipes
    for (i = 0; i < THREADS; i++)
        DosClose(hPipeTable[i]);

    // destroy the mailslot
    DosDeleteMailslot(hMailslot);


    ss.svcs_status = SERVICE_NOT_PAUSABLE | SERVICE_UNINSTALLABLE
                        | SERVICE_UNINSTALLED;

    ss.svcs_code = usErrCode;

    err = NetServiceStatus( (char far *) &ss, sizeof(ss));
    if (err) {
        exit(err);
    }

    exit(0);
}



void
InitEchoThread( void )
{

    _beginthread(EchoThread, NULL, STACKSIZE, NULL);

}


void far
EchoThread( void *arg )
{

    ECHOPACKET	epIn;
    ECHOPACKET	epOut;
    struct wksta_info_0 far * wki;
    static char szNameBuf[2 + CNLEN + sizeof(MAILSLOT_REPLY)];
    USHORT	res;
    unsigned short avail;
    USHORT	usRead;
    USHORT	usNext;
    USHORT	usNextPriority;

    res = NetWkstaGetInfo(NULL, 0, NULL, 0, &avail);

    if ((res) && (res != NERR_BufTooSmall))
        ErrEnd();

    wki = NewFarSeg(avail);

    res = NetWkstaGetInfo(NULL, 0, (char far *) wki, avail, &avail);
    if (res)
        ErrEnd();

    strcpy(epOut.szServerName, "\\\\");
    strcpy(epOut.szServerName+2, wki->wki0_computername);
    epOut.usType = TYPE_ECHOPACKET;


    res = DosMakeMailslot(MAILSLOT_REQUEST, MAX_BUFSIZE, MAX_MAILSLOT,
				&hMailslot);

    if (res)
        ErrEnd();

    while (TRUE) {

	res = DosReadMailslot(hMailslot, (char far *) &epIn,
				&usRead, &usNext, &usNextPriority, -1L);
	if (res)
	    break;

	if (epIn.usType == TYPE_ECHOPACKET) {
	    if (!strcmp(epIn.szServerName, epOut.szServerName)) // local
		strcpy(szNameBuf, MAILSLOT_REPLY);

	    else {				     // remote
		strcpy(szNameBuf, epIn.szServerName);
		strcat(szNameBuf, MAILSLOT_REPLY);
	    }

	    // now do it again
	    // wait a random amount of time
	    DosSleep((ULONG) (rand() % 1000) );

	    res = DosWriteMailslot(szNameBuf, (char far *) &epOut,
				sizeof(epOut),
				0, 2, 0L);
	}
    }

    DosDeleteMailslot(hMailslot);

    EndThread();

}



void far
ServerThread( void *arg )
{
    USHORT  res;
    HPIPE   hpPipeHandle;
    CALCBUF cb;
    USHORT  cbActual;
    USHORT  usBufSize;
    PULONG  pusOutbuf;
    HSEM    hsemPipe;
    PIDINFO pi;
    char    sembuf[sizeof("\\SEM\\MANDEL0")];


    DosGetPID(&pi);
    strcpy(sembuf,"\\SEM\\MANDEL0");
    sembuf[strlen(sembuf)-1] += (char)pi.tid;


    res = DosCreateSem(CSEM_PUBLIC, &hsemPipe, sembuf);
    if (res)
        ErrEnd();

    res = DosMakeNmPipe(PIPE_SERVER, &hpPipeHandle, 0x0002, 0x050F,
			0x1000, 0x0400, 0L);

    if (res)
        ErrEnd();

    hPipeTable[pi.tid-2] = hpPipeHandle;

    DosSemSet(hsemPipe);
    res = DosSetNmPipeSem(hpPipeHandle, hsemPipe, 42);
    if (res)
        ErrEnd();

    while (TRUE) {

	res = DosConnectNmPipe(hpPipeHandle);
        if (res)
        {
            if (fQuit) {
                EndThread();
            }

            usErrCode = SERVICE_UIC_INTERNAL;

            ErrEnd();
        }


	while (TRUE) {
	    USHORT	read;
	    AVAILDATA	avail;
	    USHORT	status;

            if (fQuit) {
                break;
            }

	    // set this, in case we need to wait
	    DosSemSet(hsemPipe);

	    // look and see if there is data waiting
	    res = DosPeekNmPipe(hpPipeHandle, (char far *)NULL, 0,
				 &read, &avail, &status);
	    if ((res) || (status != NP_CONNECTED)) {
		DosDisConnectNmPipe(hpPipeHandle);
		break;
	    }
	    // if read isn't complete, let's wait for it
	    if (avail.cbpipe < sizeof(cb))
	    {
		// wait for a read to come in; if timeout, we're hung
		res = DosSemWait(hsemPipe, SEM_TIMEOUT);
		if (res) {
		    DosDisConnectNmPipe(hpPipeHandle);
		    break;
		}

                // sanity check --
                res = DosPeekNmPipe(hpPipeHandle, (char far *)NULL, 0,
				 &read, &avail, &status);
	        if ((res) || (status != NP_CONNECTED)) {
                    DosDisConnectNmPipe(hpPipeHandle);
		    break;
                }
	    }

	    res = DosRead((HFILE)hpPipeHandle, (PVOID) &cb, sizeof(cb),
			    &cbActual);

	    if ((res) || (cbActual < sizeof(cb))) {
		DosDisConnectNmPipe(hpPipeHandle);
                break;
	    }

	    /*
	     * ok, we got some data. Allocate a buffer, do the calculations,
	     * and send back the result.
	     */

	    pusOutbuf = MandelCalc(&(cb.cptLL), &(cb.rclDraw), cb.dblPrecision,
				    cb.ulThreshold, &usBufSize);

	    res = DosWrite((HFILE)hpPipeHandle, (PVOID) pusOutbuf,
		     usBufSize, &cbActual);

	    if (res) {
		DosDisConnectNmPipe(hpPipeHandle);
                break;
	    }

	    FreeFarSeg(pusOutbuf);
	}
    }

    EndThread();

}



extern VOID FAR PASCAL
sig_handler(unsigned sig_arg,
            unsigned sig_no)
{
    struct service_status ss;

    switch (sig_arg & 0xff)
    {
    case  SERVICE_CTRL_UNINSTALL:
        DosSemClear(hsemQuit);
        break;

    case SERVICE_CTRL_INTERROGATE:
        ss.svcs_status = SERVICE_NOT_PAUSABLE | SERVICE_UNINSTALLABLE
                            | SERVICE_INSTALLED;
        ss.svcs_code = 0;
        ss.svcs_pid = 0;

        NetServiceStatus((char far *)&ss, sizeof(ss));
        break;
    }


    DosSetSigHandler(0, 0, 0, SIGA_ACKNOWLEDGE, sig_no);

}



static void
EndThread( void )
{
    _endthread();
}


void
ErrEnd( void )
{

    usErrCode = SERVICE_UIC_INTERNAL;

    DosSemClear(hsemQuit);
    while (!fQuit)
        DosSleep(100L);
    EndThread();
}
