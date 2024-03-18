/*--------------------------------------------------------------
   EASYFONT.C -- Routines for Using Image Fonts

   Copyright (C) 1990 Microsoft Corporation 
  
   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, as to
   its usability in any given situation.
  --------------------------------------------------------------*/

#define INCL_GPI
#include <os2.h>

#include <stdlib.h>
#include <string.h>

#include "easyfont.h"

static SHORT sFontSize[6]   = { 80, 100, 120, 140, 180, 240 } ;
static CHAR  *szFacename[4] = { "System Proportional",
                                "Courier", "Helv", "Tms Rmn" } ;
static LONG  alMatch[4][6] ;

/*
 * This routine will Query the current logical font.
 */

BOOL EzfQueryFonts (HPS hps)
{
     FONTMETRICS *pfm ;
     HDC         hdc ;
     LONG        lHorzRes, lVertRes, lRequestFonts, lNumberFonts ;
     SHORT       sIndex, sFace, sSize ;

     hdc = GpiQueryDevice (hps) ;
     DevQueryCaps (hdc, CAPS_HORIZONTAL_FONT_RES, 1L, &lHorzRes) ;
     DevQueryCaps (hdc, CAPS_VERTICAL_FONT_RES,   1L, &lVertRes) ;

     for (sFace = 0 ; sFace < 4 ; sFace++)
     {
          lRequestFonts = 0 ;
          lNumberFonts = GpiQueryFonts (hps, QF_PUBLIC, szFacename[sFace],
                                        &lRequestFonts, 0L, NULL) ;
          if (lNumberFonts == 0)
               continue ;

          if (lNumberFonts * sizeof (FONTMETRICS) >= 65536L)
               return FALSE ;

          pfm = malloc ((SHORT) lNumberFonts * sizeof (FONTMETRICS)) ;

	  if (pfm == NULL)
               return FALSE ;

          GpiQueryFonts (hps, QF_PUBLIC, szFacename[sFace],
                         &lNumberFonts, (LONG) sizeof (FONTMETRICS), pfm) ;

          for (sIndex = 0 ; sIndex < (SHORT) lNumberFonts ; sIndex++)
               if (pfm[sIndex].sXDeviceRes == (SHORT) lHorzRes &&
                   pfm[sIndex].sYDeviceRes == (SHORT) lVertRes &&
                   (pfm[sIndex].fsDefn & 1) == 0)
               {
                    for (sSize = 0 ; sSize < 6 ; sSize++)
                       if (pfm[sIndex].sNominalPointSize == sFontSize[sSize])
                              break ;

                    if (sSize != 6)
                         alMatch[sFace][sSize] = pfm[sIndex].lMatch ;
               }

          free (pfm) ;
     }
     return TRUE ;
}

/*
 * This routine will create a logical font.
 */

LONG EzfCreateLogFont (HPS hps, LONG lcid, USHORT idFace, USHORT idSize,
                                           USHORT fsSelection)
{
     static FATTRS fat ;

     if (idFace > 3 || idSize > 5 || alMatch[idFace][idSize] == 0)
          return FALSE ;

     fat.usRecordLength = sizeof fat ;
     fat.fsSelection    = fsSelection ;
     fat.lMatch         = alMatch[idFace][idSize] ;

     strcpy (fat.szFacename, szFacename[idFace]) ;

     return GpiCreateLogFont (hps, NULL, lcid, &fat) ;
}
