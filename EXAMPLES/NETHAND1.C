/*
   NETHAND1.C -- a sample program demonstrating NetHandle API functions
                 for the server side of named pipes.

   This program must be run on a computer that has the Server service
   running. It is to be used in conjunction with the program NETHAND2
   running on a workstation.

      usage: nethand1 [-p pipename] [-m message]

         where pipename = Name of the pipe to create.
               message  = Message to be sent through the pipe.

   Run NETHAND1 on the server to create a named pipe, then run
   NETHAND2 on a workstation to connect to that named pipe.

   API                  Used to...
   ================     ==========================================
   NetHandleGetInfo     Get the name of the user of the named pipe

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_BASE
#define     INCL_DOSNMPIPES
#include    <os2.h>        // MS OS/2 base header files

#define     INCL_NETERRORS
#define     INCL_NETHANDLE
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>
#include    <chardev.h>
#include    <io.h>

#define PIPENAMELEN        128
#define BUFFLEN            128
#define PIPE_BUFFSIZE      2048
#define DEFAULT_MESSAGE    "Message from server"
#define DEFAULT_PIPE       "handtest"
#define OPENMODE            NP_ACCESS_DUPLEX
#define PIPEMODE            NP_READMODE_BYTE | NP_TYPE_BYTE | NP_WAIT | \
                            NP_UNLIMITED_INSTANCES

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   char * pszMessage  = DEFAULT_MESSAGE;  // Message to send down pipe
   char * pszPipeName = DEFAULT_PIPE;     // Supplied pipename
   char   achBuffer[BUFFLEN];             // Data buffer
   char   achFullPipe[PIPENAMELEN];       // Full name of pipe to open
   int    iArgv;                          // Index for arguments
   unsigned short hPipe;                  // Handle to named pipe
   unsigned short cbAvail;                // Bytes available from GetInfo
   unsigned short usReturnCode;           // MS OS/2 return code
   API_RET_TYPE uReturnCode;              // LAN Manager API return code
   struct handle_info_2 * pHandleInfo2;

   for (iArgv = 1; iArgv < argc; iArgv++)
   {
      if ((*argv[iArgv] == '-') || (*argv[iArgv] == '/'))
      {
         switch (tolower(*(argv[iArgv]+1))) // Process switches
         {
            case 'p':                       // -p pipename
               pszPipeName = argv[++iArgv];
               break;
            case 'm':                       // -m message
               pszMessage = argv[++iArgv];
               break;
            case 'h':
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   }

   strcpy(achFullPipe, "\\PIPE\\");       // Set up named pipe
   strcat(achFullPipe, pszPipeName);

   printf("Creating Named Pipe %s\n", achFullPipe);

   if (usReturnCode = DosMakeNmPipe(
                         achFullPipe,     // Name of pipe to open
                         &hPipe,          // Handle to opened pipe
                         OPENMODE,        // Full duplex
                         PIPEMODE,        // Unlimited opens, blocked mode
                         PIPE_BUFFSIZE,   // Outgoing buffer size
                         PIPE_BUFFSIZE,   // Incoming buffer size
                         0L))             // Time-out value
   {
      printf( "DosMakeNmPipe failed (error %hu).\n", usReturnCode );
      exit(1);
   }

   printf("Waiting for Connect to Pipe... \n" );

   if (usReturnCode = DosConnectNmPipe(hPipe))
   {
      printf("DosConnectNmPipe failed (error %hu).\n", usReturnCode);
      exit(1);
   }

   printf("Waiting for message from pipe... \n" );

   if (read( hPipe, achBuffer, BUFFLEN ) == -1)
   {
      printf("   Read message failed (error %d)\n", errno);
      exit(1);
   }
   else
   {
      printf("   \"%s\"\n", achBuffer);
   }

   /*
    * Write message to the named pipe. Make sure that the message
    * length specified includes the terminating NUL.
    */

   printf("Writing message \"%s\" \n", pszMessage );

   if (write(hPipe, pszMessage, strlen(pszMessage) + 1) == -1)
   {
      printf("   Write message failed (error %d)\n", errno);
      exit(1);
   }

//========================================================================
//  NetHandleGetInfo
//
//  This API gets the name of the user who is connected to the named pipe. 
//  It can be called only at level 2 on the server end of a handle.
//========================================================================

   uReturnCode = NetHandleGetInfo(
                     hPipe,               // Handle to named pipe
                     2,                   // Level 2
                     achBuffer,           // Data returned here
                     BUFFLEN,             // Size of buffer, in bytes
                     (unsigned short far *) &cbAvail);

   printf("NetHandleGetInfo returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      pHandleInfo2 = (struct handle_info_2 *) achBuffer;
      printf("   User of the named pipe is %Fs\n",
                                pHandleInfo2->hdli2_username );
   }

   do                                     // Wait for disconnect
   {
   } while (DosConnectNmPipe(hPipe) == 0 );

   if ((usReturnCode = DosDisConnectNmPipe(hPipe)) == 0)
      printf("Pipe disconnected. \n");
   else
   {
      printf("DosDisConnectNmPipe failed (error %hu).\n", usReturnCode);
      exit(1);
   }
   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-p pipename] [-m message] \n", pszProgram);
   exit(1);
}
