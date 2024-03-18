/*
   NETDOM.C -- a sample program demonstrating the Domain API functions.

   This program requires that the Netlogon service be started on
   the specified server.

      usage: netdom [-s \\server] [-d domain]
         where \\server = Name of the server. A servername must be 
                          preceded by two backslashes (\\).
               domain   = Name of the domain.

   API              Used to...
   ============     ================================================
   NetGetDCName     Find the domain controller for a given domain
   NetLogonEnum     Get details about users at the domain controller

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETDOMAIN
#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>
#include    <time.h>

#include    "samples.h"    // Internal routine header file

#define     BIG_BUFF       32768

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   char *         pszServer = "";            // Server name
   char *         pszDomain = "";            // Domain name
   char           pszDCName[UNCLEN+1];       // Name of domain controller
   int            iCount;                    // Index counter
   unsigned short cRead;                     // Count of entries read
   unsigned short cAvail;                    // Count of entries available
   unsigned short cbBuffer;                  // Size of buffer, in bytes
   API_RET_TYPE   uReturnCode;               // API return code
   struct user_logon_info_2 *pBuffer;        // Data buffer

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 's':                        // -s server name
               pszServer = argv[++iCount];
               break;
            case 'd':                        // -d domain
               pszDomain = argv[++iCount];
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
//  NetGetDCName
//
//  This API returns the name of the server that is the domain
//  controller in the specified domain. If no domain name is given,
//  the name of the primary domain controller is returned.
//========================================================================

   cbBuffer = sizeof(pszDCName);
   uReturnCode = NetGetDCName(pszServer,     // Server; NULL means local
                              pszDomain,     // Domain; NULL means primary
                              pszDCName,     // Return buffer
                              cbBuffer);     // Size of buffer

   printf("NetGetDCName returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
      printf("   the domain controller is %s\n", pszDCName);
   else if (uReturnCode == NERR_DCNotFound)
   {
      printf("   domain controller not found\n");
   }

//========================================================================
//  NetLogonEnum
//
//  This API lists, the logon names, logon times, and machines that the
//  users logged on from. 
//
//  Note: In the printf() statement, the computername is pointed to by 
//  a far pointer and so is formatted with %Fs. NetLogonEnum will NOT list
//  all the users logged on in a domain. It lists only those users logged 
//  on to the specified server.
//========================================================================

   cbBuffer = BIG_BUFF;                      // Can be up to 64K
   pBuffer = (struct user_logon_info_2 *) SafeMalloc(cbBuffer);

   uReturnCode = NetLogonEnum(pszDCName,     // Server name
                       2,                    // Level (0 or 2)
                       (char far *)pBuffer,  // Data returned here
                       cbBuffer,             // Size of supplied buffer
                       &cRead,               // Count of entries read
                       &cAvail);             // Count of entries available

   printf("NetLogonEnum for \"%s\" returned %u\n",
                  pszDCName, uReturnCode);

   if (uReturnCode == NERR_Success || uReturnCode == ERROR_MORE_DATA)
   {
      putenv("TZ=GMT0");         // Allow ctime() to report local time
      for (iCount = 0; iCount < (int) cRead; iCount++)
      {
         printf(" %15s on machine %-15Fs  Logon: %s",
                     pBuffer->usrlog2_eff_name,
                     pBuffer->usrlog2_computer,
                     ctime(&pBuffer->usrlog2_logon_time));
         pBuffer++;
      }
      printf("%hu out of %hu entries returned\n", cRead, cAvail);
   }
   free(pBuffer);
   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-d domain]\n", pszProgram);
   exit(1);
}
