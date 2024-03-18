/*
   PERROR.C -- Phone switching program error handling routines
 
   Copyright (C) 1990 Microsoft Corporation 

   This file contains the functions used by PBX.EXE to write errors
   to the LAN Manager error log.

   Both ErrorRpt and ErrorExit write error messages to the LAN Manager
   error log.  ErrorRpt will return to the caller after recording the
   error and should be used when the error may be ignored.  ErrorExit
   will cause PBX.EXE to shutdown and should only be called for
   terminal errors.

   This code example is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

// Includes
#define   INCL_DOS
#define   INCL_DOSERRORS
#include  <os2.h>

#define   INCL_NETERRORLOG
#define   INCL_NETSERVICE
#define   INCL_NETERRORS
#include  <lan.h>

#include  <stdio.h>
#include  <string.h>

#include  "pbx.h"

// Function prototypes
void  AddStr(char **ppbBuf, char *pszStr);


/* AddStr ------------------------------------------------------------------
  Description:  Add a string into a buffer of strings.
  Input      :  ppbBuf -- Pointer to buffer pointer
                pszStr -- Pointer to string to add
  Output     :  None
--------------------------------------------------------------------------*/

void AddStr(char **ppbBuf, char *pszStr)
{
  while (*(*ppbBuf)++ = *pszStr++);
  return;
}


/* ErrorRpt ----------------------------------------------------------------
  Description:  Write an error to the LAN Manager error log.
  Input      :  pszMsg ---- Error message
                pszFile --- File name (e.g., PBX.C, ROUTER.C)
                usLine ---- Line number of error report call
                usRC   ---- Error return code (if any)
  Output     :  None
--------------------------------------------------------------------------*/

void ErrorRpt(char *pszMsg, char *pszFile, USHORT usLine, USHORT usRC)
{
  char    cTmp[80];                       // Temporary buffer
  char    cArgs[255];                     // Error strings
  char    *pcBuf;                         // Pointer to string buffer
  USHORT  usRetCode;                      // Return code

  // Build the error message
  pcBuf = cArgs;
  AddStr(&pcBuf, "PBX Error");
  sprintf(cTmp, "File: %s",    pszFile);
  AddStr(&pcBuf, (char *)cTmp);
  sprintf(cTmp, "Line: %u",    usLine);
  AddStr(&pcBuf, (char *)cTmp);
  sprintf(cTmp, "Message: %s", pszMsg);
  AddStr(&pcBuf, (char *)cTmp);
  sprintf(cTmp, "RetCode: %u", usRC);
  AddStr(&pcBuf, (char *)cTmp);
  AddStr(&pcBuf, " ");
  AddStr(&pcBuf, " ");
  AddStr(&pcBuf, " ");
  AddStr(&pcBuf, " ");

  // Write error to log file
  usRetCode = NetErrorLogWrite(NULL,                  // Always NULL
                               NELOG_OEM_Code,        // Internal error
                               "PBX Service",         // Component
                               NULL,                  // No Raw data
                               0,
                               cArgs,                 // Error messages
                               9,                     // Nine strings
                               NULL);                 // Always NULL

  return;
}


/* ErrorExit ---------------------------------------------------------------
  Description:  Write an error to the LAN Manager error log.
  Input      :  pszMsg ---- Error message
                pszFile --- File name (e.g., PBX.C, ROUTER.C)
                usLine ---- Line number of error report call
                usRC   ---- Error return code (if any)
  Output     :  None
--------------------------------------------------------------------------*/

void ErrorExit(char *pszMsg, char *pszFile, USHORT usLine, USHORT usRC)
{
  char    cTmp[80];                       // Temporary buffer
  char    cArgs[255];                     // Error strings
  char    *pcBuf;                         // Pointer to string buffer
  USHORT  usRetCode;                      // Return code

  // Build the error message
  pcBuf = cArgs;
  AddStr(&pcBuf, "PBX Terminating Error");
  sprintf(cTmp, "File: %s",    pszFile);
  AddStr(&pcBuf, (char *)cTmp);
  sprintf(cTmp, "Line: %u",    usLine);
  AddStr(&pcBuf, (char *)cTmp);
  sprintf(cTmp, "Message: %s", pszMsg);
  AddStr(&pcBuf, (char *)cTmp);
  sprintf(cTmp, "RetCode: %u", usRC);
  AddStr(&pcBuf, (char *)cTmp);
  AddStr(&pcBuf, " ");
  AddStr(&pcBuf, " ");
  AddStr(&pcBuf, " ");
  AddStr(&pcBuf, " ");

  // Write error to log file
  usRetCode = NetErrorLogWrite(NULL,                  // Always NULL
                               NELOG_OEM_Code,        // Internal error
                               "PBX Service",         // Component
                               NULL,                  // No Raw data
                               0,
                               cArgs,                 // Error messages
                               9,                     // Nine strings
                               NULL);                 // Always NULL

  // Initiate shutdown
  ExitHandler(ERRORSTATUS);

  // Shutdown service
  DosExit(EXIT_PROCESS, 0);

  return;
}
