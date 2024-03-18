/*
   NETSERV.C -- a sample program demonstrating NetServer API functions.

      usage:  netserv  [-s \\servername] [-c comment] [-l level]
                       [-t type] [-a admin command] [-d domain]
                       
      where:  \\server = Name of the server. A servername must be preceded
                         by two backslashes (\\).
              comment  = New comment for the server.
              type     = Types of servers to enumerate.
              level    = Level of detail to be provided/supplied.
              domain   = List of domains to count.
              admin command = Command line for remote admin.

   API                       Used to...
   =====================     ============================================
   NetServerGetInfo          Return information about the server
                             configuration
   NetServerSetInfo          Change the configuration of the server
   NetServerEnum2            List the servers visible on the network
   NetServerDiskEnum         List the disk drives on the server
   NetServerAdminCommand     Execute a sequence of commands on the server

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied,
   as to its usability in any given situation.
*/

#define  INCL_BASE
#include <os2.h>                        // MS OS/2 base header files

#define  INCL_NETERRORS
#define  INCL_NETSERVER
#include <lan.h>                        // LAN Manager header files

#include <stdio.h>                      // C run-time header files
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "samples.h"                    // Internal routine header file

#define A_CMD_STRING       "c: & cd \\ & dir"
#define A_COMMENT          "This is a new comment"
#define BIG_BUFFER_SIZE    32768

void Usage(char *pszString);

void main(int argc, char * argv[])
{
   char * pszServer = "";             // NULL or null string = local
   char * pbBuffer;                   // Pointer to data buffer
   char * pszDisk;                    // Return string NetServerDiskEnum
   char * pszComment = A_COMMENT;     // New comment string
   char * pszCommand = A_CMD_STRING;  // Cmd for NetServerAdminCommand
   char far * pszDomain = NULL;       // Input to NetServerEnum2
   int iCount;                        // Index and loop counter
   short sLevel = 0;                  // Level of detail requested
   API_RET_TYPE uReturnCode;          // API return code
   unsigned short usRemoteRetCode;    // Return code of remote command
   unsigned short cbBuffer;           // Size of buffer, in bytes
   unsigned short cEntriesRead;       // Count of records returned
   unsigned short cTotalEntries;      // Count of records available 
   unsigned short cbReturned;         // Count of bytes returned
   unsigned short cbTotalAvail;       // Count of available bytes 
   unsigned long flServerType = SV_TYPE_SERVER;  // List of servers

   struct server_info_0 *pServer0;    // Pointer to level 0 structure
   struct server_info_1 *pServer1;    // Pointer to level 1 structure
   struct server_info_2 *pServer2;    // Pointer to level 2 structure
   struct server_info_3 *pServer3;    // Pointer to level 3 structure

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 's':                        // -s servername
               pszServer = argv[++iCount];
               break;
            case 'a':                        // -a admin command
               pszCommand = argv[++iCount];
               break;
            case 'c':                        // -c comment
               pszComment = argv[++iCount];
               break;
            case 'l':                        // -l level
               sLevel = (short)(atoi(argv[++iCount]));
               break;
            case 'd':                        // -d domain
               pszDomain = argv[++iCount];
               break;
            case 't':                        // -t type
               flServerType = (unsigned long)(atoi(argv[++iCount]));
               break;
            default:
               Usage(argv[0]);
            }
         }
      else
         Usage(argv[0]);
   } // End for loop 

//========================================================================
//  NetServerGetInfo
//
//  This API returns information about the server's configuration 
//  components. The returned data reflects the current configuration, 
//  including any modifications made by calls to NetServerSetInfo. The 
//  results returned by this API function will not necessarily match the
//  contents of the LANMAN.INI file; use NetConfigGet2 to get the
//  default values stored in the LANMAN.INI file.
//========================================================================

  /*
   *  The structure returned by NetServerGetInfo is a combination
   *  of data elements and pointers to variable-size elements within the
   *  returned data. Because of this, the size of the data buffer passed
   *  to the API must be larger than the size of the structure. The extra
   *  space is needed for variable-length strings such as comment elements.
   *  The first call is used to determine how large a buffer is needed;
   *  the second call is used to obtain the data.
   */

   uReturnCode = NetServerGetInfo(pszServer, // Servername
               sLevel,                       // Reporting level (0,1,2,3)
               NULL,                         // Target buffer for info
               0,                            // Size of target buffer
               &cbTotalAvail);               // Total info available
   cbBuffer = cbTotalAvail;
   pbBuffer = SafeMalloc(cbBuffer);

   uReturnCode = NetServerGetInfo(pszServer, // Servername
               sLevel,                       // Reporting level (0,1,2,3)
               pbBuffer,                     // Target buffer for info
               cbBuffer,                     // Size of target buffer
               &cbTotalAvail);               // Total amount of info
   printf("NetServerGetInfo returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      switch (sLevel)
      {
         case 3:
            pServer3 = (struct server_info_3 *) pbBuffer;
            printf("   Audited events : 0x%lX\n",
                       pServer3->sv3_auditedevents);
         case 2:
            pServer2 = (struct server_info_2 *) pbBuffer;
            printf("   Heuristics     : %Fs\n",
                       pServer2->sv2_srvheuristics);
         case 1:
            pServer1 = (struct server_info_1 *) pbBuffer;
            printf("   Major version #: %hu\n",
                       pServer1->sv1_version_major & MAJOR_VERSION_MASK);
            printf("   Type           : 0x%X\n", pServer1->sv1_type);
            printf("   Comment        : %Fs\n", pServer1->sv1_comment);
         case 0:
            pServer0 = (struct server_info_0 *) pbBuffer;
            printf("   Computer Name  : %s\n", pServer0->sv0_name);
            break;
         default:
            printf("   Level %hu is not supported\n", sLevel);
            break;
      }
   }
   free(pbBuffer);

//========================================================================
//  NetServerSetInfo
//
//  This API function sets configuration components for the specified 
//  server. Note that the function does not change default configuration 
//  parameters in the LANMAN.INI file.
//
//  All SetInfo API functions can be called in two ways: to set all
//  parameters, or to set a single parameter. To set all parameters,
//  call NetServerGetInfo to obtain the current values of all
//  parameters, modify the components to change, and then call
//  NetServerSetInfo. To set one parameter, set the buffer pointer to
//  point to the new value for that parameter, and then call 
//  NetServerSetInfo using the corresponding sParmNum value. In this 
//  example, NetServerSetInfo sets the server's comment field.
//========================================================================

   uReturnCode = NetServerSetInfo(pszServer,    // Servername
                           sLevel,              // Level of detail (0,1)
                           pszComment,          // Pointer to input data
                           strlen(pszComment)+1,// String length + NUL
                           SV_COMMENT_PARMNUM); // Change comment only

   printf("NetServerSetInfo returned %u\n", uReturnCode);
   printf("   Set comment to \"%s\" ", pszComment);
   printf(" %s\n", uReturnCode ? "failed" : "succeeded");

//========================================================================
//  NetServerEnum2
//
//  This API lists all the servers visible on the network in the specified
//  domain. In this example, the domain argument is left NULL, indicating 
//  to list servers in the workstation's domain, its logon domain, and its
//  other domains.
//========================================================================

   cbBuffer = BIG_BUFFER_SIZE;                // Can be up to 64K
   pbBuffer = SafeMalloc(cbBuffer);           // Allocate space for buffer

   uReturnCode = NetServerEnum2(pszServer,    // Servername
                               sLevel,        // Reporting level (0,1)
                               pbBuffer,      // Buffer containing data
                               cbBuffer,      // Buffer size, in bytes
                               &cEntriesRead, // Entries in buffer
                               &cTotalEntries,// Total entries available
                               flServerType,  // Type(s) to enumerate
                               pszDomain);    // Server's domain(s)

   printf("NetServerEnum2 returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      pServer0 = (struct server_info_0 *) pbBuffer;
      pServer1 = (struct server_info_1 *) pbBuffer;
      pServer2 = (struct server_info_2 *) pbBuffer;
      pServer3 = (struct server_info_3 *) pbBuffer;

      for (iCount = 0; iCount < (int) cEntriesRead; iCount++)
      {
         switch (sLevel)
         {
            case 0:
               printf("   Computer Name : %s\n", pServer0->sv0_name);
               pServer0++;
               break;
            case 1:
               printf("   Computer Name : %s\n", pServer1->sv1_name);
               printf("   Type          : 0x%X\n", pServer1->sv1_type);
               pServer1++;
               break;
            case 2:
               printf("  Computer Name : %s\n", pServer2->sv2_name);
               printf("  Type          : 0x%X\n", pServer2->sv2_type);
               pServer2++;
               break;
            case 3:
               printf("  Computer Name : %s\n", pServer3->sv3_name);
               printf("  Type          : 0x%X\n", pServer3->sv3_type);
               pServer3++;
               break;
            default:
               break;
         } // End switch (sLevel)
      } // End for loop
      printf("%hu entries returned out of ", cEntriesRead);
      printf("%hu available\n", cTotalEntries);
   } // End if successful return

//========================================================================
//  NetServerDiskEnum
//
//  This API lists all the local disk drives for the specified server, 
//  including hard disk drives, floppy disk drives, and RAM drives.
//========================================================================

   uReturnCode = NetServerDiskEnum(pszServer,  // Servername
                              0,               // Level; must be 0
                              pbBuffer,        // Results buffer
                              cbBuffer,        // Size of buffer, in bytes
                              &cEntriesRead,   // Count of entries read
                              &cTotalEntries); // Count of entries 
                                               // available

   printf("NetServerDiskEnum returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)        // Print returned info
   {
      pszDisk = pbBuffer;
      printf("   Disk drives on server %s : \n", pszServer);
      for (iCount = 0; iCount < (int) cEntriesRead; iCount++)
      {
         printf("   %s\n", pszDisk);
         pszDisk += (strlen(pszDisk) + 1); // Skip NUL, get next string
      }
      printf("%hu entries returned out of ", cEntriesRead);
      printf("%hu available\n", cTotalEntries);
   }

//========================================================================
//  NetServerAdminCommand
//
//  This API executes a sequence of commands on the specified server.
//  The default command string, "c: & cd \\ & dir", changes the current
//  drive to the C: drive, changes the directory to the root C:\,
//  and performs a list directory DIR command. The results returned
//  in the buffer are displayed.
//========================================================================

   if ((pszServer != NULL) && (pszServer[0] != '\0'))
      printf("On server %s", pszServer);
   else
      printf("On local computer");
   printf(" execute command string \"%s\"\n", pszCommand);

   uReturnCode = NetServerAdminCommand(pszServer, // Servername
                              pszCommand,         // Commands to execute
                              &usRemoteRetCode,   // Ret code of last cmd
                              pbBuffer,           // Results buffer
                              cbBuffer,           // Size of buffer
                              &cbReturned,        // Count of bytes 
                                                  //   returned
                              &cbTotalAvail);     // Count of bytes
                                                  //   available

   printf("NetServerAdminCommand returned %u\n", uReturnCode);
   if (uReturnCode == NERR_Success)               // Print returned info
   {
      printf("Return code of last command : %hu\n", usRemoteRetCode); 
      printf("%hu out of %hu bytes returned\n", cbReturned, cbTotalAvail);
      printf("Buffer contents:\n");
      for (iCount = 0; iCount < (int) cbReturned; iCount++)
         printf("%c", *((char *)pbBuffer++) );
   }

   free(pbBuffer);
   exit(0);
}
//==============================================================
//  Usage
//
//  Display possible command-line switches for this example.
//==============================================================

void Usage(char *pszString)
{
   fprintf(stderr, "Usage: %s [-s \\\\server]", pszString);
   fprintf(stderr, " [-c comment] [-l level]\n [-t type]");
   fprintf(stderr, " [-a admin command] [-d domain(s)]\n");
   exit(1);
}
