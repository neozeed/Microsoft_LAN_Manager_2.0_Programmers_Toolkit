/*
  PBX.C -- Phone switching program.

  Copyright (C) 1990 Microsoft Corporation 

  PBX.EXE is the Server portion of the PHONE application. PBX.EXE
  creates the named-pipe used by PHONE.EXE for communication and
  routes traffic between clients.

  To perform these duties, PBX.EXE maintains a simple routing
  table with one entry for each available phone line (client pipe
  connection).

  PBX.EXE may be configured via keywords in LANMAN.INI. PBX.EXE looks
  for keywords in the [pbx] section. The two keywords are LINES and
  LINEBUF. These determine the total number of phone lines created by
  PBX.EXE. LINEBUF specifies the maximum number of messages (PHMSG
  structures) that may be passed to PBX.EXE in a single write. For
  example, to create 100 phone lines and allow buffering of up to 30
  messages, LANMAN.INI would have:

      [pbx]
            lines   = 100
            linebuf = 30

  LINES accepts values between 5 and 255. Invalid values cause the default
  value of 16 to be taken. LINEBUF accepts values between 1 and 50.
  Invalid values cause the default value of 10 to be taken.

  For every LINESPERTHREAD phone lines, PBX.EXE creates a Router
  thread to manage the lines. (See ROUTER.C for additional details).

  This code example is provided for demonstration purposes only.
  Microsoft makes no warranty, either express or implied, 
  as to its usability in any given situation.
*/

// Includes
#define   INCL_DOS
#define   INCL_DOSERRORS
#include  <os2.h>

#define   INCL_NETCONFIG
#define   INCL_NETSERVICE
#define   INCL_NETERRORS
#include  <lan.h>

#include  <errno.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <process.h>
#include  <stddef.h>

#define   INCL_GLOBAL
#include  "mail.h"
#include  "pbx.h"

// Function Prototypes (for functions not in PBX.H)
void pascal FAR SignalHandler(USHORT usSigArg, USHORT usSigNum);
void            SetSignals(void);
void            Initialize(void);


/* ExitHandler -------------------------------------------------------------
  Description:  Perform clean-up processing prior to exiting PBX.EXE.
                NOTE:  ErrorExit (i.e., ERREXIT) must not be called
                       from this routine (since ErrorExit calls this
                       routine).
  Input      :  ulSrvCode --- LAN Manager Service exit code
  Ouput      :  None
--------------------------------------------------------------------------*/

void ExitHandler(ULONG ulSrvCode)
{
  ROUTEENT FAR *pbREnt;                   // Pointer to Router Table entry
  RTHREAD  FAR *pbRThread;                // Pointer to Router info
  USHORT       usTemp;                    // Temporary variable
  USHORT       usRetCode;                 // Return code

  // Inform LAN Manager that this service is being uninstalled
  ssStatus.svcs_status = SERVICE_UNINSTALL_PENDING;
  ssStatus.svcs_code   = ulSrvCode;

  // Shutdown threads and phone lines
  if (selRMemory != 0) {                  // PBX.EXE obtained memory
    // Indicate to all Router threads that a shutdown is necessary.
    // Each Router thread should complete only the operation it is
    // currently on and then exit.

    for (usTemp=0; usTemp < pbRMemory->usRThreadCnt; usTemp++) {
      pbRThread = &(pbRMemory->bRouters[usTemp]);
      usRetCode = DosSemSet(&(pbRThread->hsemExitSem));
      if (usRetCode)
        ERRRPT("DosSemSet", usRetCode)
    }

    // Wait for each Router thread to shutdown.
    // When a Router thread exits, it clears its own exit semaphore
    // as an acknowledgement to the main thread.
    // In case of errors, do not wait more then 1000 msec for a thread
    // to complete shutdown (ignore errors since the Router thread may
    // be blocked on its DosMuxSemWait).

    for (usTemp=0; usTemp < pbRMemory->usRThreadCnt; usTemp++) {
      pbRThread = &(pbRMemory->bRouters[usTemp]);
      DosSemWait(&(pbRThread->hsemExitSem), 1000L);
    }

    // If the routing table was obtained, shutdown all phone lines.
    // Ignore any errors that may occur since the lines may be in
    // unexpected states.

    if (pbRMemory->selRouteTable != 0) {
      for (usTemp=0; usTemp < pbRMemory->usLineCnt; usTemp++) {
        pbREnt = &(pbRMemory->pbRouteTable[usTemp]);
        DosDisConnectNmPipe(pbREnt->hPhoneLine);
        DosClose(pbREnt->hPhoneLine);
      }
    }
  }

  // Inform LAN Manager that this service has been uninstalled.

  ssStatus.svcs_status = SERVICE_UNINSTALLED;
  ssStatus.svcs_code   = ulSrvCode;
  usRetCode = NetServiceStatus((char FAR *)&ssStatus, sizeof(ssStatus));
  if (usRetCode)
    ERRRPT("NetServiceStatus", usRetCode)

  return;
}


/* SignalHandler -----------------------------------------------------------
  Description:  Process system signals (all of which should come from LAN
                Manager).
  Input      :  usSigArg --- Signal argument
                usSigNum --- Signal number
                             (should only be SERVICE_RCV_SIG_FLAG)
  Output     :  None
--------------------------------------------------------------------------*/

void pascal FAR SignalHandler(USHORT usSigArg, USHORT usSigNum)
{
  UCHAR        fSigArg;                   // Signal argument
  RTHREAD FAR  *pbRThread;                // Pointer to Router info
  USHORT       usTemp;                    // Temporary variable
  USHORT       usRetCode;                 // Return code

  // Extract signal argument
  fSigArg = (UCHAR) (usSigArg & 0xFF);

  // And take the appropriate action
  switch (fSigArg) {
    case SERVICE_CTRL_UNINSTALL:
      ExitHandler(NORMALSTATUS);
      DosExit(EXIT_PROCESS, 0);
      break;

    case SERVICE_CTRL_PAUSE:
      for (usTemp=0; usTemp < pbRMemory->usRThreadCnt; usTemp++) {
        pbRThread = &(pbRMemory->bRouters[usTemp]);
        usRetCode = DosSemSet(&(pbRThread->hsemPauseSem));
        if (usRetCode)
          ERREXIT("DosSemSet", usRetCode)
      }
      ssStatus.svcs_status = SERVICE_INSTALLED     |
                             SERVICE_PAUSED        |
                             SERVICE_UNINSTALLABLE |
                             SERVICE_PAUSABLE;
      ssStatus.svcs_code   = NORMALSTATUS;
      usRetCode = NetServiceStatus((char FAR *)&ssStatus, sizeof(ssStatus));
      if (usRetCode)
        ERREXIT("NetServiceStatus", usRetCode)
      break;

    case SERVICE_CTRL_CONTINUE:
      for (usTemp=0; usTemp < pbRMemory->usRThreadCnt; usTemp++) {
        pbRThread = &(pbRMemory->bRouters[usTemp]);
        usRetCode = DosSemClear(&(pbRThread->hsemPauseSem));
        if (usRetCode)
          ERREXIT("DosSemSet", usRetCode)
      }
      ssStatus.svcs_status = SERVICE_INSTALLED     |
                             SERVICE_ACTIVE        |
                             SERVICE_UNINSTALLABLE |
                             SERVICE_PAUSABLE;
      ssStatus.svcs_code   = NORMALSTATUS;
      usRetCode = NetServiceStatus((char FAR *)&ssStatus, sizeof(ssStatus));
      if (usRetCode)
        ERREXIT("NetServiceStatus", usRetCode)
      break;

    case SERVICE_CTRL_INTERROGATE:
    default:
      usRetCode = NetServiceStatus((char FAR *)&ssStatus, sizeof(ssStatus));
      break;
  }

  // Reset the signal handler to accept the next signal
  usRetCode = DosSetSigHandler(0,
                               0,
                               0,
                               SIGA_ACKNOWLEDGE,
                               usSigNum);
  if (usRetCode)
    ERREXIT("DosSetSigHandler", usRetCode)

  return;
}


/* SetSignals --------------------------------------------------------------
  Description:  Set signal handler for PBX.EXE
  Input      :  None
  Output     :  None
--------------------------------------------------------------------------*/

void SetSignals(void)
{
  // Value associated with the previous signal handler are not needed after
  // this routine and therefore may be held in local variables.
  PFNSIGHANDLER fnSigHandler;             // Previous signal handler
  USHORT        fSPrevAction;             // Previous signal handler action
  USHORT        usRetCode;                // Return Code

  // The handler will be called only for SERVICE_RCV_SIG_FLAG signals
  // (which are the same as SIG_PFLG_A).  This implies that LAN Manager
  // will cover all CTRL+C, BREAK, and KILL signals.

  usRetCode = DosSetSigHandler(SignalHandler,
                               &fnSigHandler,
                               &fSPrevAction,
                               SIGA_ACCEPT,
                               SERVICE_RCV_SIG_FLAG);
  if (usRetCode)
    ERREXIT("DosSetSigHandler", usRetCode)

  // Tell OS/2 that CTRL+C, BREAK, and KILL signals will not be processed
  // by PBX.EXEs signal handler.

  usRetCode = DosSetSigHandler(NULL,      // Ignore CTRL+C
                               &fnSigHandler,
                               &fSPrevAction,
                               SIGA_IGNORE,
                               SIG_CTRLC);
  if (usRetCode)
    ERREXIT("DosSetSigHandler", usRetCode)
  usRetCode = DosSetSigHandler(NULL,      // Ignore BREAK
                               &fnSigHandler,
                               &fSPrevAction,
                               SIGA_IGNORE,
                               SIG_CTRLBREAK);
  if (usRetCode)
    ERREXIT("DosSetSigHandler", usRetCode)
  usRetCode = DosSetSigHandler(NULL,      // Ignore KILL
                               &fnSigHandler,
                               &fSPrevAction,
                               SIGA_IGNORE,
                               SIG_KILLPROCESS);
  if (usRetCode)
    ERREXIT("DosSetSigHandler", usRetCode)

  // A SIG_PFLG_B or SIG_PFLG_C signal should cause an error,
  // since they shouldn't occur.

  usRetCode = DosSetSigHandler(NULL,      // SIG_PFLG_B is an error
                               &fnSigHandler,
                               &fSPrevAction,
                               SIGA_IGNORE,
                               SIG_PFLG_B);
  if (usRetCode)
    ERREXIT("DosSetSigHandler", usRetCode)
  usRetCode = DosSetSigHandler(NULL,      // SIG_PFLG_C is an error
                               &fnSigHandler,
                               &fSPrevAction,
                               SIGA_IGNORE,
                               SIG_PFLG_C);
  if (usRetCode)
    ERREXIT("DosSetSigHandler", usRetCode)

  return;
}


/* Initialize --------------------------------------------------------------
  Description:  Prepare PBX.EXE for execution.  This includes:
                  a) Close standard file handles
                  b) Allocate needed storage
                  c) Reading values from LANMAN.INI
                  d) Create phone lines and Router threads
  Input      :  None
  Output     :  None
--------------------------------------------------------------------------*/

void Initialize(void)
{
  char    pbBuffer[80];                   // Parameter buffer
  USHORT  usParmLen;                      // Parameter length
  int     sTemp;                          // Temporary variable
  USHORT  usRetCode;                      // Return code

  // Close STDIN, STDOUT, and STDERR since they are not used nor required
  // Redirect STDIN, STDOUT, and STDERR to the NUL device.
  {
    HFILE   hFileNul;                     // Handle to NUL device
    HFILE   hNewHandle;                   // Temporary file handle
    USHORT  usAction;                     // Action taken by DosOpen

    // First, open the NUL device
    usRetCode = DosOpen("NUL",                   // NUL device name
                        &hFileNul,               // File handle
                        &usAction,               // Action taken
                        0L,                      // File size
                        FILE_NORMAL,             // File attribute
                        FILE_CREATE,             // Open action
                        OPEN_ACCESS_READWRITE |
                        OPEN_SHARE_DENYNONE,     // Open mode
                        0L);
    if (usRetCode)
      ERREXIT("DosOpen", usRetCode)

    // Then, redirect STDIN, STDOUT, and STDERR
    for (hNewHandle=0; hNewHandle < 3; hNewHandle++) {
      // If the handle is already directed to NUL, skip it
      if (hFileNul != hNewHandle) DosDupHandle(hFileNul, &hNewHandle);
    }

    // Lastly, close the extra handle to NUL
    if (hFileNul > 2)
      DosClose(hFileNul);
  }

  // Allocate shared memory segment
  selRMemory= 0;                          // Segment not yet obtained
  usRetCode = DosAllocSeg(sizeof(RMEMORY),  // Segment size
                          &selRMemory,      // Selector
                          SEG_NONSHARED);   // Private
  if (usRetCode)
    ERREXIT("DosAllocSeg", usRetCode)

  // Make shared memory segment pointer
  pbRMemory = MAKEP(selRMemory, 0);

  // Indicate that the router table segment has not yet been obtained
  pbRMemory->selRouteTable = 0;

  // Create semaphore to control access to the shared memory segment
  usRetCode = DosCreateSem(CSEM_PRIVATE,            // Private
                           &pbRMemory->hssmMemLock, // Semaphore handle
                           RINGSEM);                // Semaphore name
  if (usRetCode)
    ERREXIT("DosCreateSem", usRetCode)

  // Lock all access to the shared memory segment to prevent Router
  // threads from interfering with the initilization process.
  usRetCode = DosSemSet(pbRMemory->hssmMemLock);

  if (usRetCode)
    ERREXIT("DosSemSet", usRetCode)

  // Now, read configuration values from LANMAN.INI
  // First, how many lines should PBX.EXE support?
  pbRMemory->usLineCnt = DEFAULTLINES;    // Assume default
  usRetCode = NetConfigGet2(NULL,         // Local Computer
                            NULL,         // Reserved
                            PBXCOMPONENT, // LANMAN.INI Component name
                            LINESNUMBER,  // LANMAN.INI Keyword
                            pbBuffer,     // Target buffer
                            sizeof(pbBuffer), // Buffer size
                            &usParmLen);  // Actual Parameter Length

  // If a value was supplied, verify that it is valid
  if (!usRetCode               &&         // Call was successful
      usParmLen                &&         // Value was supplied
      (sTemp = atoi(pbBuffer)) &&         // Value can be converted
      sTemp <= MAXLINES        &&         // And the value is
      sTemp >= MINLINES         )         //   in the proper range
    pbRMemory->usLineCnt = sTemp;

  // What size of a line buffer should be used?
  pbRMemory->usLineBufCnt = DEFAULTLINEBUF; // Assume default
  usRetCode = NetConfigGet2(NULL,         // Local Computer
                            NULL,         // Reserved
                            PBXCOMPONENT, // LANMAN.INI Component name
                            LINEBUFNUMBER,// LANMAN.INI Keyword
                            pbBuffer,     // Target buffer
                            sizeof(pbBuffer), // Buffer size
                            &usParmLen);  // Actual Parameter Length

  // If a value was supplied, verify that it is valid
  if (!usRetCode               &&         // Call was successful
      usParmLen                &&         // Value was supplied
      (sTemp = atoi(pbBuffer)) &&         // Value can be converted
      sTemp <= MAXLINEBUF      &&         // And the value is
      sTemp >= MINLINEBUF       )         //   in the proper range
    pbRMemory->usLineBufCnt = sTemp;

  // Set the size of the phone line buffer
  // Make it a multiple of the PHMSG block (allow a little for named-pipe
  // message mode overhead)
  pbRMemory->usLineBufSize = (pbRMemory->usLineBufCnt) * (sizeof(PHMSG)+8);

  // Initialized count of phone lines in-use by clients
  pbRMemory->usLinesUsed = 0;

  // Allocate Routing Table
  usRetCode = DosAllocSeg(pbRMemory->usLineCnt * sizeof(ROUTEENT), // Size
                          &(pbRMemory->selRouteTable),  // Selector
                          SEG_NONSHARED);               // Private
  if (usRetCode)
    ERREXIT("DosAllocSeg", usRetCode)

  // Make router table pointer
  pbRMemory->pbRouteTable = MAKEP(pbRMemory->selRouteTable, 0);

  // Set number of Router threads required to handle requested lines
  pbRMemory->usRThreadCnt = pbRMemory->usLineCnt / LINESPERTHREAD;
  if (pbRMemory->usLineCnt % LINESPERTHREAD) pbRMemory->usRThreadCnt++;

  // Ensure that enough pipe handles exist for the number of requested
  // phone lines.

  usRetCode = DosSetMaxFH((pbRMemory->usLineCnt) + MINFHANDLES);
  if (usRetCode)
    ERREXIT("DosSetMaxFH", usRetCode)

  // Initialize Routing Table
  { USHORT       usThreadNum;             // Current Router thread
    USHORT       usLineNum;               // Current phone line
    ROUTEENT FAR *pbREnt;                 // Pointer to Route Table entry
    CHAR         pszPipeSem[80];          // Named-pipe semaphore name
    USHORT       usSemIndex;              // Index into MUXSEMLIST struct
    RTHREAD FAR  *pbRThread;              // Pointer to Router info

    // Initialize pointer to Router info struct and initial thread
    usThreadNum = 0;
    pbRThread   = &(pbRMemory->bRouters[usThreadNum]);
    pbRThread->mxslSems.cmxs = 0;

    // Initialize all Router table entries
    for (usLineNum = 0;
         usLineNum < pbRMemory->usLineCnt;
         usLineNum++) {

      // When LINESPERTHREAD has been reached, advance to the next
      // Router thread structure
      if (usLineNum != 0 && (usLineNum % LINESPERTHREAD) == 0) {
        usThreadNum++;
        pbRThread = &(pbRMemory->bRouters[usThreadNum]);
        pbRThread->mxslSems.cmxs = 0;
      }

      // Point to current table entry
      pbREnt = &(pbRMemory->pbRouteTable[usLineNum]);

      // Initialize information fields
      *(pbREnt->pszPhoneNumber) = '\0';
      pbREnt->fState            = Invalid;
      pbREnt->usConnection      = 0;

      // Clear phone line access semaphore
      DosSemClear(&(pbREnt->ulLineSem));

      // Create instance of Named-pipe to function as phone line
      // Large input and output buffers are required to support
      // fast typists
      usRetCode = DosMakeNmPipe(
                     PHONELINE,              // Pipe name
                     &(pbREnt->hPhoneLine),  // Handle address
                     NP_ACCESS_DUPLEX |      // Full duplex
                     NP_NOINHERIT     |      // Private
                     NP_WRITEBEHIND,         // Speedy writes
                     NP_NOWAIT           |   // Non-blocking
                     NP_READMODE_MESSAGE |   // Message mode
                     NP_TYPE_MESSAGE     |
                     pbRMemory->usLineCnt,   // Number of lines
                     100 * sizeof(PHMSG),    // Output buffer size
                     100 * sizeof(PHMSG),    // Input buffer size
                     DEFAULTTIMEOUT);        // Timeout value
      if (usRetCode)
        ERREXIT("DosMakeNmPipe", usRetCode)

      // Create a semaphore for the Named-pipe instance using the
      // semaphore generic name plus the number of the current line
      usSemIndex= usLineNum % LINESPERTHREAD;        // MUXSEMLIST index
      pbRThread->mxslSems.amxs[usSemIndex].zero = 0; // Reserved field
      sprintf(pszPipeSem, "%s%u", LINESEM, usLineNum);
      usRetCode = DosCreateSem(CSEM_PUBLIC,
                               &(pbRThread->mxslSems.amxs[usSemIndex].hsem),
                               pszPipeSem);
      if (usRetCode)
        ERREXIT("DosCreateSem", usRetCode)

      // Associate the semaphore with the Named-pipe
      usRetCode = DosSetNmPipeSem(pbREnt->hPhoneLine, // Pipe handle
                                  pbRThread->mxslSems.amxs[usSemIndex].hsem,
                                  0);
      if (usRetCode)
        ERREXIT("DosSetNmPipeSem", usRetCode)

      // Set semaphore to capture the first pipe event
      usRetCode = DosSemSet(pbRThread->mxslSems.amxs[usSemIndex].hsem);
      if (usRetCode)
        ERREXIT("DosSemSet", usRetCode)

      // Increment the MUXSEMLIST semaphore count
      pbRThread->mxslSems.cmxs++;

      // Enable the pipe instance for client connections
      usRetCode = DosConnectNmPipe(pbREnt->hPhoneLine);
      if (usRetCode && usRetCode != ERROR_PIPE_NOT_CONNECTED)
        ERREXIT("DosConnectNmPipe", usRetCode)
    }
  }

  // Create one thread for every LINESPERTHREAD phone lines.
  { USHORT      usThreadNum;              // Current Router thread
    RTHREAD FAR *pbRThread;               // Pointer to Router struct

    for (usThreadNum=0;
         usThreadNum < pbRMemory->usRThreadCnt;
         usThreadNum++) {

      // Get a local pointer to the shared memory structure
      pbRThread = &(pbRMemory->bRouters[usThreadNum]);

      // Allocate buffer for phone line
      usRetCode = DosAllocSeg(pbRMemory->usLineBufSize, // Size
                              &(pbRThread->selLineBuf), // Selector
                              SEG_NONSHARED);           // Private
      if (usRetCode)
        ERREXIT("DosAllocSeg", usRetCode)

      // Build a pointer to the phone line buffer
      pbRThread->pbLineBuf = MAKEP(pbRThread->selLineBuf, 0);

      // Clear exit flag semaphore
      usRetCode = DosSemClear(&(pbRThread->hsemExitSem));
      if (usRetCode)
        ERREXIT("DosSemClear", usRetCode)

      // Set semaphore to prevent thread from processing
      usRetCode = DosSemSet(&(pbRThread->hsemPauseSem));
      if (usRetCode)
        ERREXIT("DosSemSet", usRetCode)

      // Create Router thread and pass the current thread number
      pbRThread->sRouterID = _beginthread((void (FAR *)(void FAR *))Router,
                                        NULL,
                                        DEFAULTSTACKSIZE,
                                        (void *)usThreadNum);
      if (pbRThread->sRouterID < 0)
        ERREXIT("_beginthread", usThreadNum)
    }
  }

  return;
}


/* main --------------------------------------------------------------------
  Description:  See file header
  Input      :  None (directly)
  Output     :  None
--------------------------------------------------------------------------*/

void main(void)
{
  PIDINFO pidInfo;                        // OS/2 PID info struct
  USHORT  usRetCode;                      // Return Code

  // Obtain the process ID of this process
  usRetCode = DosGetPID(&pidInfo);
  if (usRetCode)
    ERREXIT("DosGetPID", usRetCode)

  // Inform LAN Manager that this service is in the process of installation
  // Because the signal handler is not yet installed, the service cannot
  // be uninstalled or paused.
  ssStatus.svcs_pid    = pidInfo.pid;
  ssStatus.svcs_status = SERVICE_INSTALL_PENDING   |
                         SERVICE_NOT_UNINSTALLABLE |
                         SERVICE_NOT_PAUSABLE;
  ssStatus.svcs_code   = NORMALSTATUS;
  usRetCode = NetServiceStatus((char FAR *)&ssStatus, sizeof(ssStatus));
  if (usRetCode != NERR_Success)
    ERREXIT("NetServiceStatus", usRetCode)

  // Install the signal handler
  SetSignals();

  // Indicate that the signal handler is now installed and inform LAN
  // Manager that the service may be uninstalled or paused
  ssStatus.svcs_status = SERVICE_INSTALL_PENDING |
                         SERVICE_UNINSTALLABLE   |
                         SERVICE_PAUSABLE;
  ssStatus.svcs_code   = NORMALSTATUS;
  usRetCode = NetServiceStatus((char FAR *)&ssStatus, sizeof(ssStatus));
  if (usRetCode)
    ERREXIT("NetServiceStatus", usRetCode)

  // Proceed with initialization
  Initialize();

  // Clear each Router thread's semaphore to allow processing to start.
  { USHORT      usThreadNum;              // Current Router thread
    RTHREAD FAR *pbRThread;               // Pointer to Router struct

    for (usThreadNum=0;
         usThreadNum < pbRMemory->usRThreadCnt;
         usThreadNum++) {
      pbRThread = &(pbRMemory->bRouters[usThreadNum]);
      usRetCode = DosSemClear(&(pbRThread->hsemPauseSem));
      if (usRetCode)
        ERREXIT("DosSemClear", usRetCode)
    }
  }

  // Clear the shared memory segment access semaphore
  usRetCode = DosSemClear(pbRMemory->hssmMemLock);
  if (usRetCode)
    ERREXIT("DosSemClear", usRetCode)

  // Inform LAN Manager that this service is now installed
  ssStatus.svcs_status = SERVICE_INSTALLED     |
                         SERVICE_UNINSTALLABLE |
                         SERVICE_PAUSABLE;
  ssStatus.svcs_code   = NORMALSTATUS;
  usRetCode = NetServiceStatus((char FAR *)&ssStatus, sizeof(ssStatus));
  if (usRetCode)
    ERREXIT("NetServiceStatus", usRetCode)

  // Wait (forever) on dummy semaphore
  usRetCode = DosSemSet(&(pbRMemory->hsemPBX));
  if (usRetCode)
    ERREXIT("DosSemSet", usRetCode)

  do {
    usRetCode = DosSemWait(&(pbRMemory->hsemPBX), SEM_INDEFINITE_WAIT);
    if (usRetCode != ERROR_INTERRUPT)
      ERREXIT("DosSemWait", usRetCode)
  } while (usRetCode == ERROR_INTERRUPT);

  return;
}
