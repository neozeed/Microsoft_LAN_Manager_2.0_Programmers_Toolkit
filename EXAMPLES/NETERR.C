/*
   NETERR.C -- a sample program demonstrating NetErrorLog API functions.

   This program requires that you have admin privilege if a servername
   parameter is supplied.

      usage: neterr [-s \\server] [-b backup]

      where \\server = Name of the server. A servername must be preceded 
                       by two backslashes (\\).
            backup   = Name of the backup file.

   API                  Used to...
   ================     ===========================================
   NetErrorLogClear     Back up the error log and then clear it
   NetErrorLogWrite     Write several entries into the error log
   NetErrorLogRead      Read the error log and display its contents

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETERRORS
#define     INCL_NETERRORLOG
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>
#include    <time.h>

#include    "samples.h"    // Internal routine header file

#define DEFAULT_BACKUP     "ERROR.BCK"

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   char * pszServer = "";                    // Servername
   char * pszBackup = DEFAULT_BACKUP;        // Backup log file
   struct error_log *pBuffer;                // Pointer to data buffer
   struct error_log *pEntry;                 // Single entry in log
   int              iCount;                  // Index counter
   unsigned short   cbBuffer;                // Count of bytes in buffer
   unsigned short   cbRead;                  // Count of bytes read
   unsigned short   cbAvail;                 // Count of bytes available
   unsigned short   usDataByte;              // Raw data
   API_RET_TYPE     uReturnCode;             // API return code
   HLOG             hLogHandle;              // Error log handle
   time_t           tTime;

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 's':                        // -s servername
               pszServer = argv[++iCount];
               break;
            case 'b':                        // -b backup file
               pszBackup = argv[++iCount];
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
//  NetErrorLogClear
//
//  This API clears the error log for the specified server. A backup is
//  kept in the file specified by pszBackup. If a null
//  pointer is supplied, no backup is kept.
//========================================================================

   uReturnCode = NetErrorLogClear(
                     pszServer,              // Servername
                     pszBackup,              // Backup file
                     NULL);                  // Reserved; must be NULL

   printf("NetErrorLogClear returned %u\n", uReturnCode);
   printf("   backup file = %s \n\n", pszBackup);

//========================================================================
//  NetErrorLogWrite
//
//  This API writes a few entries to the error log. These entries are
//  some typical types of errors that may be encountered. The error
//  codes are defined in the ERRLOG.H header file. 
//  Note: Because NetErrorLogWrite has no servername parameter, the entry
//  written into the local error log.
//========================================================================

   /* 
    * Write an entry of type NELOG_Resource_Shortage that has
    * a single text error message and no raw data.
    */

   uReturnCode = NetErrorLogWrite(
                     NULL,                     // Reserved; must be NULL
                     NELOG_Resource_Shortage,  // Error code
                     argv[0],                  // Component in error
                     NULL,                     // Pointer to raw data
                     0,                        // Length of raw data buffer
                     "THREADS=",               // String data
                     1,                        // Number of error strings
                     NULL);                    // Reserved; must be NULL

   printf("NetErrorLogWrite for NELOG_Resource_Shortage returned %u\n",
                     uReturnCode);

   /*
    * Write an entry of type NELOG_Init_OpenCreate_Err that has
    * a single text error message and raw data associated with it.
    */

   usDataByte = 3;                              // Path not found error

   uReturnCode = NetErrorLogWrite(
                     NULL,                      // Reserved; must be NULL
                     NELOG_Init_OpenCreate_Err, // Error code
                     argv[0],                   // Component in error
                     (char far *)&usDataByte,   // Pointer to raw data
                     sizeof(unsigned short),    // Length of raw data buffer
                     "C:\\INIT\\STARTER.CMD",   // String data
                     1,                         // Number of error strings
                     NULL);                     // Reserved; must be NULL

   printf("NetErrorLogWrite for NELOG_Init_OpenCreate_Err returned %u\n",
                     uReturnCode);

   /*
    * Write an entry of type NELOG_Srv_No_Mem_Grow that has
    * no text error message and no raw data associated with it.
    */

   uReturnCode = NetErrorLogWrite(
                     NULL,                   // Reserved; must be NULL
                     NELOG_Srv_No_Mem_Grow,  // Error code
                     argv[0],                // Component in error
                     NULL,                   // Pointer to raw data
                     0,                      // Length of raw data buffer
                     NULL,                   // String data
                     0,                      // Number of error strings
                     NULL);                  // Reserved; must be NULL

   printf("NetErrorLogWrite for NELOG_Srv_No_Mem_Grow returned %u\n\n",
                     uReturnCode);

//========================================================================
//  NetErrorLogRead
//
//  This API reads and displays the error log for the specified server.
//========================================================================

   /*
    * Allocate a small buffer space to demonstate reading the error log
    * when the log is larger than the buffer allocated to store it. The
    * maximum allowable buffer is 64K. If the error log is larger than
    * the buffer specified, the API returns as many full records as it
    * can and the NERR_Success return code. Subsequent reads start from
    * the end of the last record read. To read the whole log, the reads
    * must continue until the bytes available counter is 0.
    */

   cbBuffer = 100;
   pBuffer = SafeMalloc(cbBuffer);           // Allocate memory for buffer

   /* 
    * Set the log handle for reading from the start of the error log.
    * This handle gets modified by the API. Any subsequent reads
    * for unread data should use the returned handle.
    */

   hLogHandle.time = 0L;
   hLogHandle.last_flags = 0L;
   hLogHandle.offset = 0xFFFFFFFF;
   hLogHandle.last_flags = 0xFFFFFFFF;

   do {
      uReturnCode = NetErrorLogRead(
                        pszServer,           // Servername 
                        NULL,                // Reserved; must be NULL
                        &hLogHandle,         // Error log handle
                        0L,                  // Start at record 0
                        NULL,                // Reserved; must be NULL
                        0L,                  // Reserved; must be 0
                        0L,                  // Read the log forward
                        (char far *)pBuffer, // Data returned here
                        cbBuffer,            // Size of buffer, in bytes
                        &cbRead,             // Count of bytes read
                        &cbAvail);           // Count of bytes available

      printf("NetErrorLogRead returned %u \n", uReturnCode);

      if (uReturnCode == NERR_Success)
      {
         for ( pEntry = pBuffer;
               pEntry < (struct error_log *)((char *)pBuffer + cbRead); )
         {
            tTime = (time_t) pEntry->el_time;

            printf("   Error %hu, from %s at %s",
                pEntry->el_error, pEntry->el_name,
                asctime( gmtime ((const time_t *) &tTime) ) );

            pEntry = (struct error_log *)((char *)pEntry + pEntry->el_len);
         }
         printf("Bytes Read = 0x%X\n", cbRead);

         // To read to whole log, keep reading until cbAvail is 0.

         if (cbAvail)
            printf("Data still unread.\n\n");
         else
            printf("All data read.\n\n");
      }
   } while ((uReturnCode == NERR_Success) && (cbAvail != 0));

   free(pBuffer);
   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-b backup]\n", pszProgram);
   exit(1);
}
