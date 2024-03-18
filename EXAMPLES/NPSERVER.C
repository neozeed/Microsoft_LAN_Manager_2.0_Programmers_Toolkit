/*
   NPSERVER.C -- A sample program demonstrating named pipes.

   NPSERVER and NPCLIENT work together to demonstrate using named pipes in
   a simple client/server application pair. Since no LAN Manager-specific
   API functions are used, these programs can run without LAN Manager
   being installed.

   usage:  npserver [-c count]
      where count = Number of clients to support.

   Start NPSERVER on a computer running MS OS/2, specifying the number
   of clients to support (see below). Either in other MS OS/2 sessions on
   the same computer or from a remote computer, start as many instances
   of NPCLIENT as NPSERVER was instructed to support. Each NPCLIENT 
   writes and reads one message when any key is pressed. After
   10 messages have been written/read, NPCLIENT terminates. For each
   message received, NPSERVER echos the message to the sender.

   NPSERVER uses only two threads to handle all pipe instances created.
   The primary thread, after creating a pipe instance, associates a
   semaphore with the instance. Then, by calling DosMuxSemWait, NPSERVER
   waits for pipe activity. When DosMuxSemWait returns, NPSERVER responds
   to the pipe instance whose semaphore was cleared, and then waits again.

   The second thread is a monitor thread that wakes every 10 seconds
   and scans for pipe instances that are now closed (that is, the client
   application is no longer using the pipe). The pipe instance is then
   disconnected and reconnected to make it available to possible new
   clients.

   This technique allows multiple pipe instances to be handled with
   a minimum number of threads. Instead of using DosMuxSemWait,
   DosQNmPipeSemState can also be used. It is also possible to associate
   its own thread with each pipe instance.

   API                  Used to...
   ===========          ==================================================
   DosMakeNmPipe        Create a multi-instance named pipe
   DosConnectNmPipe     Start listening for each instance
   DosSetNmPipeSem      Associate each pipe instance with a semaphore
   DosPeekNmPipe        Check if the pipe is still active
   DosDisConnectNmPipe  Disconnect the pipe (so the pipe can be reused)
   DosRead              Read data from the named pipe
   DosWrite             Write data to the named pipe

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_BASE
#include    <os2.h>        // MS OS/2 base header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>
#include    <malloc.h>

#include    "samples.h"    // Internal routine header file

#define  MAXPIPES    (16)        // Max. semaphores for DosMuxSemWait = 16
#define  BUFSIZE     (1024)      // Input and output buffers for named pipes
#define  STACKSIZE   (2048)      // Stack for monitor thread
#define  PIPE_NAME   "\\PIPE\\MYPIPE.XYZ"
#define  SEM_NAME    "\\SEM\\MYSEM.A"

#define  FATALERROR(mess, var)   { fprintf(stderr, mess, var); exit(1); }
DEFINEMUXSEMLIST(MuxList, MAXPIPES)       // Structure for DosMuxSemWait
HPIPE    hPipe[MAXPIPES];                 // Array of pipe handles
USHORT   usNumPipes = 8;                  // Max. instances of pipe created

// Function prototypes
VOID     CreatePipes(USHORT usNumPipes);
VOID     MessageLoop(void);
VOID FAR Monitor(void);
TID      StartMonitor(void);
VOID     Usage (char * pszProgram);


void main(int argc, char *argv[])
{
   INT  iCount;                                // Index counter

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1)))   // Process switches
         {
            case 'c':                          // -c clients
               usNumPipes = atoi(argv[++iCount]);
               usNumPipes = max(usNumPipes, 1);
               usNumPipes = min(usNumPipes, MAXPIPES);
               break;
            case 'h':
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   }

   CreatePipes(usNumPipes);         // Create pipes and associate semaphores
   StartMonitor();                  // Begin monitoring for closed pipes

   printf("Ready to connect to %u clients\n", usNumPipes);

   MessageLoop();

   DosExit(EXIT_PROCESS, 0);
}

//========================================================================
//  CreatePipes
//
//  Create specified number of instances of the named pipe. A more memory-
//  efficient method would be to only make new instances when they are
//  needed. This avoids unneccessary named pipe buffer overhead.
//========================================================================

void CreatePipes(USHORT usNumPipes)
{
   CHAR     *pszPipeName = PIPE_NAME;              // Pipename
   CHAR     *pszSemName  = SEM_NAME;               // Pipe semaphore name
   USHORT   usInst;                                // Pipe instance counter
   USHORT   usResult;                              // MS OS/2 return code

   MuxList.cmxs = usNumPipes;
   if (usResult = DosSetMaxFH(usNumPipes + 20))
      FATALERROR("Error setting max file handles: %hu\n", usResult)

   for (usInst = 0; usInst < usNumPipes; usInst++)
   {
      // Create a new semaphore for each instance of the pipe.
      usResult = DosCreateSem(CSEM_PUBLIC, &MuxList.amxs[usInst].hsem,
                              pszSemName);
      if (usResult)
      {
         if (usResult == ERROR_ALREADY_EXISTS)
            printf("Error: This application has already started.\n");
         else
            printf("Error creating semaphore: %hu\n", usResult);
         exit(1);
      }

      // Set the semaphore name for the next pipe instance (e.g., mysem.b).
      pszSemName[strlen(pszSemName)-1]++;

      usResult = DosMakeNmPipe(pszPipeName,
                 &hPipe[usInst],             // Pipe handle (returned)
                 NP_ACCESS_DUPLEX |          // Open mode
                 NP_WRITEBEHIND,
                 NP_TYPE_MESSAGE |           // Pipe mode
                 NP_NOWAIT       |           // No blocking
                 usNumPipes,                 // Max. number of instances
                 BUFSIZE,                    // Size of output buffer 
                 BUFSIZE,                    // Size of input buffer 
                 0L);                        // Time-out value (use default)
      if (usResult)
         FATALERROR("Error making named pipe: %hu\n", usResult)

      usResult = DosConnectNmPipe(hPipe[usInst]);
      if ((usResult != NO_ERROR) && (usResult != ERROR_PIPE_NOT_CONNECTED))
         FATALERROR("Error connecting named pipe: %hu\n", usResult)

      if (usResult = DosSetNmPipeSem(hPipe[usInst],
                                     MuxList.amxs[usInst].hsem, usInst))
         FATALERROR("DosSetNmPipeSem error: %hu\n", usResult)

      if (usResult = DosSemSet(MuxList.amxs[usInst].hsem))
         FATALERROR("Error setting semaphore: %hu\n", usResult)
   }
}

//========================================================================
//  StartMonitor
//
//  Start monitor thread to check for closed pipes.
//========================================================================

TID StartMonitor(void)
{
   USHORT      usResult;                  // MS OS/2 return code
   BYTE FAR    *pStack;                   // Pointer to child's stack
   TID         tidMonThread;              // Child process TID

   pStack = (BYTE FAR *)SafeMalloc(STACKSIZE);
   if (pStack == NULL)
      FATALERROR("Error: Out of memory.\n", 0)
   pStack += STACKSIZE;                   // Pointer to top of stack

   if (usResult = DosCreateThread(Monitor, &tidMonThread, pStack))
      FATALERROR("Error creating Monitor thread: %hu\n", usResult)

   return(tidMonThread);
}

//========================================================================
//  Monitor
//
//  This thread monitors the status of all pipes every 10 seconds.
//  If it finds a pipe that has been closed (making the pipe no longer
//  usable), it disconnects and reconnects it, making it available again
//  for a client to open it with DosOpen().
//========================================================================

VOID FAR Monitor(void)
{
   USHORT     usInst;                     // Pipe instance counter
   USHORT     usBytesRead;                // Bytes read from pipe
   AVAILDATA  bAvailData;                 // Number of bytes available
   USHORT     usState;                    // Pipe state
   USHORT     usResult;                   // MS OS/2 return code

   for (;;)
   {
      DosSleep(10000L);                   // Sleep for 10 seconds
      for (usInst = 0; usInst < usNumPipes; usInst++)
      {
         usResult = DosPeekNmPipe(hPipe[usInst],   // Pipe handle
                                  NULL,            // Do not read pipe data
                                  0,               
                                  &usBytesRead,    // Number of bytes read
                                  &bAvailData,     // Bytes available
                                  &usState);       // Pipe state
         if (usState & NP_CLOSING)
         {
            usResult = DosDisConnectNmPipe(hPipe[usInst]);
            usResult = DosConnectNmPipe(hPipe[usInst]);
         }
      }
   }
}

//========================================================================
//  MessageLoop
//
//  This is the point where the template ends and your program begins.
//  This example only reads a message, displays it, and sends back a reply.
//  A typical application would first get a logon-type message identifying
//  the user of a particular pipe instance, store the user information (to
//  keep track of which users are using which instances of the pipe), and
//  then process each further request as it came through.
//========================================================================

VOID MessageLoop(void)
{
   USHORT   usInst;                          // Pipe instance counter
   USHORT   usMsgCnt = 0;                    // Message counter
   USHORT   usResult;                        // MS OS/2 return code
   USHORT   usBytesRead;                     // Bytes read by DosRead
   USHORT   usBytesWritten;                  // Bytes written by DosWrite
   CHAR     *pszBuf;                         // Read buffer
   CHAR     pszReply[80];                    // Message to send to client

   // Allocate the read buffer.
   if ((pszBuf = SafeMalloc(BUFSIZE)) == NULL)
      FATALERROR("Out of memory.\n", 0)

   // Process pipe request forever.
   for (;;)
   {
      // Wait for activity to occur on a pipe.
      if (usResult = DosMuxSemWait(&usInst, &MuxList, SEM_INDEFINITE_WAIT))
         FATALERROR("Error in DosMuxSemWait: %hu\n", usResult)

      // Reset the semaphore.
      if (usResult = DosSemSet(MuxList.amxs[usInst].hsem))
         FATALERROR("DosSemSet returns %hu\n", usResult)

      // Read the message from the client.
      usResult = DosRead(hPipe[usInst], pszBuf, BUFSIZE, &usBytesRead);
      if ((usResult == ERROR_NO_DATA) || (usBytesRead == 0))
         continue;                       // False alarm, nothing to read
      printf("Message received from %hu: %s\n", usInst, pszBuf);

      // Reply to the user (with the user's message incremented).
      sprintf(pszReply, "Reply number %u", usMsgCnt++);
      usResult = DosWrite(hPipe[usInst],
                          pszReply,
                          strlen(pszReply)+1,
                          &usBytesWritten);
      if ((usResult != NO_ERROR) ||
          (usBytesWritten != (strlen(pszReply)+1)))
         printf("DosWrite returned %hu with %u bytes written.\n",
                 usResult, usBytesWritten);
   }
}

//========================================================================
//  Usage
//
//  Print out the usage statement, then exit.
//========================================================================

VOID Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-c count] \n", pszProgram);
   exit(1);
};
