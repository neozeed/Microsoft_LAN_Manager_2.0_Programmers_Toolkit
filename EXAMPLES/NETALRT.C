/*
   NETALRT.C -- a sample program demonstrating the interaction between
                 the Alerter service and NetAlertRaise.

   This program requires that the Alerter service be started. It
   relies on that service to monitor the admin alert events.
   To receive the alert and display it in a popup, the Messenger
   and Netpopup services must be running and one of the current message
   alias names must be listed in the alertnames entry of the
   LANMAN.INI file on the computer that raises the alert.

      usage:  netalrt [-m message1 [message2 [message3...]]]
         where  message1 = First message to appear in the popup 
                message2 = Second message to appear in the popup 
                message3 = Third message to appear in the popup 
                etc.

   API               Used to...
   =============     =================================================
   NetAlertRaise     Raise an admin-class alert that will appear as a
                     popup on machines with the Messenger and Netpopup
                     services running.

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETALERT
#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>
#include    <time.h>

#include    "samples.h"    // Internal routine header file

#define DEFAULT_MESSAGE    "Hi there!"
#define ALERT_BUFFER       512
#define PROGRAM_NAME       "NETALRT"

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   API_RET_TYPE uReturnCode;               // API return code
   char * pbBuffer;                        // Pointer to data buffer
   char * psMergeString;                   // Pointer to alert strings
   int    iCount;                          // Index counter
   unsigned short cbBuffer = ALERT_BUFFER; // Buffer size
                                           // Alert data header info
   struct std_alert *pAlertInfo = (struct std_alert *) pbBuffer;
   struct admin_other_info * pAdminInfo;   // Admin alert data

   // Allocate a data buffer large enough to store the messages.
   pbBuffer = SafeMalloc(cbBuffer);

   // Set up the alert data structure.
   pAlertInfo = (struct std_alert *) pbBuffer;
   strcpy(pAlertInfo->alrt_eventname, ALERT_ADMIN_EVENT);
   strcpy(pAlertInfo->alrt_servicename, PROGRAM_NAME);

   pAdminInfo = (struct admin_other_info *)ALERT_OTHER_INFO(pAlertInfo);
   pAdminInfo->alrtad_errcode = -1;
   pAdminInfo->alrtad_numstrings = 0;
   psMergeString = ALERT_VAR_DATA(pAdminInfo);

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 'm':                        // -m Message
               for ( ; iCount < argc-1; )    // Merge all message strings
               {
                  iCount++;
                  strcpy(psMergeString, argv[iCount]);
                  psMergeString = psMergeString+1+strlen(argv[iCount]);
                  pAdminInfo->alrtad_numstrings++;
               }
               break;
            case 'h':
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   }

   // Display at least one string.
   if (pAdminInfo->alrtad_numstrings == 0)
   {
      strcpy(psMergeString, DEFAULT_MESSAGE);
      (pAdminInfo->alrtad_numstrings)++;
   }

//========================================================================
//  NetAlertRaise
//
//  This API sends an admin alert. If alrtad_errcode is -1, the Alerter
//  service receives the alert and sends the alert message to all
//  message aliases listed in the alertnames entry of the LANMAN.INI
//  file. These messages are then displayed in a popup if the receiving 
//  computer is running the Messenger and Netpopup services.
//========================================================================

   time(&(pAlertInfo->alrt_timestamp));

   uReturnCode = NetAlertRaise(ALERT_ADMIN_EVENT,  // Admin alert
                       (char far *)pbBuffer,       // Alert information
                       cbBuffer,                   // Buffer size
                       ALERT_MED_WAIT);            // Time-out value

   printf("NetAlertRaise returned %u\n", uReturnCode);
   free (pbBuffer);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-m message1 [message2 [message3...]]]\n",
              pszProgram);
   exit(1);
}
