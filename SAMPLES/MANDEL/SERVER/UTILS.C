/****************************************************************************

    UTILS.C --

    Stack manipulation utilities for the Windows Mandelbrot Set distributed
    drawing program.

    Copyright (C) 1990 Microsoft Corporation.

    This code sample is provided for demonstration purposes only.
    Microsoft makes no warranty, either express or implied,
    as to its usability in any given situation.

****************************************************************************/

#define INCL_DOS
#include <os2.h>

#include "utils.h"


PVOID
NewFarSeg(USHORT   size)
{
    SEL     sel;

    if (DosAllocSeg(size, &sel, 0))
	return NULL;

    return MAKEP(sel, 0);
}


PVOID
NewStackSeg(USHORT   size)
{
    SEL     sel;

    if (DosAllocSeg(size, &sel, 0))
	return NULL;

    return MAKEP(sel, size-2);
}


void
FreeFarSeg(PVOID seg)
{
    DosFreeSeg(SELECTOROF(seg));
}
