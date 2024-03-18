/*
   NETREM.C -- sample program demonstrating NetRemote API functions.

   This program executes the NetRemote APIs with the supplied parameters.
   To execute NetRemoteCopy: supply the parameters starting with -c.
   To execute NetRemoteMove: supply the parameters starting with -m.
   To execute NetRemoteExec: supply the parameters starting with -e.
   To execute NetRemoteTOD:  supply a servername with a -ts switch.
   The source and destination for NetRemoteCopy and NetRemoteMove
   can be specified using either a UNC path or a redirected drive
   letter. NetRemoteExec is carried out on the computer connected
   to the current drive. NetRemoteTOD gets the current time from the 
   specified server.

   usage:

    netrem [-cs copy source] [-cd copy dest] [-cf copy flag] [-co copy open]
           [-ms move source] [-md move dest] [-mf move flag] [-mo move open]
           [-ep executable] [-ea arguments]
           [-ts \\server]

    where copy source    = Complete path of the source file or directory
                           for NetRemoteCopy.
          copy dest      = Complete path of the destination file or
                           directory for NetRemoteCopy.
          copy flag      = Copy flag for NetRemoteCopy.
          copy open flag = Open flag for NetRemoteCopy.
          move source    = Complete path of the source file or directory
                           for NetRemoteMove.
          move dest      = Complete path of the destination file or
                           directory for NetRemoteMove.
          move flag      = Move flag for NetRemoteMove.
          move open flag = Open flag for NetRemoteMove.
          executable     = Name of the program for NetRemoteExec.
          arguments      = Argument string for NetRemoteExec.
          \\server       = Name of the server for NetRemoteTOD. A
                           servername must be preceded by two
                           backslashes (\\).

   API               Used to...
   =============     =====================================================
   NetRemoteCopy     Copy a file or directory on a remote server to
                     another file or directory on a remote server
   NetRemoteMove     Move a file or directory on a remote server to a
                     new file or directory on a remote server
   NetRemoteExec     Execute a program
   NetRemoteTOD      Obtain time of day from a remote server

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETERRORS
#define     INCL_NETREMUTIL
#include    <lan.h>          // LAN Manager header files

#include    <stdio.h>        // C run-time header files
#include    <stdlib.h>
#include    <string.h>

// Define mnemonic bit masks for the functions to execute.
#define DO_NONE              0
#define DO_COPY              0x01
#define DO_MOVE              0x02
#define DO_EXEC              0x04
#define DO_TOD               0x08

// Define mnemonic bit masks for copy and move flag words.
#define REM_OPEN_APPEND      0x01     // If dest exists, append
#define REM_OPEN_OVERWRITE   0x02     // If dest exists, overwrite
#define REM_OPEN_CREATE      0x10     // If dest does not exist, create

#define REM_ASYNCRESULT      0x02     // Equivalent of EXEC_ASYNCRESULT
#define ARG_LEN              128
#define OBJ_LEN              64

void Usage(char *pszString);

void main(int argc, char *argv[])
{
   char   fToDo = DO_NONE;              // NetRemote API to perform
   char   achReturnCodes[OBJ_LEN];      // NetRemoteExec MS OS/2 ret codes
   char   achObjectName[OBJ_LEN];       // NetRemoteExec object name
   char   achArgs[ARG_LEN];             // NetRemoteExec argument string
   char   achEnvs[ARG_LEN];             // NetRemoteExec environment string
   char * pszCopySrc = NULL;            // Can be file or directory
   char * pszCopyDest = NULL;           // Can be file or directory
   char * pszMoveSrc = NULL;            // Can be file or directory
   char * pszMoveDest = NULL;           // Can be file or directory
   char * pszPgmName = NULL;            // Program to be executed
   char * pszArgs;                      // Arguments for program
   char * pszServer = NULL;             // Time servername
   unsigned short fsCopy = 0;           // Copy flag
   unsigned short fsMove = 0;           // Move flag
   unsigned short fsCopyOpen = REM_OPEN_OVERWRITE | REM_OPEN_CREATE;
   unsigned short fsMoveOpen = REM_OPEN_OVERWRITE | REM_OPEN_CREATE;
   int            iCount;               // Index counter
   struct copy_info CopyBuf;            // Return data from NetRemoteCopy
   struct move_info MoveBuf;            // Return data from NetRemoteMove
   struct time_of_day_info TimeBuf;     // Time of day struct in REMUTIL.H
   API_RET_TYPE uRet;                   // Return code from API calls

   for (iCount = 1; iCount < argc; iCount++) // Get command-line switches
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 'c':                        // -c copy
               fToDo |= DO_COPY;
               switch (tolower(*(argv[iCount]+2)))
               {
                  case 's':                  // -cs copy source
                     pszCopySrc = argv[++iCount];
                     break;
                  case 'd':                  // -cd copy destination
                     pszCopyDest = argv[++iCount];
                     break;
                  case 'f':                  // -cf copy flag
                     fsCopy = atoi(argv[++iCount]);
                     break;
                  case 'o':                  // -co copy open flag
                     fsCopyOpen = atoi(argv[++iCount]);
                     break;
                  default:
                     Usage(argv[0]);         // Display usage and exit
               }
               break;
            case 'm':                        // -m move
               fToDo |= DO_MOVE;
               switch (tolower(*(argv[iCount]+2)))
               {
                  case 's':                  // -ms move source
                     pszMoveSrc = argv[++iCount];
                     break;
                  case 'd':                  // -md move destination
                     pszMoveDest = argv[++iCount];
                     break;
                  case 'f':                  // -mf move flag
                     fsMove = atoi(argv[++iCount]);
                     break;
                  case 'o':                  // -mo move open flag
                     fsMoveOpen = atoi(argv[++iCount]);
                     break;
                  default:
                     Usage(argv[0]);         // Display usage and exit
               }
               break;
            case 'e':                        // -e exec
               fToDo |= DO_EXEC;
               switch (tolower(*(argv[iCount]+2)))
               {
                  case 'p':                  // -ep exec executable program
                     pszPgmName = argv[++iCount];
                     achArgs[0] = '\0'; // Set double NUL terminator
                     achArgs[1] = '\0';
                     achEnvs[0] = '\0'; // Set double NUL terminator
                     achEnvs[1] = '\0';
                     break;
                  case 'a':                  // -ea exec argument string
                     pszArgs = achArgs;
                     strcpy(pszArgs, pszPgmName);      // Program name
                     pszArgs += strlen(pszArgs) + 1;   // NUL terminator
                     strcpy(pszArgs, argv[++iCount]);  // Argument string
                     pszArgs += strlen(pszArgs) + 1;   // NUL terminator
                     *pszArgs = '\0';                  // Extra NUL
                     break;
                  default:
                     Usage(argv[0]);         // Display usage and exit
               }
               break;
            case 't':                          // -t time of day
               fToDo |= DO_TOD;
               if (tolower(*(argv[iCount]+2)) == 's')
                  pszServer = argv[++iCount];  // -ts servername
               else
                  Usage(argv[0]);            // Display usage and exit
               break;
            case 'h':
            default:
               Usage(argv[0]);               // Display usage and exit
         }
      }
      else
         Usage(argv[0]);                     // Display usage and exit
   }

   if (fToDo == DO_NONE)
   {
      printf("No operation specified.\n");
      Usage(argv[0]);                        // Display usage and exit
   }

//========================================================================
//  NetRemoteCopy
//
//  This API copies a file or directory on the specified server.
//  The source is copied to the destination according to the flags. 
//  Information about the operation is returned in the CopyBuf structure.
//========================================================================

   if (fToDo & DO_COPY)
   {
      uRet = NetRemoteCopy(pszCopySrc,   // Source path
               pszCopyDest,              // Destination path
               NULL,                     // No password for source path
               NULL,                     // No password for dest path
               fsCopyOpen,               // Open flags
               fsCopy,                   // Copy flags
               (char far *)&CopyBuf,     // Return data
               sizeof(struct copy_info));// Return data size, in bytes

      printf("NetRemoteCopy returned %u\n", uRet);
      if (uRet == NERR_Success)
      {
          printf("   Copied %s to %s\n",pszCopySrc, pszCopyDest);
          printf("   Files copied = %hu\n", CopyBuf.ci_num_copied);
      }
   }

//========================================================================
//  NetRemoteMove
//
//  This API moves a file on the remote server. The source file is renamed 
//  to the name specified by the destination file. After the operation,
//  only one file exists, and its name is the destination filename.
//========================================================================

   if (fToDo & DO_MOVE)
   {
      uRet = NetRemoteMove(pszMoveSrc,    // Source path
               pszMoveDest,               // Destination path
               NULL,                      // No password for source path
               NULL,                      // No password for dest path
               fsMoveOpen,                // Open flags
               fsMove,                    // Move flags
               (char far *) &MoveBuf,     // Return data
               sizeof(struct move_info)); // Return data size, in bytes

      printf("NetRemoteMove returned %u\n",uRet);
      if (uRet == NERR_Success)
      {
          printf("   Moved %s to %s\n", pszMoveSrc, pszMoveDest);
          printf("   Number of files moved = %hu\n",MoveBuf.mi_num_moved);
      }
   }

//========================================================================
//  NetRemoteExec
//
//  This API executes the specified file on the computer connected to
//  the current drive. If the current drive is connected to a 
//  remote server, the file is executed on that server. If the current 
//  drive is local, the file is executed locally. When NETREM.EXE reads 
//  the arguments for the NetRemoteExec call, it adds the name of the 
//  program to be executed to the front of that programs' argument string.
//========================================================================

   if (fToDo & DO_EXEC)
   {
      uRet = NetRemoteExec((char far *)-1L, // Reserved; must be -1
               achObjectName,               // Contains data if error
               OBJ_LEN,                     // Length of error data buffer
               REM_ASYNCRESULT,             // Asynchronous with result code
               achArgs,                     // Argument strings
               achEnvs,                     // Environment strings
               achReturnCodes,              // DosExecPgm return codes
               pszPgmName,                  // Program to execute
               NULL,                        // Reserved; must be NULL
               0);                          // Remexec flags

      if (uRet == NERR_Success)
          printf("\nNetRemoteExec executed %s\n", pszPgmName);
      else
      {
          printf("\nNetRemoteExec returned error %u\n", uRet);
          if (achObjectName[0] != '\0')
             printf("   Error buffer = %s\n", achObjectName);
      }
   }

//=======================================================================
//  NetRemoteTOD
//
//  This API obtains the time of day from the specified server.
//  The time of day structure is defined in the REMUTIL.H header file.
//=======================================================================

   if (fToDo & DO_TOD)
   {
      uRet = NetRemoteTOD(pszServer,            // Servername
              (char far *) &TimeBuf,            // Data returned here
              sizeof(struct time_of_day_info)); // Size of buffer

      printf("NetRemoteTOD returned %u\n", uRet);
      if (uRet == NERR_Success)                 // Call completed OK
      {
          printf("   Time ");
          if ((pszServer != NULL) && (*pszServer != '\0'))
             printf("on server %s = ",pszServer);
          printf("%02u:%02u:%02u ", TimeBuf.tod_hours,
                                    TimeBuf.tod_mins,
                                    TimeBuf.tod_secs);
          printf("%02u/%02u/%u \n", TimeBuf.tod_month,
                                    TimeBuf.tod_day,
                                    TimeBuf.tod_year);
      }
   }
   exit(0);
}

void Usage(char * pszString)
{
   printf("Usage: %s [-cs copy source] [-cd copy dest] [-cf copy flag]"
          " [-co copy open]\n\t   [-ms move source] [-md move dest]"
          " [-mf move flag] [-mo move open]\n\t   [-ep executable]"
          " [-ea arguments]\n\t   [-ts \\\\server for TOD]\n",
          pszString);
   exit(1);
}
