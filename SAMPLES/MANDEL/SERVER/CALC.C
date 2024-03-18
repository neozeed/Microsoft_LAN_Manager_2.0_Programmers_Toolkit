/****************************************************************************

    CALC.C --

    Code to do the calculations for the Windows Mandelbrot Set distributed
    drawing program.

    Copyright (C) 1990 Microsoft Corporation.

    This code sample is provided for demonstration purposes only.
    Microsoft makes no warranty, either express or implied,
    as to its usability in any given situation.

****************************************************************************/

#include <os2.h>
#include <stdio.h>

#include "calc.h"
#include "utils.h"


#define FUDGEFACTOR 29

extern int calcmand( void );

extern long creal = 0L;
extern long cimag = 0L;
extern short maxit = 0;;


typedef struct _mults {
    double	rs;		/* real squared */
    double	is;		/* imag squared */
    double	ri;		/* real * imaginary */
} mults;


static void get_mults(PCPOINT	pcpt, mults far * pmults);
static void pointl2cpoint(  PPOINTL  pt, PCPOINT  cptTarget);
ULONG Mandelval(  PCPOINT pcpt, ULONG ulThreshold);
BOOL PreCheck( void );

extern long convert( double );


static long fudge = (1 << FUDGEFACTOR);
static double dfudge = (double) 0x20000000;
static long lHigh = 0x3fffffff;
static long lLow = 0xbfffffff;


ULONG
Mandelval( PCPOINT pcpt,
	   ULONG ulThreshold)
{
    ULONG	    i;
    static CPOINT   cptNew;
    mults	    m;
    double	    vector;

    m.rs = (double) 0;
    m.is = (double) 0;
    m.ri = (double) 0;

    /* loop until we hit threshold, or point goes infinite ( > 4) */
    for (i = 0L; i < ulThreshold; ++i)
    {

	/* compute the next point */
	cptNew.real = (m.rs - m.is) + pcpt->real;
	cptNew.imag = (m.ri + m.ri) + pcpt->imag;

	/* calculate multiple values */
	get_mults(&cptNew, &m);

	/* if this is above 4, it will go infinite */
	vector = m.is + m.rs;
	if ( vector >= (double) 4.0)
	    return i;
    }

    /* won't go infinite */
    return i;
}


PULONG	 MandelCalc( PCPOINT pcptLL,
		     PRECTL  prcDraw,
		     double  precision,
		     ULONG   ulThreshold,
		     PUSHORT pusBufSize)
{
    ULONG   height;
    ULONG   width;
    ULONG   h;
    PULONG  pbBuf;
    PULONG  pbPtr;
    static  ULONG ulSem = 0L;
    static  HSEM hsemCalc = (HSEM) &ulSem;

    long prec;
    long imag;


    prec = convert(precision);

    creal = convert(pcptLL->real) + (prcDraw->xLeft * prec);
    imag = convert(pcptLL->imag) + (prcDraw->yBottom * prec);

    maxit = (short) ulThreshold;

    height = (prcDraw->yTop - prcDraw->yBottom) + 1;
    width = (prcDraw->xRight - prcDraw->xLeft) + 1;

    *pusBufSize = (USHORT)(height * width * sizeof(ULONG));

    pbBuf = NewFarSeg(*pusBufSize);
    pbPtr = pbBuf;

    DosSemRequest(hsemCalc, -1L);

    for ( ; width > 0; --width)
    {
	for ( h = height, cimag = imag; h > 0; --h, cimag += prec)
	{
            if (PreCheck())
	        *(pbPtr++) = (ULONG) calcmand();
            else
                *(pbPtr++) = 0L;
	}
	creal += prec;
    }

    DosSemClear(hsemCalc);

    return pbBuf;
}


static void
get_mults(PCPOINT   pcpt,
	  mults far * pmults)
{
    pmults->rs = pcpt->real * pcpt->real;
    pmults->is = pcpt->imag * pcpt->imag;
    pmults->ri = pcpt->real * pcpt->imag;
}


long convert( double val)
{
    long val2;

    val *= dfudge;
    val2 = (long)val;

    return (val2);
}


BOOL
PreCheck( void )
{

    if ((creal < lLow) || (creal > lHigh) || (cimag < lLow) || (cimag > lHigh))
        return FALSE;

    return TRUE;
}
