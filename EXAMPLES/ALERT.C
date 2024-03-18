/*
   ALERT.C -- This program demonstrates NetAlertRaise and is designed 
              to be used in conjunction with SERVICE.C to demonstrate a
              sample service.

   usage:  ALERT 

   Build Commands:

      For Microsoft C 5.1:
         CL /c ALERT.C
         LINK /NOD /NOI ALERT,,,SLIBCE+DOSCALLS+LAN;

      For Microsoft C 6.0:
         CL /c ALERT.C
         LINK /NOD /NOI ALERT,,,SLIBCE+OS2+LAN;

   Run Instructions:
      a) Build SERVICE.EXE and start in one full-screen session.
      b) Execute ALERT.EXE in another full-screen session.

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, as
   to its usability in any given situation.
*/

#define     INCL_NETALERT
#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <stdlib.h>
#include    <string.h>
#include    <time.h>
#include    <stdio.h>      // C run-time header files

#define PROGRAM_NAME  "ALERT"
#define COFFEE_ALERT  "COFFEE_ALERT"       // Alert to raise

void main (void)
{
   API_RET_TYPE uReturnCode;               // API return code
   struct std_alert StdAlert;              // Standard alert data structure

   // Fill in the std_alert structure
   time(&(StdAlert.alrt_timestamp));
   strcpy(StdAlert.alrt_eventname,   COFFEE_ALERT);
   strcpy(StdAlert.alrt_servicename, PROGRAM_NAME);

   // Issue the alert
   uReturnCode = NetAlertRaise(COFFEE_ALERT,     // Alert type
                       (char far *)&StdAlert,    // Alert information
                       sizeof(struct std_alert), // Buffer size
                       ALERT_MED_WAIT);          // Time-out value

   // Print results
   printf("NetAlertRaise returned %u\n", uReturnCode);
   exit(0);
}
