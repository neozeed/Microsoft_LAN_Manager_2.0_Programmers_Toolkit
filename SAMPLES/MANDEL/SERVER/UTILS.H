/****************************************************************************

    UTILS.H -- Function definitions for UTILS.C

    Copyright (C) 1990 Microsoft Corporation.

****************************************************************************/


extern PVOID NewFarSeg( USHORT );
extern PVOID NewStackSeg(USHORT);
extern void FreeFarSeg(PVOID);

