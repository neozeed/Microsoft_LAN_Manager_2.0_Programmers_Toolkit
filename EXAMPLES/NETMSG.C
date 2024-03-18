/*
    NETMSG.C -- a sample program demonstrating NetMessageBufferSend.

    This program requires that the Messenger service be running on
    the specified server, and that you have admin privileges or
    accounts, server, print, or comm operator privilege on that server.

    usage:  netmsg [-s \\server] [-r recipient] [-m message]

       where \\server   = Name of the server. A servername must be
                          preceded by two backslashes (\\).
              recipient = Name of the message recipient.
              message   = Message to be sent.

    Note: When supplying a username as the name of the recipient, the name 
    is case-sensitive. If the name has been added from the command-line 
    or from the full-screen interface, it will be uppercase. If the
    name has been added using NetMessageNameAdd, the case will be 
    unaltered.

    API                      Used to...
    ====================     ==========================================
    NetMessageBufferSend     Sends a buffer (message) of information to
                             a registered message alias

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define   INCL_NETERRORS
#define   INCL_NETMESSAGE
#define   INCL_NETWKSTA
#include  <lan.h>          // LAN Manager header files

#include  <stdio.h>        // C run-time header files
#include  <stdlib.h>
#include  <string.h>

#include  "samples.h"      // Internal routine header file

#define DEFAULT_MESSAGE    "Hi there!"
#define TRUE               1
#define FALSE              0

void Usage (char * pszProgram);

void main(int argc, char * argv[])
{
   char * pszServer = NULL;                // NULL means local machine
   char * pszRecipient;                    // Message recipient
   char * pszMessage = DEFAULT_MESSAGE;    // Message to send
   char * pbBuffer;                        // Data buffer
   char   achDomain[DNLEN + 1];            // Domain, allow for * at end
   int    iCount;                          // Index counter
   short  fRecipient = FALSE;              // Flag if recipient specified
   unsigned short cbAvail;                 // Count of bytes available
   struct wksta_info_10 * pWksta10;        // Pointer to workstation info
   API_RET_TYPE uReturnCode;               // API return code

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1)))   // Process switches
         {
            case 's':                          // -s server
               pszServer = argv[++iCount];
               break;
            case 'r':                          // -r recipient
               pszRecipient = argv[++iCount];
               fRecipient = TRUE;
               break;
            case 'm':                          // -m message
               pszMessage = argv[++iCount];
               break;
            case 'h':
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   }

//=========================================================================
//  NetMessageBufferSend
//
//  This API sends a message to another user/computer. If no message 
//  recipient is specified, this example calls NetWkstaGetInfo to determine
//  the primary domain for the workstation, then sends a broadcast message
//  to all computers in that domain. First, NetWkstaGetInfo is called with 
//  a NULL buffer to determine the amount of data available, then 
//  NetWkstaGetInfo is called again with the returned buffer size.
//=========================================================================

   if (fRecipient == FALSE)
   {
      uReturnCode = NetWkstaGetInfo(
                        pszServer,         // Servername
                        10,                // Level of detail
                        NULL,              // Data buffer
                        0,                 // Size of buffer
                        &cbAvail);         // Count of bytes available

      if (uReturnCode != NERR_BufTooSmall)
      {
        printf("NetWkstaGetInfo for \"%s\" failed with %u\n",
                pszServer, uReturnCode);
        exit(1);
      }

      // Allocate a data buffer large enough for workstation information.
      pbBuffer = SafeMalloc(cbAvail);

      uReturnCode = NetWkstaGetInfo(
                        pszServer,         // Servername
                        10,                // Level of detail
                        pbBuffer,          // Data buffer
                        cbAvail,           // Size of buffer
                        &cbAvail);         // Count of bytes available

      if (uReturnCode != NERR_Success)
      {
        printf("NetWkstaGetInfo for \"%s\" failed with %u\n",
                pszServer, uReturnCode);
        exit(1);
      }

      // Add * to domain name so that the message is broadcast.

      pWksta10 = (struct wksta_info_10 *) pbBuffer;
      FarStrcpy((char far *)achDomain, pWksta10->wki10_logon_domain);
      strcat(achDomain, "*");
      pszRecipient = achDomain;
   }

   uReturnCode = NetMessageBufferSend(
                        pszServer,            // Servername
                        pszRecipient,         // Message recipient
                        pszMessage,           // Message to send
                        strlen(pszMessage));  // Length of message

   printf("NetMessageBufferSend returned %u\n", uReturnCode);
   printf("   Sending message to \"%s\"\n", pszRecipient);
   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-r recipient]"
                   " [-m message]\n", pszProgram);
   exit(1);
}
