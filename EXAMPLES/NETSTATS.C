/*
   NETSTATS.C -- a sample program demonstrating NetStatisticGet2.

   This program requires that you have admin privilege or server
   operator privilege on the specified server if a servername
   parameter is supplied.

   usage:  netstats [-s \\server] [-v service] [-clear]

   where  \\server   = Name of the server. A servername must be preceded
                       by two backslashes (\\).
          service    = Name of the service.
          clear      = Flag to specify to clear the statistics.

   API                   Used to...
   =================     ===============================================
   NetStatisticsGet2     Get, and optionally clear, operating statistics
                         for a service.

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETERRORS
#define     INCL_NETSTATS
#include    <lan.h>             // LAN Manager header files

#include    <stdio.h>           // C run-time header files
#include    <stdlib.h>
#include    <string.h>
#include    <time.h>

#include    "samples.h"         // Internal routine header file

#define     DEFAULT_SERVICE     "WORKSTATION"

void usage (char * pszProgram);

void main(int argc, char * argv[])
{
   char * pszServer = "";                 // Servername
   char * pszService = DEFAULT_SERVICE;   // Service on which to get stats
   char * pbBuffer;                       // Pointer to data buffer
   int    iCount;                         // Index for arguments
   API_RET_TYPE   uReturnCode;            // API return code
   unsigned short cbBuffer;               // Size of buffer
   unsigned short cbTotalAvail;           // Count of bytes available
   unsigned long  flClear = 0;            // 0 = leave stats; 1 = clear
   struct stat_server_0 * pStatServ0;     // Server statistics
   struct stat_workstation_0 * pStatWksta0;  // Workstation statistics

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1)))     // Process switches
         {
            case 's':                            // -s server name
               pszServer = argv[++iCount];
               break;
            case 'v':                            // -v service
               pszService = argv[++iCount];
               break;
            case 'c':                            // -clear
               flClear = STATSOPT_CLR;
               iCount++;
               break;
            case 'h':
            default:
               usage(argv[0]);
         }
      }
      else
         usage(argv[0]);
   }

//========================================================================
//  NetStatisticsGet2
//
//  This API gets, and optionally clears, operating statistics for a 
//  service. Only the Server and Workstation services are supported.
//  Note: To calculate the local starting time of the statistics,
//  the environment variable TZ should be set to GMT0.
//========================================================================

   // Allocate a data buffer large enough for the statistics.
   if (strcmpi(pszService, "SERVER"))
      cbBuffer = sizeof(struct stat_workstation_0);
   else
      cbBuffer = sizeof(struct stat_server_0);

   pbBuffer = SafeMalloc(cbBuffer);

   uReturnCode = NetStatisticsGet2(
                           pszServer,        // Servername
                           pszService,       // Name of service to report
                           0L,               // Reserved; must be 0L
                           0,                // Level; must be 0
                           flClear,          // Options flag
                           pbBuffer,         // Information buffer
                           cbBuffer,         // Size of information buffer
                           &cbTotalAvail);   // Count of bytes available

   printf("NetStatisticsGet2 returned %u\n", uReturnCode);

   switch (uReturnCode)
   {
      case NERR_Success:
         putenv("TZ=GMT0");          // Allow ctime() to report local time
         if (strcmpi(pszService, "SERVER"))   // Check if not SERVER
         {
            pStatWksta0 = (struct stat_workstation_0 *) pbBuffer;
            printf("   Starting time of statistics        : %s",
                    ctime((const time_t *)&(pStatWksta0->stw0_start)));
            printf("   Workstation bytes sent (high DWORD): %lu\n",
                    pStatWksta0->stw0_bytessent_r_hi);
            printf("   Workstation bytes sent (low DWORD) : %lu\n",
                    pStatWksta0->stw0_bytessent_r_lo);
         }
         else                                // Must be SERVER
         {
            pStatServ0 = (struct stat_server_0 *) pbBuffer;
            printf("   starting time of statistics   : %s",
                    ctime((const time_t *)&(pStatServ0->sts0_start)));
            printf("   server bytes sent (high DWORD): %lu\n",
                    pStatServ0->sts0_bytessent_high);
            printf("   server bytes sent (low DWORD) : %lu\n",
                    pStatServ0->sts0_bytessent_low);
         }
         printf("%hu bytes of data available\n", cbTotalAvail);
         break;

      case NERR_BadTransactConfig:
         printf("   NetStatisticsGet2 requires the server to share IPC$\n");

      case ERROR_NOT_SUPPORTED:
         printf("   Service name must be SERVER or WORKSTATION\n");
         break;

      default:
         break;
   }
   free (pbBuffer);
}

void usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-v service] [-clear]\n",
                   pszProgram);
   exit(1);
}
