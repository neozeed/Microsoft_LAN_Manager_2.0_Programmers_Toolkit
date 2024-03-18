/*
   NETCNFG.C -- a sample program demonstrating NetConfig API functions.

   This program requires that you have admin privilege or any
   type of operator privilege on the specified server.

      usage: netcnfg [-s \\server] [-p parameter] [-c component]
         where \\server  = Name of the server. A servername must be
                           preceded by two backslashes (\\).
               parameter = Name of the LANMAN.INI parameter to get.
               component = Name of the component from which the
                           parameter is to be retrieved.

   API                  Used to...
   ================     ===========================================
   NetConfigGet2        Retrieve a parameter in the LANMAN.INI file
                          on the specified server
   NetConfigGetAll2     Retrieve complete component info from the
                          specified LANMAN.INI file

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETCONFIG
#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"    // Internal routine header file

#define DEFAULT_COMPONENT  "Workstation"
#define DEFAULT_PARAMETER  "Domain"
#define SMALL_BUFF         64
#define BIG_BUFF           4096

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   char *pszServer = "";                     // Server name
   char *pszComponent  = DEFAULT_COMPONENT;  // Component to list
   char *pszParameter  = DEFAULT_PARAMETER;  // Parameter within component
   char *pszBuffer, *p;                      // String pointers
   int            iCount;                    // Index counter
   unsigned short cbBuffer;                  // Count of bytes in buffer
   unsigned short cbParmlen;                 // Length of returned parameter
   unsigned short cbRead;                    // Count of bytes read
   unsigned short cbAvail;                   // Count of bytes available
   API_RET_TYPE   uReturnCode;               // API return code

   for (iCount = 1; iCount < argc; iCount++)
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 's':                        // -s servername
               pszServer = argv[++iCount];
               break;
            case 'c':                        // -c component
               pszComponent = argv[++iCount];
               break;
            case 'p':                        // -p parameter
               pszParameter = argv[++iCount];
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
//  NetConfigGet2
//
//  This API retrieves a single entry from the LANMAN.INI file on a
//  specified server. If no servername is given, the local LANMAN.INI
//  file is used. When executed remotely, the user must have admin
//  privilege or at least one operator privilege on the remote server.
//========================================================================

   cbBuffer = SMALL_BUFF;
   pszBuffer = SafeMalloc(cbBuffer);

   uReturnCode = NetConfigGet2(
                           pszServer,     // Servername
                           NULL,          // Reserved; must be NULL
                           pszComponent,  // Component in LANMAN.INI
                           pszParameter,  // Parameter in component
                           pszBuffer,     // Pointer to return buffer
                           cbBuffer,      // Size of buffer
                           &cbParmlen);   // Length of returned parameter

   printf("NetConfigGet2 returned %u \n", uReturnCode);

   if (uReturnCode == NERR_Success)
      printf("   %s = %s\n\n", pszParameter, pszBuffer);
   else if (uReturnCode == NERR_BufTooSmall)
      printf("   %hu bytes were provided, but %hu bytes were needed\n\n",
            cbBuffer, cbParmlen);
   free(pszBuffer);

//========================================================================
//  NetConfigGetAll2
//
//  This API returns information for an entire LANMAN.INI component such
//  as [networks] or [workstation]. The returned information is a
//  sequence of null-terminated strings followed by a null string.
//========================================================================

   cbBuffer = BIG_BUFF;
   pszBuffer = SafeMalloc(cbBuffer);

   uReturnCode = NetConfigGetAll2(
                           pszServer,     // Name of remote server
                           NULL,          // Reserved; must be NULL
                           pszComponent,  // Component of LANMAN.INI
                           pszBuffer,     // Pointer to return buffer
                           cbBuffer,      // Size of buffer
                           &cbRead,       // Count of bytes read
                           &cbAvail);     // Count of bytes available

   printf("NetConfigGetAll2 returned %u \n", uReturnCode);

   switch(uReturnCode)
   {
      case NERR_Success:                  // It worked
         p = pszBuffer;
         while (*p)                       // While not at null string
         {
            printf("   %s\n", p);         // Print string
            p += (strlen(p) + 1);         // Step past trailing NUL
         }
         break;
      case ERROR_BAD_NETPATH:
         printf("   Server %s not found.\n", pszServer);
         break;
      case NERR_InvalidAPI:
         printf("   The remote server %s does not support this API.\n",
                 pszServer);
         break;
      default:
         break;
   }
   free(pszBuffer);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-c component]" \
                   " [-p parameter]\n", pszProgram);
   exit(1);
}
