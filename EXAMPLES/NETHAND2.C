/*
   NETHAND2.C -- a sample program demonstrating NetHandle API functions
                 for the workstation side of named pipes.

   This program must be run on a computer that has the Workstation service
   running. It is to be used in conjunction with the program NETHAND1
   running on a server. 

      usage: nethand2 -s \\server [-p pipename] [-m message]

         where  \\server = Name of the server. A servername
                           must be preceded by two backslashes (\\).
                pipename = Name of the pipe to connect to.
                message  = Message to be sent through the pipe.

   Run NETHAND1 on the server to create a named pipe, then run
   NETHAND2 on a workstation to connect to that named pipe.

   API                  Used to...
   ================     ================================================
   NetHandleGetInfo     Get the values for charcount and chartime
   NetHandleSetInfo     Set the value for charcount to half the original

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/


#define     INCL_NETERRORS
#define     INCL_NETHANDLE
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>
#include    <chardev.h>
#include    <fcntl.h>
#include    <sys\types.h>
#include    <sys\stat.h>
#include    <share.h>
#include    <io.h>

#define PIPENAMELEN        128
#define BUFFLEN            128
#define DEFAULT_MESSAGE    "Message from workstation"
#define DEFAULT_PIPE       "handtest"

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   char * pszServer   = NULL;
   char * pszMessage  = DEFAULT_MESSAGE;  // Message to send down pipe
   char * pszPipeName = DEFAULT_PIPE;     // Supplied pipename
   char   achFullPipe[PIPENAMELEN];       // Full name of pipe
   char   achBuffer[BUFFLEN];             // Data buffer
   int    iArgv;                          // Index for arguments
   unsigned short hPipe;                  // Handle to named pipe
   unsigned short cbAvail;                // Bytes available from GetInfo
   unsigned short cbCharCount;            // Pipe charcount
   API_RET_TYPE uReturnCode;              // LAN Manager API return code
   struct handle_info_1 HandleInfo1;

   for (iArgv = 1; iArgv < argc; iArgv++)
   {
      if ((*argv[iArgv] == '-') || (*argv[iArgv] == '/'))
      {
         switch (tolower(*(argv[iArgv]+1))) // Process switches
         {
            case 's':                       // -s servername
               pszServer = argv[++iArgv];
               break;
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

   if (pszServer == NULL)                 // Must specify a servername
   {
      Usage(argv[0]);
   }
                                          // Set up full name of pipe
   strcpy(achFullPipe, pszServer);
   strcat(achFullPipe, "\\PIPE\\");
   strcat(achFullPipe, pszPipeName);

   printf("Opening named pipe %s\n", achFullPipe);
   if ((hPipe = sopen(achFullPipe, O_BINARY | O_RDWR, SH_DENYNO))==  -1)
   {
      printf( "   Open of pipe failed (error %d).\n", errno );
      exit(1);
   }        

//=========================================================================
// NetHandleGetInfo
//
//  This API gets the values for chartime and charcount for the named pipe.
//  These values will be the same as the default values for the workstation
//  as listed in the LANMAN.INI file (unless explicitly altered). This API
//  can be called only at level 1 on the workstation end of a serial-device
//  handle.
//=========================================================================

   uReturnCode = NetHandleGetInfo(
                     hPipe,                    // Handle to named pipe
                     1,                        // Level 1
                     (char far *)&HandleInfo1, // Data returned here
                     sizeof(struct handle_info_1),
                     (unsigned short far *) &cbAvail);

   printf("NetHandleGetInfo returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      cbCharCount = HandleInfo1.hdli1_charcount;
      printf("   Chartime  = %ld\n", HandleInfo1.hdli1_chartime);
      printf("   CharCount = %hu\n", cbCharCount);
   }

//========================================================================
//  NetHandleSetInfo
//
//  This API sets the value for charcount to half the default value.
//  There are two ways to call NetHandleSetInfo: If sParmNum is set to
//  PARMNUM_ALL, you must pass a whole structure. Otherwise, you can 
//  set sParmNum to the element of the structure you want to change and
//  pass a pointer to the value. The second method is shown here.
//========================================================================

   cbCharCount /= 2;
   printf("Setting the charcount to %hu\n", cbCharCount);

   uReturnCode = NetHandleSetInfo(
                     hPipe,                    // Handle to named pipe
                     1,                        // Level; must be 1
                     (char far *)&cbCharCount, // Data to be set
                     sizeof(unsigned short),   // Size of buffer 
                     HANDLE_SET_CHAR_COUNT);   // Set the charcount only

   printf("NetHandleSetInfo returned %u\n", uReturnCode);

                                               // Verify the values set
   uReturnCode = NetHandleGetInfo(
                     hPipe,                    // Handle to named pipe
                     1,                        // Level 1
                     (char far *)&HandleInfo1, // Data returned here
                     sizeof(struct handle_info_1),
                     (unsigned short far *) &cbAvail);

   printf("NetHandleGetInfo returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      printf("   Chartime  = %lu\n", HandleInfo1.hdli1_chartime);
      printf("   CharCount = %hu\n", HandleInfo1.hdli1_charcount);
   }

   /*
    * Write a message to the named pipe. Make sure that the 
    * message length specified includes the terminating NUL.
    */
   
   printf("Writing \"%s\" to pipe... \n", pszMessage);

   if (write(hPipe, pszMessage, strlen(pszMessage) + 1) == -1)
   {
      printf("   Write message failed (error %d)\n", errno);
      exit(1);
   }

   printf("Waiting for message from pipe... \n" );

   if (read(hPipe, achBuffer, BUFFLEN) == -1)
   {
      printf("   Read message failed (error %d)\n", errno);
      exit(1);
   }
   printf("    \"%s\"\n", achBuffer);
                               
   printf("Closing pipe...\n");           // Close the pipe
   if (close(hPipe))
   {
      printf("   Close handle failed (error %d)\n", errno);
      exit(1);
   }
   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s -s \\\\server [-p pipename] [-m message] \n",
              pszProgram);
   exit(1);
}
