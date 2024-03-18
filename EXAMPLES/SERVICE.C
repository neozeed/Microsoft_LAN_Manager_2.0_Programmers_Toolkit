/*
   SERVICE.C - A sample service program that can be run with LAN Manager.


   Compile Instructions:
      This is a multithread application and should be compiled
      accordingly. Here are sample command-line sequences for
      Microsoft C compilers:

   Microsoft C 5.1:
      CL /c /AL SAMPLES.C
      LIB SAMPLES -+SAMPLES;
      CL /c /AL /I \PMTK\INCLUDE /I \C510\INCLUDE\MT SERVICE.C
      LINK /NOD /NOI SERVICE,,,LLIBCMT+DOSCALLS+LAN+SAMPLES;

      The directories after the /I switches on the compile line
      (\PMTK\INCLUDE and \C510\INCLUDE\MT) represent the MS OS/2 
      Presentation Manager include file directory and the Microsoft 
      C 5.1 multithread include directory, respectively. Substitute 
      the correct pathnames for your Presentation Manager and multithread 
      include directories.

   Microsoft C 6.0:
      CL /c /AL SAMPLES.C
      LIB SAMPLES -+SAMPLES;
      CL /c /AL /MT SERVICE.C
      LINK /NOD /NOI SERVICE,,,LLIBCMT+OS2+LAN+SAMPLES;

   Note that with both Microsoft C 5.1 and Microsoft C 6.0 you must
   build SAMPLES.LIB as a large model program first.

   Usage:
         a)   Put following line in LANMAN.INI in [services] section
              service = <path of SERVICE.EXE>\SERVICE.EXE
         b)   Optionally, put following section in LANMAN.INI
                 [services] 
                     decaf  = yes
         c)   Start the service
                 NET START SERVICE
              or
                 NET START SERVICE /DECAF:YES

   API                   Used to...
   =================     ================================================
   NetServiceStatus      Notify LAN Manager about the current
                         status of service
   DosMakeMailslot       Make a mailslot to receive alert messages
   DosReadMailslot       Read alert messages coming in a mailslot
   DosDeleteMailslot     Delete a mailslot
   NetAlertStart         Register a mailslot to receive an alert message
   NetAlertStop          Cancel receiving alert messages
   NetErrorLogWrite      Write error message to LAN Manager error log


   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied,
   as to its usability in any given situation.
*/

#define     INCL_KBD
#define     INCL_VIO
#define     INCL_DOSSIGNALS
#define     INCL_DOSPROCESS
#define     INCL_DOSSEMAPHORES
#define     INCL_DOSFILEMGR
#define     INCL_DOSERRORS
#include    <os2.h>        // MS OS/2 header files

#define     INCL_NETMAILSLOT
#define     INCL_NETSERVICE
#define     INCL_NETALERT
#define     INCL_NETERRORS
#define     INCL_NETERRORLOG
#include    <lan.h>        // LAN Manager header files
                     
#include    <stdio.h>
#include    <process.h>
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"    // Internal routine header file

#define DEFAULT_STACK_SIZE      (4096+512) // 4K for API calls and 
                                           // space for the service
#define DEFAULTCOMPONENTNAME    "SERVICE"
#define COFFEE_ALERT            "COFFEE_ALERT"
#define ALERT_TYPE_MESSAGE      "Coffee is ready! Come and Get it!"
#define ALERT_MAILBOX           "\\MAILSLOT\\ALERT"
#define MESSAGE_SIZE            (sizeof(struct std_alert) + \
                                sizeof(struct user_other_info))
#define MAILSLOT_SIZE           (10 * MESSAGE_SIZE)
#define WAIT_FOR_ALERT_TIME     ALERT_LONG_WAIT
#define DEFAULT_PARAMETER       "DECAF"

#define INVALIDPARAM        "Invalid Parameter %s"
#define MSGERRCNTRLCSIGNAL  "Error in Setting CTL-C Handler %d"
#define MSGERRCNTRLBSIGNAL  "Error in Setting CTL-BREAK Handler %d"
#define MSGERRKILLPSIGNAL   "Error in Setting KILLPROCESS Handler %d"
#define MSGERRFLAGASIGNAL   "Error in Setting Flag A Handler %d"
#define MSGERRFLAGBSIGNAL   "Error in Setting Flag B Handler %d"
#define MSGERRFLAGCSIGNAL   "Error in Setting Flag C Handler %d"
#define MSGERRSRVSSIGNAL    "Error in Setting Service Handler %d"
#define MSGERRDOSSEMSET     "Error in DosSemSet %d"
#define MSGERRDOSSEMWAIT    "Error in DosSemWait %d"
#define MSGERRBEGINTHREAD   "Error in Spawning Thread tid: %d"
#define MSGERRMAKEMAIL      "Error in DosMailSlot %d"
#define MSGERRREADMAIL      "Error in DosReadMailslot %d"
#define MSGERRALERTSTOP     "Error in NetAlertStop %d"
#define MSGERRALERTSTART    "Error in NetAlertStart %d"
#define MSGERRSEMWAIT       "Error in DosSemWait %d"
#define MSGERRGETPID        "Error in DosGetPID %d"
#define MSGERRDOSOPEN       "Error in DosOpen %d"

PFNSIGHANDLER pnPrevHandler;       // Previous handle address
unsigned short pfPrevAction;       // Previous action flag
int  fSigStarted;                  // Flag: TRUE if signal handler started
struct service_status ssStatus;    // Service status structure

PBYTE  pbStack;                    // Address of top of stack
TID tThreadTid;                    // TID of Alert handler thread

unsigned long hSemPause = 0;       // RAM semaphore for pausing AlertHandler
char  szErrBuffer[80];             // Buffer used to report errors
int   fAlertRegistered = FALSE;    // TRUE if alert registered
unsigned  hMailSlot;               // Mailslot handle

// Function declarations
void far AlertHandler(void *arg);
void ErrOut(unsigned short, char far *);
void ExitHandler(void);
void far pascal SignalHandler(unsigned short, unsigned short);


void main(int argc, char *argv[])
{
   int iCount;                    // Index for parsing argv
   unsigned uReturnCode;          // Return code
   char  szErrBuffer[80];         // Buffer used to report errors
   unsigned short usAction;       // Action taken by DosOpen
   unsigned long hSem = 0;        // Dummy semaphore; wait forever
   PIDINFO pidInfo;               // Get PID of main process
   char    szParmValue[80];       // Buffer for value of parameter
   char *  pszDecaf = DEFAULT_PARAMETER;  // DECAF parameter
   HFILE   hFileNul;              // Handle for NUL device
   HFILE   hNewHandle;            // Temporary handle
   fSigStarted  = FALSE;          // Signal handler not started
                                  // Set up service status for param error
   ssStatus.svcs_status = SERVICE_UNINSTALLED |
                          SERVICE_UNINSTALLABLE;
   ssStatus.svcs_code = SERVICE_UIC_M_ERRLOG;    // Look at error log

   /* 
    * Parse the service parameters passed by LAN Manager.
    * The parameters passed in argv can be from LANMAN.INI
    * if they are defined in LANMAN.INI. For example:
    *    [services]
    *         DECAF = yes
    * Or, they can be from the command line. For example:
    *    "net start service /DECAF:yes"
    * Both ways of setting parameters get passed through to the 
    * service as DECAF = yes.
    */

   for (iCount = 1; iCount < argc; iCount++)
   {
      if (strnicmp(argv[iCount], pszDecaf, strlen(pszDecaf)) == 0)
      {  // Copy the string after equal sign
         strcpy(szParmValue, argv[iCount]+strlen(pszDecaf)+1);
      }
      else
      {
         ssStatus.svcs_code |= SERVICE_UIC_AMBIGPARM;
         uReturnCode = NetServiceStatus((char far *) &ssStatus,
                                        sizeof(ssStatus));
         sprintf(szErrBuffer, INVALIDPARAM, argv[iCount]);
         ErrOut(0, szErrBuffer);
      }
   }

   /* 
    * Get the process ID of the main process to be
    * used in subsequest NetServiceStatus calls.
    */

   uReturnCode = DosGetPID( &pidInfo);

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRGETPID, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }
   ssStatus.svcs_pid = pidInfo.pid;

   /* 
    * Notify LAN Manager that the service is installing. At this time, 
    * the service cannot be paused or uninstalled because signal handler 
    * has not been installed.
    */

   ssStatus.svcs_status = SERVICE_INSTALL_PENDING |
                          SERVICE_NOT_UNINSTALLABLE |
                          SERVICE_NOT_PAUSABLE;
   ssStatus.svcs_code = SERVICE_UIC_NORMAL;

   uReturnCode = NetServiceStatus((char far *) &ssStatus, sizeof(ssStatus));

   /* 
    * Install the signal handler. Ignore CTRL-C, BREAK, and KILL 
    * signals and let LAN Manager take care of them. The signal 
    * handler will be raised when SERVICE_RCV_SIG_FLAG is raised
    * (SERVICE_RCV_SIG_FLAG is the same as SIG_PFLG_A).
    */

   uReturnCode = DosSetSigHandler(SignalHandler,
                                  &pnPrevHandler,
                                  &pfPrevAction,
                                  SIGA_ACCEPT,
                                  SERVICE_RCV_SIG_FLAG);

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRCNTRLCSIGNAL, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   /* 
    * Signal handler has been installed. Service can now be
    * paused or uninstalled.
    */

   fSigStarted = TRUE;
   ssStatus.svcs_status = SERVICE_INSTALL_PENDING |
                          SERVICE_UNINSTALLABLE |
                          SERVICE_PAUSABLE;
   ssStatus.svcs_code = SERVICE_UIC_NORMAL;

   uReturnCode = NetServiceStatus((char far *)&ssStatus, sizeof(ssStatus));

   uReturnCode = DosSetSigHandler(NULL,      // Ignore CTRL-C
                                  &pnPrevHandler,
                                  &pfPrevAction,
                                  SIGA_IGNORE,
                                  SIG_CTRLC);

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRCNTRLCSIGNAL, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   uReturnCode = DosSetSigHandler(NULL,      // Ignore BREAK signal
                                  &pnPrevHandler,
                                  &pfPrevAction,
                                  SIGA_IGNORE,
                                  SIG_CTRLBREAK);

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRCNTRLBSIGNAL, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   uReturnCode = DosSetSigHandler(NULL,      // Ignore KILL signal
                                  &pnPrevHandler,
                                  &pfPrevAction,
                                  SIGA_IGNORE,
                                  SIG_KILLPROCESS);

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRKILLPSIGNAL, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   uReturnCode = DosSetSigHandler(NULL, // Flag B should cause error
                                  &pnPrevHandler,
                                  &pfPrevAction,
                                  SIGA_ERROR,
                                  SIG_PFLG_B);

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRFLAGBSIGNAL, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   uReturnCode = DosSetSigHandler(NULL, // Flag C should cause error
                                  &pnPrevHandler,
                                  &pfPrevAction,
                                  SIGA_ERROR,
                                  SIG_PFLG_C);

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRFLAGCSIGNAL, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   /* 
    * Close stdin, stdout, and stderr handles so that
    * they cannot be used from this service. Redirect stdin, stdout,
    * stderr to NULL device.
    */

    // Open the null device
    uReturnCode = DosOpen("NUL",        // Name of device
                           &hFileNul,      // Handle
                           &usAction,   // Action taken
                           0L,          // File size
                           FILE_NORMAL, // File attribute
                           FILE_OPEN,   // Open action
                                        // Open mode
                           OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYNONE,
                           0L);         // Reserved
    if (uReturnCode != NERR_Success)
    {
      sprintf(szErrBuffer, MSGERRDOSOPEN, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
    }

    // Reroute stdin, stderr, and stderr to NUL
    for (hNewHandle=0; hNewHandle < 3; hNewHandle++)
    {
        // Skip the duplicating operation if the
        // handle already points to NUL
        if (hFileNul != hNewHandle)
            DosDupHandle(hFileNul, &hNewHandle);
    }

    // Extra handle to NUL not needed anymore
    if (hFileNul > 2)
        DosClose(hFileNul);

   // Spawn the application thread.

   pbStack = (PBYTE) SafeMalloc(DEFAULT_STACK_SIZE);
   tThreadTid = _beginthread(AlertHandler,
                             pbStack,
                             DEFAULT_STACK_SIZE,
                             NULL);

   if (tThreadTid == -1) {
      sprintf(szErrBuffer, MSGERRBEGINTHREAD, tThreadTid);
      ErrOut(0, szErrBuffer);
   }

   // Notify LAN Manager that the service is installed.

   ssStatus.svcs_status = SERVICE_INSTALLED |
                          SERVICE_UNINSTALLABLE |
                          SERVICE_PAUSABLE;
   ssStatus.svcs_code = SERVICE_UIC_NORMAL;

   uReturnCode = NetServiceStatus((char far *) &ssStatus, sizeof(ssStatus));

   /* 
    * Wait on dummy semaphore. This thread will wait forever on the
    * dummy semaphore.
    */

   uReturnCode = DosSemSet(&hSem);  // Set the semaphore

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRDOSSEMSET, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   /* 
    * The main thread waits on the RAM semaphore, but it is 
    * interrupted whenever a signal comes. Loop over DosSemWait 
    * if interrupted by a signal.
    */

   do {
      uReturnCode = DosSemWait(&hSem, SEM_INDEFINITE_WAIT);
      if (uReturnCode != ERROR_INTERRUPT)
      {
         sprintf(szErrBuffer,MSGERRDOSSEMWAIT, uReturnCode);
         ErrOut(uReturnCode, szErrBuffer);
      }
   } while (uReturnCode == ERROR_INTERRUPT);
}

//=======================================================================
//  ErrOut
// 
//  This routine is called whenever an error that needs to
//  be reported occurs. It writes a text message in the error log
//  and then calls the ExitHandler routine to stop the service.
//=======================================================================

void ErrOut(unsigned short usErrCode, char far * psErrStr)
{
   NetErrorLogWrite(NULL,                // Reserved; must be NULL
                    usErrCode,           // Error code
                    MYSERVICENAME,       // Component name
                    NULL,                // Pointer to raw data buffer
                    0,                   // Length of raw data buffer
                    psErrStr,            // String data
                    1,                   // Number of error strings
                    NULL);               // Reserved; Must be NULL

   /*
    * If signal handler IS installed, notify LAN Manager 
    * that the service is exiting.
    */
   if (fSigStarted)
       ExitHandler();

   _endthread();                         // End AlertHandler
 
   DosExit(EXIT_PROCESS, 0);
}

//=======================================================================
//  SignalHandler
// 
//  The signal handling routine gets control when the system sends a
//  status-changing request to the service. The routine should always 
//  expect these arguments:
//     usSigArg = Opcode that describes the request 
//     usSigNum = Number that identifies the process flag
//                the signal was raised on
//=======================================================================

void far pascal SignalHandler(unsigned short usSigArg,
                              unsigned short usSigNum)
{
   unsigned uReturnCode;
   unsigned char fOpCode;
                                           // Compute the function code
   fOpCode = (unsigned char) (usSigArg & 0xFF);
                   
   switch (fOpCode)                        // Do the function
   {
      case SERVICE_CTRL_UNINSTALL:         // Uninstall; end the service
         ExitHandler();
         DosExit(EXIT_PROCESS, 0);         // Stop this process
         break;
      case SERVICE_CTRL_PAUSE:             // Pause service
         DosSemSet(&hSemPause);            // Pause AlertHandler
         ssStatus.svcs_status = SERVICE_INSTALLED |
                                SERVICE_PAUSED |
                                SERVICE_UNINSTALLABLE|
                                SERVICE_PAUSABLE;
         ssStatus.svcs_code = SERVICE_UIC_NORMAL;
         uReturnCode = NetServiceStatus((char far *) &ssStatus,
                                        sizeof(ssStatus));
         break;
      case SERVICE_CTRL_CONTINUE:          // Continue service
         DosSemClear(&hSemPause);          // Continue AlertHandler
         ssStatus.svcs_status = SERVICE_INSTALLED |
                                SERVICE_ACTIVE |
                                SERVICE_UNINSTALLABLE|
                                SERVICE_PAUSABLE;
         ssStatus.svcs_code = SERVICE_UIC_NORMAL;
         uReturnCode = NetServiceStatus((char far *) &ssStatus,
                                        sizeof(ssStatus));
         break;
      case SERVICE_CTRL_INTERROGATE:       // Give service status
      default:
         uReturnCode = NetServiceStatus((char far *) &ssStatus,
                                        sizeof(ssStatus));
         break;
   }

   /*
    * Reenable signal handling. This signal handler accepts the next 
    * signal raised on usSigNum.
    */
   uReturnCode = DosSetSigHandler(0,
                          0,
                          0,
                          SIGA_ACKNOWLEDGE,
                          usSigNum);
   return;
}

//=======================================================================
//  ExitHandler 
//
//  This routine should perform any exit processing tasks and
//  then inform LAN Manager that the service is uninstalled.
//=======================================================================

void ExitHandler()
{
   API_RET_TYPE uReturnCode;

   //Notify LAN Manager that the service will uninstall.
   ssStatus.svcs_status = SERVICE_UNINSTALL_PENDING |
                          SERVICE_UNINSTALLABLE |
                          SERVICE_NOT_PAUSABLE;
   ssStatus.svcs_code = SERVICE_UIC_NORMAL;

   uReturnCode = NetServiceStatus((char far *) &ssStatus, sizeof(ssStatus));

   // Exit processing would go here.

   /*  
    * If the service is registered to receive alerts, deregister and
    * remove the alert semaphore or mailslot.
    */
   if (fAlertRegistered)
   {
      NetAlertStop(COFFEE_ALERT, ALERT_MAILBOX);
      DosDeleteMailslot(hMailSlot);
   }


   // Notify LAN Manager that the service is uninstalled.
   ssStatus.svcs_status = SERVICE_UNINSTALLED |
                          SERVICE_UNINSTALLABLE |
                          SERVICE_NOT_PAUSABLE;
   ssStatus.svcs_code = SERVICE_UIC_NORMAL;

   NetServiceStatus((char far *) &ssStatus, sizeof(ssStatus));
   return;
}

//=======================================================================
//  AlertHandler
// 
//  AlertHandler is a thread spawned by the main thread to service
//  processing of the COFFEE_ALERT type alert event. The thread is 
//  terminated from the ExitHandler routine when this service is stopped.
//=======================================================================

void far AlertHandler(void *arg)
{
   API_RET_TYPE uReturnCode;          // API return code
   KBDKEYINFO kbci;                   // Used to get keyboard input
   unsigned short usRow = 5;          // Screen row
   unsigned short usColumn = 5;       // Screen column
   char sVioString[80];               // Popup output string
                                      // Delay popup if one on screen
   unsigned short fPopUp = VP_WAIT | VP_OPAQUE;
   unsigned char bAttr = 0x17;        // Screen attributes
   unsigned short cbReturned;         // Message size
   unsigned short cbNextSize;         // Next message size
   unsigned short cbNextPriority;     // Next message priority
   char cbAlertBuffer[MESSAGE_SIZE];  // Message buffer

   // Notify LAN Manager that the service is active.
   ssStatus.svcs_status = SERVICE_INSTALLED |
                          SERVICE_ACTIVE |
                          SERVICE_UNINSTALLABLE|
                          SERVICE_PAUSABLE;
   ssStatus.svcs_code = SERVICE_UIC_NORMAL;

   uReturnCode = NetServiceStatus((char far *) &ssStatus, sizeof(ssStatus));

   // Make a mailbox to listen to alert messages.

   uReturnCode = DosMakeMailslot(ALERT_MAILBOX, // Mailbox name
                                 MESSAGE_SIZE,  // Maximum message size
                                 MAILSLOT_SIZE, // Mailslot size
                                 &hMailSlot);   // Mailslot handle

   if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRMAKEMAIL, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   /* 
    * Register to receive COFEE_ALERT-type alerts
    * in ALERT_MAILBOX mailbox.
    */

   uReturnCode = NetAlertStart(COFFEE_ALERT, ALERT_MAILBOX, MESSAGE_SIZE);

   // Already registered? If yes, deregister and register fresh.
   if (uReturnCode == NERR_AlertExists)
   {
      uReturnCode = NetAlertStop(COFFEE_ALERT, ALERT_MAILBOX);

      if (uReturnCode != NERR_Success)
      {
         sprintf(szErrBuffer, MSGERRALERTSTOP, uReturnCode);
         ErrOut(uReturnCode, szErrBuffer);
      }

      uReturnCode = NetAlertStart(COFFEE_ALERT, ALERT_MAILBOX,
                                  MESSAGE_SIZE);

      if (uReturnCode != NERR_Success)
      {
         sprintf(szErrBuffer, MSGERRALERTSTART, uReturnCode);
         ErrOut(uReturnCode, szErrBuffer);
      }
   }
   else if (uReturnCode != NERR_Success)
   {
      sprintf(szErrBuffer, MSGERRALERTSTART, uReturnCode);
      ErrOut(uReturnCode, szErrBuffer);
   }

   fAlertRegistered = TRUE;

   /* 
    * Loop forever now, waiting to receive the alert. Once the 
    * alert is received, tell the user by sending a popup.
    */

   for ( ; ; )
   {
      /* 
       * Check if the service has been asked to pause. Wait until the
       * service is asked to continue.
       */
      uReturnCode = DosSemWait(&hSemPause, SEM_INDEFINITE_WAIT);

      if (uReturnCode != NERR_Success)
      {
         sprintf(szErrBuffer, MSGERRSEMWAIT, uReturnCode);
         ErrOut(uReturnCode, szErrBuffer);
      }

      // Wait to receive alert from LAN Manager.

      uReturnCode = DosReadMailslot(hMailSlot,
                                 cbAlertBuffer,
                                 &cbReturned,
                                 &cbNextSize,
                                 &cbNextPriority,
                                 WAIT_FOR_ALERT_TIME);

      if ((uReturnCode != NERR_Success)
           && (uReturnCode != ERROR_SEM_TIMEOUT))
      {
         sprintf(szErrBuffer, MSGERRREADMAIL, uReturnCode);
         ErrOut(uReturnCode, szErrBuffer);
      }
      // If the alert is received, send a popup.
      if ((uReturnCode == NERR_Success) && (cbReturned))
      {
         VioPopUp(&fPopUp, 0);
         // Paint the screen blue.
         VioWrtNAttr(&bAttr, 25 * 80, 0, 0, 0);
         // Show the message.
         sprintf(sVioString,"%s", ALERT_TYPE_MESSAGE);
         VioWrtCharStrAtt(sVioString, strlen(sVioString),
                          usRow, usColumn, &bAttr, 0);
         usRow += 2;
         sprintf(sVioString, "%s","Hit any key to get back to prompt");
         // Set the cursor positition.
         VioSetCurPos(usRow, usColumn+strlen(sVioString)+1, 0);
         VioWrtCharStrAtt(sVioString, strlen(sVioString),
                          usRow, usColumn, &bAttr, 0);

         // Wait for user to press key.
         KbdCharIn(&kbci, IO_WAIT,0);
         // Close popup.
         VioEndPopUp(0);
      }
   } // End for loop.
   return;
}
