/*
   NETMAIL.C -- a sample program demonstrating Mailslot API functions.

   This program requires no special privilege.
     usage:  netmail  [-m mailslot] [-s size] [-t text] [-c class]
                      [-p priority] [-i iterations]
         where  mailslot   = Name of mailslot to be used.
                size       = Maximum message size for mailslot.
                text       = Text of message sent to mailslot.
                class      = Class of message sent to mailslot (1 or 2).
                priority   = Priority of message sent to mailslot (0 to 9).
                iterations = Number of times to send the message.

   API                  Used to...
   =================    ==================================================
   DosMakeMailslot      Make a mailslot on the local computer
   DosWriteMailslot     Write a message to the created mailslot
   DosMailslotInfo      Get information about the mailslot and print it
   DosPeekMailslot      Read the most current message without removing it
   DosReadMailslot      Read and then remove the most current message
   DosDeleteMailslot    Delete the created mailslot

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETERRORS
#define     INCL_NETMAILSLOT
#include    <lan.h>             // LAN Manager header files

#include    <stdio.h>           // C run-time header files
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"         // Internal routine header file

#define     DEFAULT_MAILSLOT    "\\mailslot\\testname"
#define     DEFAULT_MESSAGE     "message sent to mailslot"
#define     DEFAULT_MSGSIZE     1024       // Max. message size
#define     DEFAULT_CLASS       1          // First class
#define     DEFAULT_PRIORITY    0          // Lowest priority
#define     DEFAULT_ITERATIONS  1          // Send message once

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   char *         pszMailslot = DEFAULT_MAILSLOT;   // Mailslot to use
   char *         pszMessage  = DEFAULT_MESSAGE;    // Message to be sent
   char *         pbBuffer;                // Pointer to data buffer
   int            iCount;                  // Index counter
   int            cIterations = DEFAULT_ITERATIONS; // Iteration counter
   unsigned       hMailslot;               // Handle to mailslot 
   unsigned short cbBuffer;                // Size of data buffer 
   unsigned short cbMessageSize = DEFAULT_MSGSIZE;
   unsigned short cbMailslotSize, cMessages;
   unsigned short cbReturned, cbNextSize, usNextPriority;
   unsigned short usPriority = DEFAULT_PRIORITY;
   unsigned short usClass = DEFAULT_CLASS;
   API_RET_TYPE   uReturnCode;             // API return code

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 'm':                        // -m mailslot name
               pszMailslot = argv[++iCount];
               break;
            case 's':                        // -s max. message size
               cbMessageSize = atoi(argv[++iCount]);
               break;
            case 't':                        // -t text
               pszMessage = argv[++iCount];
               break;
            case 'p':                        // -p priority
               usPriority = atoi(argv[++iCount]);
               break;
            case 'c':                        // -c class
               usClass = atoi(argv[++iCount]);
               break;
            case 'i':                        // -i iterations
               cIterations = atoi(argv[++iCount]);
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
// DosMakeMailslot
//
// This API creates a mailslot on the local computer. The mailslot 
// size is set to 0, indicating to the API to choose a value based 
// on the size of the message buffer.
//========================================================================

   uReturnCode = DosMakeMailslot (
                        pszMailslot,       // Mailslot name
                        cbMessageSize,     // Max. message size
                        0,                 // Size of mailslot 
                        &hMailslot);       // Handle to mailslot

   printf("DosMakeMailslot of \"%s\" returned %u \n",
              pszMailslot, uReturnCode);

   if (uReturnCode != NERR_Success)
      exit(1);

//========================================================================
// DosWriteMailslot
//
// This API writes cIterations messages to the mailslot just created.
// If the message being written to the mailslot is an ASCIIZ string,
// the specified length must include the NUL terminator.
//========================================================================

   for (iCount = 1; iCount <= cIterations; iCount++)
   {
      uReturnCode = DosWriteMailslot(
                        pszMailslot,       // Mailslot name
                        pszMessage,        // Message to write to mailslot
                        strlen(pszMessage)+1,  // Length; allow for NUL
                        usPriority,            // Message priority
                        usClass,               // Message class
                        0L);                   // Immediate time-out

      printf("DosWriteMailslot #%d returned %u \n",
                  iCount, uReturnCode);
   }

//========================================================================
// DosMailslotInfo
//
// This API gets information about the mailslot and then prints it.
//========================================================================

   uReturnCode = DosMailslotInfo(
                        hMailslot,         // Handle to mailslot
                        &cbMessageSize,    // Max. message size accepted
                        &cbMailslotSize,   // Size of mailslot
                        &cbNextSize,       // Size of next message
                        &usNextPriority,   // Priority of next message
                        &cMessages);       // Count of messages in mailslot

   printf("DosMailslotInfo returned %u \n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      printf("   Message buffer size : %hu \n", cbMessageSize); 
      printf("   Mailslot size       : %hu \n", cbMailslotSize); 
      printf("   Size of next message: %hu \n", cbNextSize);
      if (cbNextSize)
         printf("   Priority of next msg: %hu\n", usNextPriority);
      printf("   Number of messages  : %hu \n", cMessages);
   }

//========================================================================
// DosPeekMailslot
//
// This API reads the most current message without removing it.
//========================================================================

   /*
    *  Allocate a data buffer large enough for the next message.
    *  Use the default buffer size, just in case a message of higher
    *  priority is received between the DosMailslotInfo call and
    *  the DosPeekMailslot call.
    */

   cbBuffer = cbMessageSize;               // Default buffer size
   pbBuffer = SafeMalloc(cbBuffer);

   uReturnCode = DosPeekMailslot(
                        hMailslot,         // Handle to mailslot
                        pbBuffer,          // Buffer for returned message
                        &cbReturned,       // Size of returned message
                        &cbNextSize,       // Size of next message
                        &usNextPriority);  // Priority of next message

   printf("DosPeekMailslot returned %u \n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      printf("   Message received    : \"%s\" \n", pbBuffer); 
      printf("   Size of message read: %hu bytes\n", cbReturned);
      printf("   Size of next message: %hu bytes\n", cbNextSize);
      if (cbNextSize)
         printf("   Priority of next msg: %hu\n", usNextPriority);
   }

//========================================================================
// DosReadMailslot
//
// Read and delete the most current message.
//========================================================================

   uReturnCode = DosReadMailslot(
                        hMailslot,         // Handle to mailslot
                        pbBuffer,          // Buffer for returned message
                        &cbReturned,       // Size of returned message
                        &cbNextSize,       // Size of next message
                        &usNextPriority,   // Priority of next message
                        0L);               // Time-out value; don't wait

   printf("DosReadMailslot returned %u \n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      printf("   Message received    : \"%s\" \n", pbBuffer); 
      printf("   Size of message read: %hu bytes\n", cbReturned);
      printf("   Size of next message: %hu bytes\n", cbNextSize);
      if (cbNextSize)
         printf("   Priority of next msg: %hu\n", usNextPriority);
   }
   free (pbBuffer);

//========================================================================
// DosDeleteMailslot
//
// This API deletes the created mailslot.
//========================================================================

   uReturnCode = DosDeleteMailslot(hMailslot);

   printf("DosDeleteMailslot of \"%s\" returned %u \n",
              pszMailslot, uReturnCode);

   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage:  %s [-m mailslot] [-s size] [-t text] "
      "[-c class]\n\t\t[-p priority] [-i iterations]\n", pszProgram);
   exit(1);
}
