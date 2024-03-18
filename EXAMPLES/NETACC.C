/*
   NETACC.C -- a sample program demonstrating NetAccess API functions.

   This program requires that you have admin privilege if a servername
   parameter is supplied.

      usage: netacc [-s \\server] [-u username] [-r resource] [-b basepath]
         where \\server = Name of the server. A servername must be preceded by
                          two backslashes (\\).
               username = Name of the user.
               resource = Name of the resource.
               basepath = Enumerate resources with this base path.

   API                  Used to...
   ================     ================================================
   NetAccessEnum        Display a list of access permissions
   NetAccessCheck       Check a user's write access to the resource
   NetAccessGetInfo     Check that an ACL exists for the resource
   NetAccessAdd         Add an ACL for the resource if one does not exist
   NetAccessSetInfo     Give all users read/write access to the resource
   NetAccessDel         Delete the ACL for the resource

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_BASE
#include    <os2.h>        // MS OS/2 base header files

#define     INCL_NETACCESS
#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"    // Internal routine header file

#define DEFAULT_USER       "ADMIN"
#define DEFAULT_RESOURCE   "\\pipe\\restrict"
#define BIG_BUFFER         32768

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   API_RET_TYPE uReturnCode;              // API return code
   char * pszServer = NULL;               // Servername
   char * pszUserName = DEFAULT_USER;     // Username
   char * pszResource = DEFAULT_RESOURCE; // Resource to check
   char * pszBasePath = NULL;             // No default base path
   char * pbBuffer;                       // Pointer to data buffer
   struct access_info_1 *pAclBuf;         // Pointer to ACL info
   struct access_list   *pAceInfo;        // Pointer to ACE info
   int            iArgv;                  // Index for arguments
   short          iACE;                   // Index for ACEs
   unsigned short iACL;                   // Index for ACLs
   unsigned short cAclRead;               // Count of ACLs read
   unsigned short cAclTotal;              // Count of ACLs available
   unsigned short cbAvail;                // Count of bytes available
   unsigned short cbBuffer;               // Size of buffer, in bytes
   unsigned short usResult;               // Result of access check
   unsigned short fAttrib;                // Auditing attribute flag
   char cGroup;
   BOOL fResourceFound = FALSE;
                                       
   for (iArgv = 1; iArgv < argc; iArgv++)
   {
      if ((*argv[iArgv] == '-') || (*argv[iArgv] == '/'))
      {
         switch (tolower(*(argv[iArgv]+1)))    // Process switches
         {
            case 's':                          // -s server
               pszServer = argv[++iArgv];
               break;
            case 'u':                          // -u username
               pszUserName = argv[++iArgv];
               break;
            case 'r':                          // -r resource
               pszResource = argv[++iArgv];
               break;
            case 'b':                          // -b base path
               pszBasePath = argv[++iArgv];
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
// NetAccessEnum
//
// This API displays the list of resource access permissions.
// Note: Details other than resource name are available only at info
// level 1. If all resources are wanted, then no base pathname should
// be supplied. Recursive searching is enabled.
//========================================================================

   cbBuffer = BIG_BUFFER;                 // Can be up to 64K

   /*
    * You can determine the number of entries by calling NetAccessEnum
    * with pbBuffer=NULL and cbBuffer=0. The size of the buffer needed
    * to enumerate the users is, however, larger than
    * cAclTotal * sizeof(struct access_info_1) because of the
    * variable-length data (referenced by pointers in the structure).
    * It is easiest to use a big buffer.
    */

   pbBuffer = SafeMalloc(cbBuffer);       // Start of memory block

   uReturnCode = NetAccessEnum(
                     pszServer,           // Servername
                     pszBasePath,         // Base path to search from
                     1,                   // Nonzero, recursive search
                     1,                   // Information level
                     pbBuffer,            // Data returned here
                     cbBuffer,            // Size of buffer, in bytes
                     &cAclRead,           // Count of ACLs read 
                     &cAclTotal);         // Count of ACLs available 

   printf("NetAccessEnum returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      pAclBuf = (struct access_info_1 *) pbBuffer;
      for (iACL = 0; iACL < cAclRead; iACL++)
      {
         printf("   resource = %Fs\n", pAclBuf->acc1_resource_name);
         pAceInfo = (struct access_list *)(pAclBuf + 1);
         for (iACE = 0; iACE < pAclBuf->acc1_count; iACE++)
         {
                                          // Print a * before groupnames
            cGroup = ' ';
            if (pAceInfo->acl_access & ACCESS_GROUP)
            {                             // Strip group access bit
               pAceInfo->acl_access &= ACCESS_ALL;
               cGroup = '*';
            }
            printf("\t%c%s:%x \n", cGroup, pAceInfo->acl_ugname,
                        pAceInfo->acl_access);
            pAceInfo++;
         }
         pAclBuf = (struct access_info_1 *)pAceInfo;
      }
      printf("%hu out of %hu entries returned\n", cAclRead, cAclTotal);
   }

//========================================================================
// NetAccessCheck
//
// This API checks that a user has permission to write to a resource.
// It is unique among the Access API functions; it is the only
// one that cannot have a groupname in the name field.
// Note: NetAccessCheck cannot be executed remotely.
//========================================================================


   uReturnCode = NetAccessCheck(
                     NULL,                // Reserved field
                     pszUserName,         // Check for user
                     pszResource,         // Resource to check
                     ACCESS_WRITE,        // Check for write permission
                     &usResult);

   /*
    * The result field is valid only if the API returns NERR_Success.
    * If there is no ACL for the resource, its parent directory, or
    * the drive label, the API sets the result field to NERR_Success
    * for an admin class user and ERROR_ACCESS_DENIED for any other
    * type of user.
    */

   printf("\nNetAccessCheck for write permission returned %u\n",
                 uReturnCode);
   printf("   User name = %s,  Resource = %s\n",
                 pszUserName, pszResource);

   if (uReturnCode == NERR_Success)
   {
      printf("   Result field returned = %hu\n", usResult);
   }

//========================================================================
// NetAccessGetInfo
//
// Call NetAccessGetInfo at level 0 to see if the resource exists.
//========================================================================

   uReturnCode = NetAccessGetInfo(
                     pszServer,           // Servername 
                     pszResource,         // Pointer to resource name
                     0,                   // Only want to know if it exists
                     pbBuffer,            // Data returned here
                     cbBuffer,            // Size of buffer, in bytes
                     &cbAvail);           // Count of bytes available

   printf("\nNetAccessGetInfo of %s returned %u\n",
                  pszResource, uReturnCode);

   switch (uReturnCode)
   {
      case NERR_Success:
         printf("    Resource %s exists\n\n", pszResource);
         fResourceFound = TRUE;
         break;
      case NERR_ResourceNotFound:
         printf("    Resource %s does not exist\n\n", pszResource);
         break;
   }

//========================================================================
// NetAccessAdd
//
// This API adds an ACL for the resource (if one does not exist)
// and sets the attribute flag so all access attempts are audited.
//========================================================================

   if (!fResourceFound)
   {                                      // Set up access permission info
      pAclBuf = (struct access_info_1 *) pbBuffer;

      pAclBuf->acc1_resource_name = (char far *) pszResource;
      pAclBuf->acc1_attr = 1;             // Audit all access attempts
      pAclBuf->acc1_count = 0;            // Set no user/group info

      uReturnCode = NetAccessAdd(
                        pszServer,            // Servername 
                        1,                    // Info level; must be 1
                        (char far *)pAclBuf,  // Data to be set
                        cbBuffer);            // Size of buffer, in bytes

      printf("NetAccessAdd of %s returned %u\n\n",
                     pszResource, uReturnCode);
   }

//========================================================================
// NetAccessSetInfo
//
// This API changes the access record for the resource so that any 
// user who has user permission has read permission only. This can
// be overwritten by setting special access permissions for 
// individual usernames.
//========================================================================

   /*
    * There are two ways to call NetAccessSetInfo. 
    * If sParmNum == PARMNUM_ALL, you must pass it a whole structure.  
    * Otherwise, you can set sParmNum to the element of the structure you 
    * want to change. Both ways are shown here.
    */

                                          // Set up access permission info
   pAclBuf = (struct access_info_1 *) pbBuffer;
   pAclBuf->acc1_resource_name = (char far *) pszResource;
   pAclBuf->acc1_attr = 1;                // Audit all access attempts
   pAclBuf->acc1_count = 1;
                                          // Set up access_list structure
   pAceInfo = (struct access_list *)(pAclBuf+1),
   strcpy(pAceInfo->acl_ugname, "USERS");
   pAceInfo->acl_access = ACCESS_READ;

   uReturnCode = NetAccessSetInfo(
                     pszServer,           // Servername 
                     pszResource,         // Pointer to resource name
                     1,                   // Info level; must be 1
                     (char far *)pAclBuf, // Data to be set
                     cbBuffer,            // Size of buffer, in bytes
                     PARMNUM_ALL);        // Supply full set of info

   printf("NetAccessSetInfo with full buffer returned %u\n", uReturnCode);

   /*
    * Sets sParmNum to ACCESS_ATTR_PARMNUM to change the attribute field
    * of the access_info_1 structure. To specify no audit logging, set
    * acc1_attr to 0.
    */

   fAttrib = 0;

   uReturnCode = NetAccessSetInfo(
                     pszServer,             // Servername 
                     pszResource,           // Pointer to resource name
                     1,                     // Info level
                     (char far *)&fAttrib,  // Turn off all auditing
                     sizeof(short),         // Size of buffer, in bytes
                     ACCESS_ATTR_PARMNUM);  // Give audit attribute only

   printf("NetAccessSetInfo with audit bit only returned %u\n\n",
                    uReturnCode);

//========================================================================
// NetAccessDel
//
// This API deletes the access record for the resource.
//========================================================================

   uReturnCode = NetAccessDel(
                     pszServer,           // Servername 
                     pszResource);        // Pointer to resource name

   printf("NetAccessDel of %s returned %u\n", pszResource, uReturnCode);

   free(pbBuffer);
   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server] [-u username]"\
                   " [-r resource] [-b basepath]\n", pszProgram);
   exit(1);
}
