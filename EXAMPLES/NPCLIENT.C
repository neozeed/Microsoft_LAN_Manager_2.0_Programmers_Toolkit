/*
   NPCLIENT.C -- A sample program demonstrating named pipes.

   NPSERVER and NPCLIENT work together to demonstrate using named pipes in
   a simple client/server application pair. Since no LAN Manager specific
   API functions are used, these programs can run without LAN Manager 
   being installed.

   The client application, NPCLIENT, opens the named pipe, sends and
   receives 10 messages (one message each time a key is pressed), closes
   the pipe, and terminates. For additional details, see NPSERVER.C

   NOTE:  Because this program uses Family API functions (FAPIs) for 
          pipe access, it must be built as a bound application.

   usage:  npclient [-s \\server]
      where \\server = Name of the server (defaults to the local 
                       machine). A servername must be preceded by 
                       two backslashes (\\).

   API                  Used to...
   ===========          =================================================
   DosOpen              Open a named pipe
   DosWrite             Write a message to the pipe
   DosRead              Send a message to the pipe
   DosClose             Close a named pipe

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_DOSNMPIPES
#define     INCL_DOSERRORS
#include    <os2.h>        // MS OS/2 header files

#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <conio.h>      // C run-time header files
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"    // Internal routine header file

#define     BUFSIZE                 (1024)
#define     PIPE_NAME               "\\PIPE\\MYPIPE.XYZ"
#define     FATALERROR(mess, var)   { fprintf(stderr, mess, var); exit(1); }

// Function prototypes
VOID Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   USHORT   usResult;                     // MS OS/2 return code
   USHORT   usAction;                     // DosOpen file action
   USHORT   usByteCnt;                    // Bytes read/written
   INT      iCount;                       // Index counter
   HPIPE    hPipe;                        // Named pipe handle
   CHAR     *pbBuf;                       // Receive buffer
   CHAR     pszMessage[80];               // Send buffer
   CHAR     pszPipeName[80];              // Pipename

   *pszPipeName = '\0';

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1)))    // Process switches
         {
            case 's':                           // -s server
               strcpy(pszPipeName, argv[++iCount]);
               break;
            case 'h':
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   }

   strcat(pszPipeName, PIPE_NAME);              // Set pipename

   if ((pbBuf = SafeMalloc(BUFSIZE)) == NULL)   // Allocate data buffer
      FATALERROR("Out of memory.\n", 0)

   usResult = DosOpen(pszPipeName,              // Pipename
                      &hPipe,                   // Pipe handle
                      &usAction,                // Action
                      0L,                       // Initial file size
                      FILE_NORMAL,              // File attribute
                      FILE_CREATE | FILE_OPEN,  // Open flag
                      OPEN_ACCESS_READWRITE |   // Bidirectional pipe
                      OPEN_SHARE_DENYREADWRITE,
                      0L);                      // Reserved; must be 0

   // Check for standard MS OS/2 errors first.
   if (usResult == ERROR_PATH_NOT_FOUND)
      FATALERROR("Unable to open pipe\n", 0)

   else if (usResult == ERROR_PIPE_BUSY)
      FATALERROR("The maximum number of clients are already connected\n", 0)

   /*
    * The next two errors are defined in NETCONS.H and are returned
    * only for remote pipes.
    */
   else if (usResult == ERROR_BAD_NETPATH)
      FATALERROR("Network path \"%s\" is not valid\n", pszPipeName)

   else if (usResult == ERROR_NETWORK_ACCESS_DENIED)
      FATALERROR("You do not have permission on that server\n", 0)

   // Otherwise, if an error was returned, it's an unexpected one.
   else if (usResult)
      FATALERROR("Error opening named pipe: %u\n", usResult)

   printf("Ready to send messages 0 to 9 (press Enter)\n");
   for (iCount = 0; iCount < 10; iCount++)
   {
      getch();                      // Wait for key press

      sprintf(pszMessage, "Message number %u", iCount);
      usResult = DosWrite(hPipe,                // Pipe handle
                          pszMessage,           // Output data
                          strlen(pszMessage)+1, // Output size
                          &usByteCnt);          // Bytes written
      if (usResult != NO_ERROR)
        printf("DosWrite returned error %u\n", usResult);

      usResult = DosRead(hPipe,                 // Pipe handle,
                         pbBuf,                 // Input buffer
                         BUFSIZE,               // Input size
                         &usByteCnt);           // Bytes read
      if (usResult == NO_ERROR)
         printf("Received message: %s\n", pbBuf);
      else
         printf("DosRead returned error %u\n", usResult);
   }

   DosClose(hPipe);
   exit(0);
}

VOID Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] \n", pszProgram);
   exit(1);
};

