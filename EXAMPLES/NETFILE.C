/*
   NETFILE.C -- a sample program demonstrating NetFile API functions.

   This program requires that you have admin privilege on the specified
   server.

      usage: netfile [-s \\server] [-b basepath] [-u username]

      where \\server = Name of the server. A servername must be preceded by 
                       two backslashes (\\).
            basepath = Enumerate only open files along this path.
            username = Enumerate only files opened by this user.

   API                 Used to...
   ===============     ================================================
   NetFileEnum2        List files in the base path opened by user
   NetFileGetInfo2     Get information available about each listed file
   NetFileClose2       Close specified files on the specified server

   This sample code is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied,
   as to its usability in any given situation.

*/

#define     INCL_NETFILE
#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"    // Internal routine header file

void Usage(char *pszString);

void main(int argc, char *argv[])
{
   char *pszServerName = "";              // Required parameters for calls
   char far *pszBasePath = (char far *) NULL;
   char far *pszUserName = (char far *) NULL;
   API_RET_TYPE   uReturnCode;            // API return codes
   int            iCount;                 // Index counter
   unsigned short cbBuflen;               // Count of bytes
   unsigned short cEntriesRead;           // Entries read
   unsigned short cEntriesRemaining;      // Entries remaining to be read
   unsigned short cGetEntries = 0;        // Count of all enumerated IDs
   unsigned short cbTotalAvail;           // Count of bytes available
   unsigned short fTableAllocated = 0;    // Flag to build table of IDs
   struct file_info_3 *pBuf3, *p3;        // File IDs; use only level 2,3
   FRK resumekey;   // File resume key, used when enum data > buffer size
   unsigned long *pulIds, *pulStartId;    // List of file IDs

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 's':                        // -s servername
               pszServerName = argv[++iCount];
               break;
            case 'b':                        // -b base path
               pszBasePath = (char far *)argv[++iCount];
               break;
            case 'u':                        // -u username
               pszUserName = (char far *)argv[++iCount];
               break;
            case 'h':
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   } // End for loop 

   printf("\nFile Category API Examples\n");
   if (pszServerName[0] != '\0')
      printf("Server = %s\n", pszServerName);
   if (pszBasePath != NULL)
      printf("Base path = %s\n", pszBasePath);
   if (pszUserName != NULL)
      printf("User name = %s\n", pszUserName);

//========================================================================
//  NetFileEnum2
//
//  This API lists all open files on the server below the specified given
//  base path. If no base path is given, all open files are listed.
//========================================================================

   cbBuflen = 256;      // Small size to demonstrate use of resume key
   pBuf3 = (struct file_info_3 *) SafeMalloc(cbBuflen);
   p3 = pBuf3;          // Save pointer for free() cleanup at end
   FRK_INIT(resumekey); // Must init file resume key; use SHARES.H macro

   do                   // Use resume key and loop until done
   {
      uReturnCode = NetFileEnum2(pszServerName,    // NULL means local
                               pszBasePath,        // NULL means root
                               pszUserName,        // NULL means all users
                               3,                  // Level (0 through 3)
                               (char far *)pBuf3,  // Return buffer
                               cbBuflen,           // Return buffer length
                               &cEntriesRead,      // Count of entries read
                               &cEntriesRemaining, // Entries not read
                               &resumekey);        // Resume key

      printf("NetFileEnum2 returned %u\n", uReturnCode); 
      if (uReturnCode == NERR_Success || uReturnCode == ERROR_MORE_DATA)
      {
         printf("   Entries read = %hu, Entries remaining = %hu\n", 
                       cEntriesRead, cEntriesRemaining);
            // Save the file IDs.
         if (cEntriesRead > 0)
         {
            // Allocate memory for file ID table first time through only.
            if (fTableAllocated == 0) 
            {
               cGetEntries = cEntriesRead + cEntriesRemaining;
               pulStartId = pulIds = (unsigned long *) 
                          SafeMalloc(cGetEntries * sizeof(unsigned long));
               fTableAllocated = 1;         // Assure allocate only once
            }

            // Print the file information.
            printf("   Id      Perms   Locks   User            Path\n");
            for (iCount = 0; iCount < (int) cEntriesRead; iCount++)
            {
               printf("   %-8lu%-8hu%-8hu%-16Fs%Fs\n", pBuf3->fi3_id,
                     pBuf3->fi3_permissions, pBuf3->fi3_num_locks,
                     pBuf3->fi3_username, pBuf3->fi3_pathname);
               *pulIds = pBuf3->fi3_id;
               pulIds++;
               pBuf3++;
            } // End for loop
            pBuf3 = p3;   // Pointer was changed; restore to top of buffer.
         } // End if cEntriesRead > 0
      } // End if successful call
   } while (uReturnCode == ERROR_MORE_DATA); // Use FRK until enum all
   free(p3);

//========================================================================
//  NetFileGetInfo2
//
//  This API retrieves all file IDs listed from the NetFileEnum2 call.
//========================================================================

   if (cGetEntries != 0)
   {
      cbBuflen = 1024;
      pBuf3 = (struct file_info_3 *) SafeMalloc(cbBuflen);
      p3 = pBuf3;           // Save pointer for free() cleanup at end
      pulIds = pulStartId;  // Start at beginning of list
      printf("NetFileGetInfo2 results:\n");
      for (iCount = 0; iCount < (int) cGetEntries; iCount++)
      {
         uReturnCode = NetFileGetInfo2(
                              pszServerName,      // NULL means local
                              *pulIds,            // File ID from enum 
                              3,                  // Level (0 through 3)
                              (char far *)pBuf3,  // Return buffer
                              cbBuflen,           // Return buffer length
                              &cbTotalAvail);     // Entries not yet read

         if (uReturnCode)
            printf("NetFileGetInfo2 for file %lu returned %u\n",
                  *pulIds, uReturnCode);
         else
            printf("   File %lu: %-8hu%-8hu%-16Fs%Fs\n", *pulIds,
                  pBuf3->fi3_permissions, pBuf3->fi3_num_locks,
                  pBuf3->fi3_username, pBuf3->fi3_pathname);
         pulIds++;
      }
   }

//========================================================================
//  NetFileClose2
//
//  This API closes the specified open files on the specified server.
//========================================================================

   if (cGetEntries != 0)
   {
      pulIds = pulStartId;
      for (iCount = 0; iCount < (int) cGetEntries; iCount++)
      {
         uReturnCode = NetFileClose2(pszServerName,    // NULL means local
                              (unsigned long)*pulIds); // File ID from enum

         printf("NetFileClose2 for file %lu returned %u\n",
                        *pulIds, uReturnCode);
         pulIds++;
      }
      free(pulStartId);
   }

   free(p3);
   exit(0);
}

void Usage(char *pszString)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-b basepath] "
                   "[-u username]\n", pszString);
   exit(1);
}
