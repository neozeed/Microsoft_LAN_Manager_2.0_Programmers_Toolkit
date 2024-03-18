/****************************************************************************

    CALC.H -- Data structure for complex point storage.

    Copyright (C) 1990 Microsoft Corporation.

****************************************************************************/

/*
 *  A point on the complex plane
 */

typedef struct _cpoint {
    double	real;
    double	imag;
} CPOINT;

typedef CPOINT far * PCPOINT;
typedef CPOINT near * NPCPOINT;


PULONG MandelCalc( PCPOINT, PRECTL, double, ULONG, PUSHORT);
