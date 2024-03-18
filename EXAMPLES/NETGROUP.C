/*
   NETGROUP.C -- a sample program demonstrating NetGroup API functions.

   This program requires that you have admin privilege or accounts
   operator privilege on the specified server.

      usage: netgroup [-s \\server] [-g groupname] [-c comment]

      where  \\server  = Name of the server. A servername must be 
                         preceded by two backslashes (\\).
             groupname = Name of the group to be added.
             comment   = Comment for the group to be added.

   API                  Used to...
   ================     =========================================
   NetGroupAdd          Add a new group
   NetGroupEnum         Display list of groups and group comments
   NetGroupSetInfo      Change a group's comment
   NetGroupGetInfo      Display a particular group's comment
   NetGroupSetUsers     Set the members of a group
   NetGroupAddUser      Add a new member to the group
   NetGroupDelUser      Delete a member from the group
   NetGroupGetUsers     List the members of a group
   NetGroupDel          Delete a group

   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

#define     INCL_NETGROUP
#define     INCL_NETERRORS
#include    <lan.h>        // LAN Manager header files

#include    <stdio.h>      // C run-time header files
#include    <stdlib.h>
#include    <string.h>

#include    "samples.h"    // Internal routine header file

#define DEFAULT_GROUP      "TESTERS"
#define DEFAULT_COMMENT    "Default comment for new group"
#define NEW_COMMENT        "New group comment"
#define TRIAL_USER1        "BRUCE"
#define TRIAL_USER2        "LIZ"
#define TRIAL_USER3        "JOHN"
#define BIG_BUFF           32768

void Usage (char * pszProgram);

void main(int argc, char *argv[])
{
   char *         pszServer = "";
   char *         pszNewGroup = DEFAULT_GROUP;
   char *         pszComment  = DEFAULT_COMMENT;
   char *         pszNewUser1 = TRIAL_USER1;
   char *         pszNewUser2 = TRIAL_USER2;
   char *         pszNewUser3 = TRIAL_USER3;
   char *         pbBuffer;               // Pointer to data buffer
   int            iArgv;                  // Index for arguments
   unsigned short iEntries;               // Index for entries read
   unsigned short cRead;                  // Count read
   unsigned short cAvail;                 // Count available
   unsigned short cbBuffer;               // Size of buffer, in bytes
   API_RET_TYPE   uReturnCode;            // API return code
   struct group_info_1 * pGroupInfo1;     // Pointer to group info
   struct group_users_info_0 * pGroupUsersInfo0;

   for (iArgv = 1; iArgv < argc; iArgv++)
   {
      if ((*argv[iArgv] == '-') || (*argv[iArgv] == '/'))
      {
         switch (tolower(*(argv[iArgv]+1))) // Process switches
         {
            case 's':                       // -s servername
               pszServer = argv[++iArgv];
               break;
            case 'g':                       // -g groupname
               pszNewGroup = argv[++iArgv];
               break;
            case 'c':                       // -c comment
               pszComment = argv[++iArgv];
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
//  NetGroupAdd
//
//  This API adds a new group with info level 1.
//========================================================================

   cbBuffer = sizeof(struct group_info_1);
   pGroupInfo1 = (struct group_info_1 *) SafeMalloc(cbBuffer);
   strcpy(pGroupInfo1->grpi1_name, pszNewGroup);
   pGroupInfo1->grpi1_comment = pszComment;

   uReturnCode = NetGroupAdd(
                     pszServer,              // Servername
                     1,                      // Info level
                     (char far *)pGroupInfo1,// Input buffer
                     cbBuffer);              // Size of buffer 
   free(pGroupInfo1);

   printf("NetGroupAdd of group \"%s\" returned %u\n",
              pszNewGroup, uReturnCode);

//========================================================================
//  NetGroupEnum
//
//  This API displays the list of current groupnames and group comments.
//========================================================================

   cbBuffer = BIG_BUFF;                      // Can be up to 64K
   pbBuffer = SafeMalloc(cbBuffer);          // Data buffer

   uReturnCode = NetGroupEnum(
                     pszServer,              // Servername
                     1,                      // Info level
                     pbBuffer,               // Data returned here
                     cbBuffer,               // Size of buffer, in bytes
                     &cRead,                 // Count of entries read
                     &cAvail);               // Count of entries available

   printf("NetGroupEnum returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      pGroupInfo1 = (struct group_info_1 *) pbBuffer;
      for (iEntries = 0; iEntries < cRead; iEntries++)
      {
         printf("   %-24s - %Fs\n", pGroupInfo1->grpi1_name, 
                   pGroupInfo1->grpi1_comment);
         pGroupInfo1++;
      }
      printf("%hu out of %hu entries returned\n", cRead, cAvail);
   }
   free(pbBuffer);

//========================================================================
//  NetGroupSetInfo
//
//  This API sets information for a group.
//
//  There are two ways to call NetGroupSetInfo. If sParmNum is set to 
//  PARMNUM_ALL, you must pass it a whole structure. Otherwise, you can
//  set sParmNum to the structure element you want to change. The
//  whole structure is used in this example to change the group comment.
//========================================================================

   cbBuffer = sizeof(struct group_info_1) + MAXCOMMENTSZ + 1;
   pbBuffer = SafeMalloc(cbBuffer);          // Info buffer

   pGroupInfo1 = (struct group_info_1 *) pbBuffer;
   strcpy(pGroupInfo1->grpi1_name, pszNewGroup);

   /*
    * Data referenced by a pointer from within the fixed-length
    * portion of the data structure must be located outside the buffer
    * passed to the API or it will be ignored in remote NeGroupSetInfo 
    * calls.
    */

   pGroupInfo1->grpi1_comment = NEW_COMMENT;

   uReturnCode = NetGroupSetInfo(
                     pszServer,              // Servername
                     pszNewGroup,            // Groupname
                     1,                      // Info level
                     pbBuffer,               // Data buffer
                     cbBuffer,               // Size of buffer, in bytes 
                     PARMNUM_ALL);           // Parameter number code

   printf("NetGroupSetInfo returned %u\n", uReturnCode);
   printf("   Changing comment for group \"%s\" %s\n", pszNewGroup,
                     uReturnCode ? "failed" : "succeeded");

//========================================================================
//  NetGroupGetInfo
//
//  This API gets the information available about the new group.
//========================================================================

   uReturnCode = NetGroupGetInfo(
                     pszServer,              // Servername
                     pszNewGroup,            // Groupname
                     1,                      // Info level
                     pbBuffer,               // Data buffer
                     cbBuffer,               // Size of buffer, in bytes
                     &cAvail);               // Count of bytes available

   printf("NetGroupGetInfo returned %u\n", uReturnCode);
   if (uReturnCode == NERR_Success)
   {
      printf("   %-24s - %Fs\n", pGroupInfo1->grpi1_name, 
                     pGroupInfo1->grpi1_comment);
   }
   free(pbBuffer);

//========================================================================
//  NetGroupSetUsers
//
//  This API sets the list of users who belong to the new group.
//========================================================================

   cbBuffer = sizeof(struct group_users_info_0) * 2;
   pbBuffer = SafeMalloc(cbBuffer);          // Info buffer

   pGroupUsersInfo0 = (struct group_users_info_0 *) pbBuffer;
   strcpy(pGroupUsersInfo0->grui0_name, pszNewUser1);
   pGroupUsersInfo0++;
   strcpy(pGroupUsersInfo0->grui0_name, pszNewUser2);

   uReturnCode = NetGroupSetUsers(
                     pszServer,              // Servername
                     pszNewGroup,            // Groupname
                     0,                      // Info level; must be 0
                     pbBuffer,               // Data buffer
                     cbBuffer,               // Size of buffer, in bytes
                     2);                     // Entries

   printf("NetGroupSetUsers returned %u\n", uReturnCode);

//========================================================================
//  NetGroupAddUser
//
//  This API adds a user to the new group.
//========================================================================

   uReturnCode = NetGroupAddUser(
                     pszServer,              // Servername
                     pszNewGroup,            // Groupname
                     pszNewUser3);           // Username to add

   printf("NetGroupAddUser for user \"%s\" returned %u\n",
              pszNewUser3, uReturnCode);

//========================================================================
//  NetGroupDelUser
//
//  This API deletes a user from the new group.
//========================================================================

   uReturnCode = NetGroupDelUser(
                     pszServer,              // Servername
                     pszNewGroup,            // Groupname
                     pszNewUser2);           // Username to delete

   printf("NetGroupDelUser for user \"%s\" returned %u\n",
              pszNewUser2, uReturnCode);

//========================================================================
//  NetGroupGetUsers
//
//  List the users who belong to the new group.
//========================================================================

   uReturnCode = NetGroupGetUsers(
                     pszServer,              // Servername
                     pszNewGroup,            // Groupname
                     0,                      // Info level; must be 0
                     pbBuffer,               // Buffer
                     cbBuffer,               // Size of buffer
                     &cRead,                 // Count of entries read
                     &cAvail);               // Count of entries available

   printf("NetGroupGetUsers returned %u\n", uReturnCode);

   if (uReturnCode == NERR_Success)
   {
      pGroupUsersInfo0 = (struct group_users_info_0 *) pbBuffer;
      for (iEntries = 0; iEntries < cRead; iEntries++)
      {
         printf("   %s\n", pGroupUsersInfo0->grui0_name); 
         pGroupUsersInfo0++;
      }
   }
   free(pbBuffer);

//========================================================================
//  NetGroupDel
//
//  This API deletes the new group.
//========================================================================

   uReturnCode = NetGroupDel(pszServer,      // Servername
                             pszNewGroup);   // Groupname to delete
   free(pGroupInfo1);

   printf("NetGroupDel of group \"%s\" returned %u\n",
              pszNewGroup, uReturnCode);

   exit(0);
}

void Usage (char * pszProgram)
{
   fprintf(stderr, "Usage: %s [-s \\\\server]"\
                   " [-g groupname] [-c comment]\n", pszProgram);
   exit(1);
}

