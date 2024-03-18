/*
   SAMPLES.C -- a file containing common routines for the Programmer's
                Referance sample programs.

   These common routines for the sample programs are intended to be compiled
   into SAMPLES.LIB. They provide a safe memory allocation function and
   three sample string manipulation functions that take far pointers.
*/

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

//========================================================================
//  _SafeMallocFunc()
//
//  This safe version of malloc provides a quick check that a malloc
//  succeeds; it terminates with an error if malloc does not succeed.
//  Production code, especially for a Windows or Presentation Manager
//  environment, should use a more robust version of SafeMallocFunc().
//
//  This function is not meant to be called directly. Instead, programs
//  should call the SafeMalloc macro defined in SAMPLES.H as:
//
//  #define  SafeMalloc(size)  _SafeMallocFunc(size, __FILE__, __LINE__)
//========================================================================

void *_SafeMallocFunc(unsigned cbSize, char *pszFilename, unsigned usLine)
{
   void *ptr;

   if ((ptr = malloc(cbSize)) == NULL)
   {
      fprintf(stderr, "Malloc failed.  size:%u  file:%s  line:%u\n",
              cbSize, pszFilename, usLine);
      exit(1);
   }
   else
      return (ptr);
}

//========================================================================
//  The following string functions use far pointers.
//
//  Since much of the string data returned from LAN Manager API
//  functions uses far pointers, they cannot be easily manipulated using
//  the standard C run-time library calls if your program is in small or
//  medium memory model. By using these far versions and their function
//  prototypes, the sample programs will work in any memory model (near
//  pointers will be promoted to far if necessary.)
//========================================================================

char far * FarStrcpy (char far *pszDestination, char far *pszSource)
{
   char far * pszReturn = pszDestination;

   while (*pszDestination++ = *pszSource++)
           ;                   // Copy source over destination
   return (pszReturn);
}

char far * FarStrcat( char far *pszDestination, char far *pszSource )
{
   char far * pszReturn = pszDestination;

   while (*pszDestination)
           pszDestination++;   // Go to end of destination string
   while (*pszDestination++ = *pszSource++)
           ;                   // Concatenate source to destination
   return (pszReturn);
}

int FarStrcmpi(char far *pszDestination, char far *pszSource)
{
   int f,l;

   do {
      f = tolower(*pszDestination);
      l = tolower(*pszSource);
      pszDestination++;
      pszSource++;
   } while (f && (f == l));

   return (f - l);
}

