/*
   NETAUD.C -- a sample program demonstrating NetAudit API functions.

   This program requires that you have admin privilege on the specified
   server.

      usage: netaud [-s \\server] [-b backup]

      where \\server = Name of the server. A servername must be preceded 
                       by two backslashes (\\).
            backup   = Name of the backup file.

   API               Used to...
   =============     ===========================================
   NetAuditRead      Read the audit log and display its contents
   NetAuditClear     Copy the audit log and then clear it
   NetAuditWrite     Write entries into the audit log

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETERRORS
#define     INCL_NETAUDIT
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <malloc.h>
#include    <time.h>

#define DEFAULT_BACKUP     "AUDIT.BCK"
#define BIG_BUFFER         32768

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   char *         pszServer = "";         // Servername
   char *         pszBackup = DEFAULT_BACKUP;  // Backup audit log 
   int            iCount;                 // Index counter
   HLOG           hLogHandle;             // Audit log handle
   time_t         tTime;                  // Time of entry
   unsigned short cbBuffer;               // Size of buffer, in bytes
   unsigned short cbRead;                 // Count of bytes read
   unsigned short cbAvail;                // Count of bytes available
   API_RET_TYPE   uReturnCode;            // API return code
   struct audit_entry far * fpBuffer;     // Pointer to the data buffer
   struct audit_entry far * fpEntry;      // Single entry in the audit log
   struct ae_servicestat far * fpService; // Service status structure

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
//  NetAuditRead
//
//  This API reads and displays the audit log for the specified server.
//  If the entry is for Service Status Code or Text Changed, print
//  the service and computername.
//========================================================================

   /*
    * Allocate a large buffer space to handle large audit logs.
    * The largest buffer allowed is 64K. If the audit log is 
    * larger than the buffer specified, the API returns as many
    * full records as it can and the NERR_Success return code.
    * Subsequent reading starts from the end of the last record
    * read. To read the whole log, reading must continue until 0
    * is returned for the bytes available counter.
    */

   cbBuffer = BIG_BUFFER;                 // Can be up to 64K

   /*
    * Set the log handle for reading from the start of the audit log.
    * This handle is modified by the API and any subsequent reading
    * of unread data should use the returned handle.
    */

   hLogHandle.time = 0L;
   hLogHandle.last_flags = 0L;
   hLogHandle.offset = 0xFFFFFFFF;
   hLogHandle.last_flags = 0xFFFFFFFF;

   fpBuffer = _fmalloc(cbBuffer);         // Allocate memory for buffer

   if (fpBuffer == NULL)
   {
      fprintf(stderr, "Malloc failed -- out of memory.\n");
      exit(1);
   }
   uReturnCode = NetAuditRead(
                     pszServer,               // Servername
                     NULL,                    // Reserved; must be NULL
                     (HLOG far *)&hLogHandle, // Audit log handle
                     0L,                      // Start at record 0
                     NULL,                    // Reserved; must be NULL
                     0L,                      // Reserved; must be 0
                     0L,                      // Read the log forward
                     (char far *)fpBuffer,    // Data returned here
                     cbBuffer,                // Size of buffer, in bytes
                     &cbRead,                 // Count of bytes read
                     &cbAvail);               // Count of available bytes

   printf("NetAuditRead returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      for ( fpEntry = fpBuffer;
            fpEntry < (struct audit_entry far *)
                            ((char far *)fpBuffer + cbRead); )
      {
         tTime = (time_t) fpEntry->ae_time;

         printf("   Type %d, at %s", fpEntry->ae_type, 
                        asctime( gmtime ((const time_t *) &tTime) ) );

         /*
          * If the entry is for Service Status Code or Text Changed,
          * print the service and computername. This demonstrates how
          * to extract information using the offsets to the log.
          */

         if (fpEntry->ae_type == AE_SERVICESTAT)
         {
            fpService = (struct ae_servicestat far *)
                     ((char far *)fpEntry + fpEntry->ae_data_offset);
            printf("\tComputer = %Fs\n",
                     (char far *)fpService+fpService->ae_ss_compname); 
            printf("\tService  = %Fs\n",
                     (char far *)fpService+fpService->ae_ss_svcname); 
         }
         fpEntry = (struct audit_entry far *) 
                     ((char far *)fpEntry + fpEntry->ae_len);
      }

      printf("\nBytes Read = 0x%X\n", cbRead);

      // To read the whole log, keep reading until cbAvail is 0.

      if (cbAvail)
         printf("Data still unread.\n\n");
      else
         printf("All data read.\n\n");
   }

//========================================================================
//  NetAuditClear
//
//  This API clears the audit log for the specified server. A backup 
//  will be kept in the file specified by pszBackup. If a null pointer 
//  is supplied, no backup is kept.
//========================================================================

   uReturnCode = NetAuditClear(
                     pszServer,           // Servername
                     pszBackup,           // Name of backup file
                     NULL);               // Reserved; must be NULL

   printf("NetAuditClear returned %u\n", uReturnCode);
   printf("   backup file = %s \n", pszBackup);

//========================================================================
//  NetAuditWrite
//
//  This API writes back the entries read by the NetAuditRead call.
//  Each entry read consisted of a fixed-length header, a variable-
//  length data section, and a terminating size indicator. Only
//  the variable-length data area is supplied in the NetAuditWrite
//  buffer. Note: For any entries to be written to the audit log, the
//  Server service must be started with auditing enabled.
//========================================================================

   for ( fpEntry = fpBuffer;
         fpEntry < (struct audit_entry far *)
                         ((char far *)fpBuffer + cbRead); )
   {
      uReturnCode = NetAuditWrite(
                        fpEntry->ae_type,      // Entry type
                                               // Buffer address
                        (char far *)fpEntry + fpEntry->ae_data_offset,
                                               // Buffer length
                        fpEntry->ae_len - fpEntry->ae_data_offset 
                           - sizeof(unsigned short),
                        NULL,                  // Reserved; must be NULL
                        NULL);                 // Reserved; must be NULL

      if (uReturnCode != NERR_Success)
      {
         printf("NetAuditWrite returned %u", uReturnCode);
         break;
      };

      fpEntry = (struct audit_entry far *) 
                         ((char far *)fpEntry + fpEntry->ae_len);
   }

   _ffree(fpBuffer);
   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-b backup]\n", pszProgram);
   exit(1);
}
