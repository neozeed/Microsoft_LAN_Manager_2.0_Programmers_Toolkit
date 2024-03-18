/*
   NETSESS.C -- a sample program demonstrating NetSession API functions.

   This program requires that you have admin privilege or server
   operator privilege on the specified server.

      usage: netsess [-s \\server] [-w \\workstation]
         where  \\server      = Name of the server. A servername must be
                                preceded by two backslashes (\\).
                \\workstation = Name of the client machine to check.

   API                   Used to...
   =================     ================================================
   NetSessionEnum        Display list of workstations connected to server
   NetSessionGetInfo     Check that a particular workstation is connected
   NetSessionDel         Delete a session for a particular workstation

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_BASE
#include    <os2.h>        // MS OS/2 base header files

#define     INCL_NETERRORS
#define     INCL_NETSESSION
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"    // Internal routine header file

#define     STRINGLEN 256
#define     NETWKSTAGETINFOSIZE 1048

// Function prototypes
void Usage  (char * pszProgram);

void main(int argc, char *argv[])
{
   char *         pszServer = "";            // Servername
   char *         pszClientName = "";        // Workstation name
   char *         pbBuffer;                  // Pointer to data buffer
   int            iCount;                    // Index counter
   unsigned short cbBuffer;                  // Size of data buffer
   unsigned short cEntriesRead;              // Count of entries read
   unsigned short cTotalAvail;               // Count of entries available
   API_RET_TYPE   uReturnCode;               // API return code
   struct session_info_2 *  pSessInfo2;      // Session info; level 2
   struct session_info_10 * pSessInfo10;     // Session info; level 10
                             
   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 's':                        // -s server
               pszServer = argv[++iCount];
               break;
            case 'w':                        // -w workstation name
               pszClientName = argv[++iCount];
               break;
            case 'h':
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   }

//========================================================================
//  NetSessionEnum
//
//  This API lists the workstations connected to the server. 
//  Calculate the buffer size needed by determining the number of 
//  sessions and multiplying this value by the size needed to 
//  store the data for one session.
//========================================================================

   // Supply a zero-length buffer and get back the number of sessions.

   uReturnCode = NetSessionEnum(pszServer,     // "" or NULL means local
                               10,             // Level (0,1,2,10)
                               NULL,           // Return buffer
                               0,              // Size of return buffer
                               &cEntriesRead,  // Count of entries read
                               &cTotalAvail);  // Count of total available

   if (uReturnCode != NERR_Success && uReturnCode != ERROR_MORE_DATA)
      printf("NetSessionEnum returned %u\n", uReturnCode);
   else
   {
      printf("There are %hu session(s)\n", cTotalAvail);
      if (cTotalAvail != 0 )
      {
         cbBuffer = cTotalAvail * (sizeof (struct session_info_10) +
                         CNLEN+1+              // Space for sesi10_cname
                         CNLEN+1);             // Space for sesi10_username
         pbBuffer = SafeMalloc(cbBuffer);

         // Buffer is large enough unless new sessions have been created.

         uReturnCode = NetSessionEnum(pszServer, // "" or NULL means local
                                10,              // Level (0,1,2,10)
                                pbBuffer,        // Return buffer
                                cbBuffer,        // Size of return buffer
                                &cEntriesRead,   // Count of entries read
                                &cTotalAvail);   // Count of total available

         // Display information returned by the Enum call.

         if (uReturnCode == NERR_Success || uReturnCode == ERROR_MORE_DATA)
         {
            pSessInfo10 = (struct session_info_10 *) pbBuffer;
            for (iCount = 0; iCount++ < (int) cEntriesRead; pSessInfo10++)
               printf("   \"%Fs\"\n", pSessInfo10->sesi10_cname);
         // May be NULL if uReturnCode != NERR_Success.
         }
         else
            printf("NetSessionEnum returned %u\n", uReturnCode);

         free(pbBuffer);
      }
   }

//========================================================================
//  NetSessionGetInfo
//
//  This API displays information about sessions at level 2 (maximum 
//  information). Call NetSessionGetInfo with a zero-length buffer to 
//  determine the size of buffer required, and then call it again with 
//  the correct buffer size. 
//========================================================================

   uReturnCode = NetSessionGetInfo(pszServer,  // "" or NULL means local
                                pszClientName, // Client to get info on
                                2,             // Level (0,1,2 or 10)
                                NULL,          // Return buffer
                                0,             // Size of return buffer
                                &cbBuffer);    // Count of bytes available

   if (uReturnCode != NERR_BufTooSmall)
      printf("NetSessionGetInfo with 0 byte buffer returned %u\n",
             uReturnCode);
   else
   {
      pbBuffer = SafeMalloc(cbBuffer);

      uReturnCode = NetSessionGetInfo(pszServer, // "" or NULL means local
                                pszClientName,   // Client to get info on
                                2,               // Level (0,1,2, or 10)
                                pbBuffer,        // Return buffer
                                cbBuffer,        // Size of return buffer
                                &cTotalAvail);   // Count of bytes available

      printf("\nNetSessionGetInfo with %hu byte buffer returned %u\n\n",
                 cTotalAvail, uReturnCode);

      if (uReturnCode == NERR_Success )
      {
         pSessInfo2 = (struct session_info_2 *) pbBuffer;
         printf ("  Computer name :  %Fs\n", pSessInfo2->sesi2_cname);
         printf ("  User name     :  %Fs\n", pSessInfo2->sesi2_username);
         printf ("  # Connections :  %hu\n", pSessInfo2->sesi2_num_conns);
         printf ("  # Opens       :  %hu\n", pSessInfo2->sesi2_num_opens);
         printf ("  # Users       :  %hu\n", pSessInfo2->sesi2_num_users);
         printf ("  Seconds active:  %lu\n", pSessInfo2->sesi2_time);
         printf ("  Seconds idle  :  %lu\n", pSessInfo2->sesi2_idle_time);
         printf ("  User flags    :  %lu\n", pSessInfo2->sesi2_user_flags);
         printf ("  Client version:  %Fs\n", pSessInfo2->sesi2_cltype_name);
      }
      free(pbBuffer);
   }

//========================================================================
//  NetSessionDel
//
//  This API deletes the session with the specified workstation.
//========================================================================

   uReturnCode = NetSessionDel(pszServer,      // "" or NULL means local
                               pszClientName,  // Clientname
                               0);             // Reserved; must be 0

   printf("NetSessionDel returned %u\n", uReturnCode );

   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-w \\workstation]\n",
              pszProgram);
   exit(1);
}
