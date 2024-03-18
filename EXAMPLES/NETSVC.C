/*
   NETSVC.C -- a sample program demonstrating NetService API functions.

   This program requires that you have admin privilege on the specified
   server.

   usage:  netsvc [-s \\server] [-v servicename] [-l level]

   where:  \\server    = Name of the server. A servername must be preceded
                         by two backslashes (\\).
           servicename = Name of the service.
           level       = Level of detail requested.

   API                   Used to...
   =================     ================================================
   NetServiceInstall     Install the specified service
   NetServiceControl     Check progress of installation and uninstall the
                         specified service
   NetServiceEnum        List services and their status

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied,
   as to its usability in any given situation.
*/

#define     INCL_BASE
#include    <os2.h>        // MS OS/2 base header files

#define     INCL_NETERRORS
#define     INCL_NETSERVICE
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <conio.h>
#include    <stdlib.h>

#include    "samples.h"    // Internal routine header file

#define A_SERVICE          SERVICE_TIMESOURCE  // Default servicename
#define DEFAULT_WAITTIME   150          // 0.1 seconds; 150 = 1.5 sec
#define MAX_POLLS          5            // Max. checks (5*1.5 sec = 20 sec)
#define WAIT_MULT          10  

void InstallService (char *pszServer, char *pszService);
void UninstallService (char *pszServer, char *pszService);
void Usage(char * pszString);

void main(int argc, char *argv[])
{
   char *                 pszServer = "";  // Default to local machine
   char *                 pszService = A_SERVICE; // Servicename
   char *                 pbBuffer;        // Buffer for return data
   unsigned short         cEntriesRead;    // Count of entries in buffer
   unsigned short         cTotalEntries;   // Count available
   unsigned short         cbBuffer;        // Size of buffer, in bytes
   int                    iCount;          // Index and loop counter
   short                  sLevel =  0;     // Level of detail
   API_RET_TYPE           uRet;            // API function return code
   struct service_info_0 *pSvc0;           // Service info; level 0
   struct service_info_1 *pSvc1;           // Service info; level 1
   struct service_info_2 *pSvc2;           // Service info; level 2

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 's':                        // -s servername
               pszServer = argv[++iCount];
               break;
            case 'v':                        // -v servicename
               pszService = argv[++iCount];
               break;
            case 'l':                        // -l level
               sLevel = (short)(atoi(argv[++iCount]));
               break;
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   } // End for loop 

//======================================================================
//  NetServiceInstall and NetServiceControl
//
//  This API installs a service. Reassure the user that installation is
//  proceeding by using NetServiceControl.
//======================================================================

   InstallService(pszServer, pszService);

//====================================================================
//  NetServiceEnum
//
//  This API displays a list of installed services.
//====================================================================

   // First, a call to see what size buffer is needed.
   uRet = NetServiceEnum(pszServer,      // Servername
                        sLevel,          // Info level
                        NULL,            // Pointer to buffer
                        0,               // Size of buffer 
                        &cEntriesRead,   // Count of entries read
                        &cTotalEntries); // Count of entries available

   // Allocate enough for level 2; big enough for all other levels.
   cbBuffer = cTotalEntries * sizeof (struct service_info_2);
   pbBuffer = SafeMalloc(cbBuffer);
   pSvc0 = (struct service_info_0 *) pbBuffer;     // If sLevel == 0
   pSvc1 = (struct service_info_1 *) pbBuffer;     // If sLevel == 1
   pSvc2 = (struct service_info_2 *) pbBuffer;     // If sLevel == 2

   uRet = NetServiceEnum(pszServer,   // Servername
                     sLevel,          // Info level
                     pbBuffer,        // Data returned here
                     cbBuffer,        // Size of buffer, in bytes
                     &cEntriesRead,   // Count of entries read
                     &cTotalEntries); // Count of entries available

   printf("NetServiceEnum returned %u\n", uRet);
   if (uRet == NERR_Success)
   {
      printf("Services installed");
      if ((pszServer == NULL) || (*pszServer == '\0'))
          printf(" on local server:\n");
      else
          printf(" on server %s:\n", pszServer);
      for (iCount = 0; iCount < (int) cEntriesRead; iCount++)
      {
         switch (sLevel)
         {
            case 0:
               printf("   %s\n", pSvc0->svci0_name);
               pSvc0++;
               break;
            case 1:
               printf("Service:  %s\n", pSvc1->svci1_name);
               printf("   Status :  0x%hX\n", pSvc1->svci1_status);
               printf("   Code   :  0x%lX\n", pSvc1->svci1_code);
               pSvc1++;
               break;
            case 2:
               printf("Service:  %s\n", pSvc2->svci2_name);
               printf("   Status :  0x%hX\n", pSvc2->svci2_status);
               printf("   Code   :  0x%lX\n", pSvc2->svci2_code);
               printf("   Text   :  %s\n", pSvc2->svci2_text);
               pSvc2++;
               break;
            default:
               break;
         } // End switch
      } // End for loop
   } // End if successful return

//====================================================================
//  NetServiceControl
//
//  This API uninstalls the service.
//====================================================================

   UninstallService(pszServer, pszService);
   free(pbBuffer);
   exit(0);
}

//====================================================================
//  NetServiceInstall
//
//  This API installs the service. Reassure the user that installation
//  is proceeding by using NetServiceControl.
//====================================================================

void InstallService (char *pszServer, char *pszService)
{
   int                   iCount = 0;
   unsigned short        usOldCheck = 0;
   unsigned short        usNewCheck = 0;
   unsigned long         ulWaitTime;
   unsigned              uRet;
   struct service_info_2 StatBuf;

   uRet = NetServiceInstall(pszServer,   // Servername
                  pszService,            // Servicename 
                  NULL,                  // No command-line args
                  (char far *) &StatBuf, // Level 2 return buffer
                  sizeof(StatBuf));      // Size of return buffer
   printf("NetServiceInstall %s returned %u\n", pszService, uRet);
   switch (uRet)
   {
      case NERR_Success:
         break;
      /*
       * NERR_BadServiceName and ERROR_FILE_NOT_FOUND can be caused
       * by the absence of the service entry in the LANMAN.INI file
       * or when the entry points to a directory that does not contain
       * the executable service program.
       */
      case NERR_BadServiceName:
      case ERROR_FILE_NOT_FOUND:
         printf("\n%s could not be installed\n", pszService);
         return;

      default:
         return;
   }

   do  // Poll every few seconds.
   {
      uRet = NetServiceControl(pszServer, // Servername
                pszService,               // Servicename
                SERVICE_CTRL_INTERROGATE, // Opcode
                0,                        // Service-specific args
                (char far * ) &StatBuf,   // Return buffer
                sizeof(StatBuf) );        // Size of return buffer

      switch (StatBuf.svci2_status & SERVICE_INSTALL_STATE)
      {
         case SERVICE_INSTALLED:
            printf ("\n%s successfully installed\n", pszService);
            return;

         case SERVICE_INSTALL_PENDING:
            printf(".");
            break;

         default:
            printf ("\nService %s failed to install\n", pszService);
            printf ("NetServiceControl returned status %d\n",
               StatBuf.svci2_status & SERVICE_INSTALL_STATE);
            break;
      }

      /*
       * Check the service timing hints. As long as the hints are being
       * changed, assume that the service is still alive.
       */
      if (StatBuf.svci2_code & SERVICE_CCP_QUERY_HINT)
      {
         usNewCheck = (unsigned)
            (StatBuf.svci2_code & SERVICE_CCP_CHKPT_NUM);
         if (usNewCheck != usOldCheck)   // Hints are being changed
         {
            iCount = 0;
            usOldCheck = usNewCheck;
         }
      }

      // Get wait time from data structure.

      ulWaitTime = (long) WAIT_MULT * ( ( StatBuf.svci2_code
                          &  SERVICE_IP_WAIT_TIME )
                          >> SERVICE_IP_WAITTIME_SHIFT);

      // Provide a default wait time if the service doesn't give one.

      if (ulWaitTime == 0)
         ulWaitTime = DEFAULT_WAITTIME;

      // If we've gone maximum amount of time without an update, fail.

      if (((unsigned long)iCount) >= (MAX_POLLS * ulWaitTime))
      {
         printf("\n%s failed to install. ", pszService);
         printf("The service did not report an error.\n");
         break;
      }
      /*
       * DosSleep works only with MS OS/2 or if the application is bound.
       * Change to a for() loop to run as a pure MS-DOS application.
       */
      DosSleep(ulWaitTime);
      iCount++;

   } while ((StatBuf.svci2_status & SERVICE_INSTALL_STATE)
              == SERVICE_INSTALL_PENDING );

   // Successful installation returns true from the switch statement.
   return;
}

//====================================================================
//  NetServiceControl
//
//  This API uninstalls the service.
//====================================================================

void UninstallService (char *pszServer, char *pszService)
{
   int                   iCount = 0;
   unsigned              uRet;
   unsigned long         ulWaitTime;
   struct service_info_2 StatBuf;

   uRet = NetServiceControl (pszServer,    // Servername
                  pszService,              // Servicename
                  SERVICE_CTRL_UNINSTALL,  // Opcode
                  0,                       // Service-specific args
                  (char far *) &StatBuf,   // Return buffer
                  sizeof(StatBuf));        // Size of return buffer
   printf("NetServiceControl %s returned %u\n", pszService, uRet);

   do     // Poll every few seconds.
   {
      uRet = NetServiceControl(pszServer,
                pszService,
                SERVICE_CTRL_INTERROGATE,
                0,
                (char far * ) &StatBuf,
                sizeof(StatBuf));

      switch (StatBuf.svci2_status & SERVICE_INSTALL_STATE)
      {
         case SERVICE_UNINSTALLED:
            printf ("\nService %s successfully stopped\n", pszService);
            return;
         case SERVICE_UNINSTALL_PENDING:  // Keep waiting
            break;
         default:
            printf ("\nService %s failed to stop\n", pszService);
            printf ("NetServiceControl returned status %d\n",
                       StatBuf.svci2_status & SERVICE_INSTALL_STATE);
            break;
      }

      // Get wait time from data structure.

      ulWaitTime = (long) WAIT_MULT * ( ( StatBuf.svci2_code
                             &  SERVICE_IP_WAIT_TIME )
                             >> SERVICE_IP_WAITTIME_SHIFT);

      // Provide a default wait time if the service doesn't give one.

      if (ulWaitTime == 0)
         ulWaitTime = DEFAULT_WAITTIME;

      // If service is not stopped in after 20 polls, fail.
      if (((unsigned long)iCount) >= (MAX_POLLS * ulWaitTime))
      {
         printf( "\n%s failed to stop: ", pszService);
         printf(" The service did not report an error.\n");
         break;
      }

      printf (".");     // Display to let user know program is active.
      /*
       * DosSleep only works under OS/2 or if the application is bound.
       * Change to a for() loop to run as a pure DOS application.
       */
      DosSleep( ulWaitTime );
      iCount++;

   } while ((StatBuf.svci2_status & SERVICE_INSTALL_STATE)
          == SERVICE_UNINSTALL_PENDING);

   // Successful installation returns from the switch statement.
   return;
}
//=================================================================
//  Usage
//
//  Display possible command-line switches for this example.
//=================================================================

void Usage(char * pszString)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-v servicename]", pszString);
   fprintf(stderr, " [-l level]\n");
   exit(1);
}
