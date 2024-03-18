/*
   NETWKSTA.C -- a sample program demonstrating NetWksta API functions.

   This program requires that you have admin privileges on the
   specified server.

   usage: netwksta [-s \\server] [-l level] [-d domain] [-u username]
                   [-p password] [-f logoff force]
   where  \\server     = Name of the server. A servername must be 
                         preceded by two backslashes (\\).
          level        = Level of detail to be provided/supplied.
          domain       = Logon domain.
          username     = Name of the logon user.
          password     = Password for the logon user.
          logoff force = Level of logoff force.

   API                 Used to...
   ===============     ==============================================
   NetWkstaGetInfo     Return information about the configuration
                       components for a workstation.
   NetWkstaSetInfo     Change configuration of currently executing
                       workstation. This example doubles the values
                       for charcount and chartime each time it is run.
   NetWkstaSetUID2     Log on or log off the user.

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied,
   as to its usability in any given situation.
*/

#define  INCL_BASE
#include <os2.h>            // MS OS/2 base header files

#define  INCL_NETWKSTA
#define  INCL_NETERRORS
#include <lan.h>            // LAN Manager header files

#include <stdio.h>          // C run-time header files
#include <stdlib.h>
#include <string.h>

#include "samples.h"        // Internal routine header files

//  Define function prototypes
void ProcessLogonCode(struct user_logon_info_1 *p1);
void ProcessLogoffCode(struct user_logoff_info_1 *p1);
void ProcessAccessDenied(struct user_logon_info_1 *p1);
void Usage(char * pszString);

void main(int argc, char * argv[])
{
   char * pbBuffer;                     // Pointer to data buffer
   char * pszDomainName = "";           // Default to local machine
   char * pszServer = "";               // Default to local machine
   char * pszUserName = "";             // Default to null username
   char * pszPassword = "";             // Default to null password
   int    iCount;                       // Index counter
   short  sLevel = 0;                   // Level of info in pbBuffer
   unsigned short cbBuflen;             // Count of bytes
   unsigned short cbTotalAvail;         // Count of bytes available
   unsigned short usLogoffForce = 0;    // Level of force for logoff
   API_RET_TYPE   uRetCode;             // API return code
   struct wksta_info_0 *p0;             // Workstation info; level 0
   struct wksta_info_1 *p1;             // Workstation info; level 1
   struct wksta_info_10 *p10;           // Workstation info; level 10

   for (iCount = 1; iCount < argc; iCount++) // Get command-line switches
   {
      if ((*argv[iCount] == '-') || (*argv[iCount] == '/'))
      {
         switch (tolower(*(argv[iCount]+1))) // Process switches
         {
            case 'd':                        // -d domain name
               pszDomainName = argv[++iCount];
               break;
            case 's':                        // -s servername
               pszServer = argv[++iCount];
               break;
            case 'u':                        // -u username
               pszUserName = argv[++iCount];
               break;
            case 'p':                        // -p password
               pszPassword = argv[++iCount];
               break;
            case 'l':                        // -l level
               sLevel = atoi(argv[++iCount]);
               break;
            case 'f':                        // -f logoff force
               usLogoffForce = atoi(argv[++iCount]); 
               break;
            case 'h':
            default:
               Usage(argv[0]);
         }
      }
      else
         Usage(argv[0]);
   }
   printf("\nWorkstation Category API Examples\n");

//========================================================================
//  NetWkstaGetInfo
//
//  This API returns information about the workstation configuration.
//  The data reflects all changes made by NetWkstaSetInfo, but may
//  not be the same as the default values as set in the LANMAN.INI file. 
//  For LANMAN.INI parameter values, use NetConfigGet2.
//========================================================================

/*
 * NetWkstaGetInfo returns both fixed-length and variable-length data.
 * The size of the data buffer passed to the API function must be 
 * larger than the size of the structure. The extra space is needed
 * for variable-length strings, such as computername and username. 
 * Call NetWkstaGetInfo the first time with a zero-length buffer, 
 * and the API function returns the buffer size needed.
 */
   uRetCode = NetWkstaGetInfo(pszServer,    // Servername
                           sLevel,          // Reporting level (0,1,10)
                           NULL,            // Target buffer for info
                           0,               // Size of target buffer
                           &cbBuflen);      // Count of bytes available

   if (uRetCode == NERR_BufTooSmall)
      pbBuffer = SafeMalloc(cbBuflen);
   else
   {
      printf("NetWkstaGetInfo returned %u\n", uRetCode);
      exit(1);
   }

   uRetCode = NetWkstaGetInfo(pszServer,    // Servername
                           sLevel,          // Reporting level (0,1,10)
                           pbBuffer,        // Target buffer for info
                           cbBuflen,        // Size of target buffer
                           &cbTotalAvail);  // Total info, in bytes

   printf("NetWkstaGetInfo returned %u\n", uRetCode);
   if (uRetCode != NERR_Success)
      exit(1);

   if ((sLevel == 0) || (sLevel == 1))  // Display common elements
   {
      p0 = (struct wksta_info_0 *) pbBuffer;
      printf("    Computer Name   : %Fs\n", p0->wki0_computername);
      printf("    User Name       : %Fs\n", p0->wki0_username);
      printf("    Lan Group       : %Fs\n", p0->wki0_langroup);
      printf("    Logon Server    : %Fs\n", p0->wki0_logon_server);
      printf("    Char Time       : %lu\n", p0->wki0_chartime);
      printf("    Char Count      : %hu\n", p0->wki0_charcount);
   }
   if (sLevel == 1)
   {
      p1 = (struct wksta_info_1 *) pbBuffer;
      printf("    Logon Domain    : %s\n", p1->wki1_logon_domain);
      printf("    Other Domains   : %s\n", p1->wki1_oth_domains);
      printf("    Datagram Buffers: %hu\n", p1->wki1_numdgrambuf);
   }
   if (sLevel == 10)
   {
      p10 = (struct wksta_info_10 *) pbBuffer;
      printf("    Computer Name   : %Fs\n", p10->wki10_computername);
      printf("    User Name       : %Fs\n", p10->wki10_username);
      printf("    Logon Domain    : %s\n", p10->wki10_logon_domain);
      printf("    Other Domains   : %s\n", p10->wki10_oth_domains);
   }

//========================================================================
//  NetWkstaSetInfo
//
//  This API sets configuration components for the specified LAN Manager
//  workstation. This API does not modify the LANMAN.INI file. The call 
//  to NetWkstaSetInfo can reset all parameters or only one parameter. 
//  To reset all parameters, first call NetWkstaGetInfo, modify the 
//  desired components, then use NetWkstaSetInfo. If you want to change 
//  only one parameter, supply the variable that will change, and then 
//  call NetWkstaSetInfo with the corresponding parameter number code.
//========================================================================

   // Previous call to NetWkstaGetInfo assures valid data in structure. 

   p0 = (struct wksta_info_0 *) pbBuffer;

   //  Double the chartime and charcount values
   p0->wki0_chartime *= 2;
   p0->wki0_charcount *= 2;

   uRetCode = NetWkstaSetInfo(pszServer,    // Servername
                         sLevel,            // Reporting level
                         pbBuffer,          // Source buffer for info
                         cbBuflen,          // Size of source buffer
                         PARMNUM_ALL);      // Parameter number code

   printf("NetWkstaSetInfo returned %u\n", uRetCode);

   if (uRetCode == NERR_Success)
   {
      printf("    Char Time set to  : %lu\n", p0->wki0_chartime);
      printf("    Char Count set to : %hu\n", p0->wki0_charcount);
   }
   free(pbBuffer);

//========================================================================
//  NetWkstaSetUID2
//
//  This API logs a user on to or off from the network. The username 
//  parameter determines which operation to perform; a null username 
//  indicates alog off operation, a non-null username indicates a logon
//  operation.
//========================================================================

   // Make an initial call to determine the required return buffer size.

   uRetCode = NetWkstaSetUID2(NULL,   // Reserved; must be NULL
                    NULL,             // Domain to log on to
                    NULL,             // User to logon or null=logoff
                    NULL,             // User password if logon
                    "",               // Reserved; must be null string
                    0,                // Logoff force
                    1,                // Level; must be 1
                    NULL,             // Logon data returned
                    0,                // Size of data area, in bytes
                    &cbTotalAvail);   // Count of total bytes available

   cbBuflen = cbTotalAvail;
   pbBuffer = SafeMalloc(cbBuflen);

   uRetCode = NetWkstaSetUID2(NULL,   // Reserved; must be NULL
                    pszDomainName,    // Domain to log on to
                    pszUserName,      // User to logon or null=logoff
                    pszPassword,      // User password if logon
                    "",               // Reserved; must be null string
                    usLogoffForce,    // Logoff force
                    1,                // Level; must be 1
                    pbBuffer,         // Logon data returned
                    cbBuflen,         // Size of buffer, in bytes
                    &cbTotalAvail);   // Count of total bytes available
   printf("NetWkstaSetUID2 returned %u\n", uRetCode);

   if ((*pszUserName == '\0') || (pszUserName == NULL))
   {
      printf("Log off using logoff code %hu\n", usLogoffForce);
      switch (uRetCode)
      {
         case NERR_NotLoggedOn:
            printf("Not logged on\n");
            break;
         case NERR_Success:
         case NERR_UnableToDelName_W:
            ProcessLogoffCode((struct user_logoff_info_1 *) pbBuffer);
            break;
         default:
            break;
      } 
   }
   else  // Non-null username indicates a logon operation.
   {
      printf("Log user %s (password %s)",pszUserName, pszPassword);
      if ((*pszDomainName == '\0') || (pszDomainName == NULL))
         printf(" on the workstation's primary domain\n");
      else
         printf(" on domain %s\n", pszDomainName);

      switch (uRetCode)
      {
         case NERR_Success:
         case NERR_UnableToAddName_W:
            ProcessLogonCode((struct user_logon_info_1 *) pbBuffer);
            break;
         case NERR_StandaloneLogon:
            printf("No logon server specified, logged on STANDALONE\n");
            break;
         case NERR_BadUsername:
         case NERR_BadPassword:
            printf("Invalid username or password\n");
            break;
         case NERR_AlreadyLoggedOn:
         case NERR_NotLoggedOn:
            printf("Did not logon user \n");
            break;
         case ERROR_ACCESS_DENIED:
            ProcessAccessDenied((struct user_logon_info_1 *) pbBuffer);
            break;
         default:
            break;
      } 
   }
   free(pbBuffer);
}

//========================================================================
//  Applications that call NetWkstaSetUID2 need to examine two different 
//  return code variables to determine whether structure elements in the 
//  return buffer contain valid data.
//
//  This example program first examines the return code returned by the
//  function, and then calls the functions ProcessLogoffCode,
//  ProcessLogonCode, and ProcessAccessDenied to examine the return code
//  within the user_logon_info_1 or user_logoff_info_1 data structures.
//
//  The combination of values of the function return code and the code
//  present in the return buffer determines which structure elements
//  contain valid data.
//
//  ProcessLogoffCode examines the usrlogf1_code element of the 
//  user_logoff_info_1 data structure when NetWkstaSetUID2, called with a 
//  null username, return NERR_Success or NERR_UnableToDelName_W.
//========================================================================

void ProcessLogoffCode(struct user_logoff_info_1 *p1)
{
   printf("Logoff code = %hu\n", p1->usrlogf1_code);

   switch (p1->usrlogf1_code)
   {
      case NERR_Success:            // All elements valid
         printf("Duration of logon: %lu\n", p1->usrlogf1_duration);
         printf("%hu other logons\n", p1->usrlogf1_num_logons);
         break;
      case NERR_NonValidatedLogon:  // No other valid elements in p1
         printf("Non validated logon\n");
         break;
      case NERR_StandaloneLogon:
         printf("Standalone logon\n");
         break;
      default:
         printf("No valid fields\n");
         break;
   }
   return;
}

//========================================================================
// For calls to NetWkstaSetUID2, two different return code variables 
// are needed to determine whether structure elements in the return
// buffer contain valid data. ProcessLogonCode examines the 
// usrlog1_code element of the user_logon_info_1 data structure when 
// NetWkstaSetUID2, called with a non-null username, returned NERR_Success 
// or NERR_UnableToAddName_W.
//========================================================================

void ProcessLogonCode(struct user_logon_info_1 *p1)
{
   printf("Logon code = %hu\n", p1->usrlog1_code);
   switch (p1->usrlog1_code)
   {
      case NERR_Success:            // All codes valid
         printf("User %s", p1->usrlog1_eff_name);
         printf(" has privilege level %hu\n", p1->usrlog1_priv);
         break;
      case NERR_NonValidatedLogon:  // Computer, script valid
         printf("Used logon script %s", p1->usrlog1_script_path);
         printf(" on %s\n", p1->usrlog1_computer);
         break;
      case NERR_StandaloneLogon:
         printf("Standalone logon\n");
         break;
      default:
         printf("No valid fields\n");
         break;
   }
   return;
}

//========================================================================
//  For calls to NetWkstaSetUID2, two different return code variables 
//  are needed to determine whether structure elements in the return 
//  buffer contain valid data.

//  ProcessAccessDenied examines the usrlog1_code element of the 
//  user_logon_info_1 data structure when NetWkstaSetUID2, called with a 
//  non-null username, returns the return code value ERROR_ACCESS_DENIED.
//========================================================================

void ProcessAccessDenied(struct user_logon_info_1 *p1)
{
   printf("Logon code = %hu\n", p1->usrlog1_code);
   switch (p1->usrlog1_code)
   {
      case NERR_PasswordExpired:
         printf("Password expired\n");
         break;
      case NERR_InvalidWorkstation:
         printf("Invalid workstation\n");
         break;
      case NERR_InvalidLogonHours:
         printf("Invalid logon hours\n");
         break;
      case NERR_LogonScriptError:
         printf("Logon script error\n");
         break;
      case ERROR_ACCESS_DENIED:
         printf("Access denied\n");
         break;
      default:
         printf("No valid fields\n");
         break;
   }
   return;
}

//========================================================================
//  Usage
//  
//  Display possible command line switches for this example.
//========================================================================

void Usage(char * pszString)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-d domain]", pszString);
   fprintf(stderr, " [-l level]\n\t[-u username] [-p password]");
   fprintf(stderr, " [-f logoff force]\n");
   exit(1);
}
