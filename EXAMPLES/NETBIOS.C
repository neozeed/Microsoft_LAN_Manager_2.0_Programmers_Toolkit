/*
   NETBIOS.C -- a sample program demonstrating NetBIOS API functions.

   This program requires that you have admin privilege on the specified
   server.

      usage: netbios [-s \\server] [-n netname] [-l level]
                     [-d device] [-o open option] [-c command]
      where \\server    = Name of the server. A servername must be
                          preceded by two backslashes (\\).
            netname     = Name of the network.
            level       = Level of detail.
            device      = Logical network name, such as net1.
            open option = Flag for NetBiosOpen.
            command     = Command code for NCB.

   API                 Used to...
   ===============     =============================================
   NetBiosEnum         List network names
   NetBiosGetInfo      Get information about each network
   NetBiosOpen         Open a handle to a logical network
   NetBiosSubmit       Submit an NCB to the network
   NetBiosClose        Close specified files on the specified server

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied,
   as to its usability in any given situation.
*/

#define     INCL_NETBIOS
#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"    // Internal routine header file

#define DEFAULT_BUFFER_SIZE        1024
#define DEFAULT_DEV                "NET1"
#define NCB_CHAIN_SINGLE           0
#define NCB_CHAIN_ERROR_RETRY      1
#define NCB_CHAIN_PROCEED_ON_ERROR 2
#define NCB_CHAIN_STOP_ON_ERROR    3

void Usage(char *pszString);

void main(int argc, char *argv[])
{
   char * pszServer = "";              // Servername
   char * pszNetName = "";             // Network name
   char * pszDevName = DEFAULT_DEV;    // Devicename
   int    iCount;                      // Index counter
   unsigned short hDevName;            // Handle to logical network
   unsigned char  cbCommand = NCBCALL| ASYNCH; // Command to submit
   unsigned short usOpenOpt = NB_REGULAR; // Access mode
   unsigned short sLevel = 1;          // Level
   unsigned short cbBuffer;            // Count of bytes
   unsigned short cEntriesRead;        // Count of entries read
   unsigned short cTotalAvail;         // Count of entries or bytes
   API_RET_TYPE   uRet;                // API function return code
   struct netbios_info_0 *p0;          // Pointer to level 0 data
   struct netbios_info_1 *p1;          // Pointer to level 1 data
   char *pbBuffer;                     // Return buffer
   NCB ncbblock;                       // NCB block


   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 's':                        // -s servername
               pszServer = argv[++iCount];
               break;
            case 'n':                        // -n network name
               pszNetName = argv[++iCount];
               break;
            case 'l':                        // -l level
               sLevel = atoi(argv[++iCount]);
               break;
            case 'd':                        // -d device
               pszDevName = argv[++iCount];
               break;
            case 'o':                        // -o open option
               usOpenOpt = atoi(argv[++iCount]);
               break;
            case 'c':                        // -c command for submit
               cbCommand = (unsigned char) atoi(argv[++iCount]);
               break;
            case 'h':
            default:
               Usage(argv[0]);
               break;
         }
      }
      else
         Usage(argv[0]);
   }

//========================================================================
//  NetBiosEnum
//
//  This API lists all logical networks on the computer.
//========================================================================

   cbBuffer = DEFAULT_BUFFER_SIZE;
   pbBuffer = SafeMalloc(cbBuffer);
   uRet = NetBiosEnum(pszServer,       // Null ptr or string means local
                      sLevel,          // Level (0 or 1)
                      pbBuffer,        // Return buffer
                      cbBuffer,        // Size of return buffer 
                      &cEntriesRead,   // Count of entries returned
                      &cTotalAvail);   // Count of bytes available

   printf("NetBiosEnum returned %u\n", uRet);
   if (uRet == NERR_Success)
   {
      for (iCount = 0; iCount < (int) cEntriesRead; iCount++)
      {
         if (sLevel == 0)
         {
            p0 = (struct netbios_info_0 *)  // Pointer to next entry
                    ( pbBuffer + iCount * sizeof(struct netbios_info_0));
            printf("Network name = %s\n", p0->nb0_net_name);
         }
         else  // Assume sLevel == 1
         {
            p1 = (struct netbios_info_1 *)  // Pointer to next entry
                    (pbBuffer + iCount * sizeof(struct netbios_info_1));
            printf("   Network name: %s\n", p1->nb1_net_name);
            printf("   Driver name : %s\n", p1->nb1_driver_name);
            printf("   Adapter card: %c\n", p1->nb1_lana_num);
            printf("   Driver type : %hu\n", p1->nb1_driver_type);
            printf("   Status      : %hu\n", p1->nb1_net_status);
            printf("   Bandwidth   : %lu\n", p1->nb1_net_bandwidth);
            printf("   Max sessions: %hu\n", p1->nb1_max_sess);
            printf("   Max ncbs    : %hu\n", p1->nb1_max_ncbs);
            printf("   Max names   : %hu\n", p1->nb1_max_names);
         }
      }
      printf("%hu out of %hu entries read\n", cEntriesRead, cTotalAvail);
   }
   free(pbBuffer);

//========================================================================
// NetBiosGetInfo
//
// This API gets information about the specified network name.
//========================================================================

   uRet = NetBiosGetInfo(pszServer,    // NULL means local
              pszNetName,              // Network name
              sLevel,                  // Level (0 or 1)
              NULL,                    // Return buffer
              0,                       // Size of return buffer 
              &cTotalAvail);           // Count of bytes available 

   cbBuffer = cTotalAvail;
   pbBuffer = SafeMalloc(cbBuffer);

   uRet = NetBiosGetInfo(pszServer,    // NULL means local
              pszNetName,              // Network name
              sLevel,                  // Level (0 or 1)
              pbBuffer,                // Return buffer
              cbBuffer,                // Size of return buffer 
              &cTotalAvail);           // Count of bytes available 

   printf("NetBiosGetInfo returned %u\n", uRet);
   if (uRet == NERR_Success)
   {
      if (sLevel == 0)
      {
         p0 = (struct netbios_info_0 *) pbBuffer;
         printf("   Network name = %s\n", p0->nb0_net_name);
      }
      else  // Assume sLevel == 1
      {
         p1 = (struct netbios_info_1 *) pbBuffer;
         printf("   Network name: %s\n", p1->nb1_net_name);
         printf("   Driver name : %s\n", p1->nb1_driver_name);
         printf("   Adapter card: %c\n", p1->nb1_lana_num);
         printf("   Driver type : %hu\n", p1->nb1_driver_type);
         printf("   Status      : %hu\n", p1->nb1_net_status);
         printf("   Bandwidth   : %lu\n", p1->nb1_net_bandwidth);
         printf("   Max sessions: %hu\n", p1->nb1_max_sess);
         printf("   Max ncbs    : %hu\n", p1->nb1_max_ncbs);
         printf("   Max names   : %hu\n", p1->nb1_max_names);
      }
   }
   free(pbBuffer);

//========================================================================
//  NetBiosOpen, NetBiosSubmit, NetBiosClose
//
//  These APIs open a handle to a particular logical network,
//  submit an NCB, and then close the handle.
//========================================================================

   uRet = NetBiosOpen(pszDevName,      // Devicename
                      0,               // Reserved; must be 0
                      usOpenOpt,       // Access mode
                      &hDevName);      // Pointer to handle

   printf("NetBiosOpen returned %u\n", uRet);
   if (uRet == NERR_Success)
   {
      ncbblock.ncb_command = cbCommand;

      uRet = NetBiosSubmit(hDevName,   // Handle for device
                           NCB_CHAIN_SINGLE, // Chaining option
                           &ncbblock); // Pointer to NCB 

      printf("NetBiosSubmit returned %u\n", uRet);
      printf("   Return code = 0x%0x\n", ncbblock.ncb_retcode);

      uRet = NetBiosClose(hDevName,    // Handle for device
                          0);          // Reserved; must be 0

      printf("NetBiosClose returned %u\n", uRet);
   }
  exit(0);
}

void Usage(char *pszString)
{
   fprintf(stderr, "Usage:  %s [-s \\\\server]", pszString);
   fprintf(stderr, " [-n netname] [-l level]\n");
   fprintf(stderr, "\t\t[-d device] [-o open option] [-c command]\n");
   exit(1);
}
