page ,132

	TITLE	Temporary stack switcher for terminating threads.

?PLM  = 1		; Use PASCAL calling convention
?WIN  = 1		; Generate Windows prolog/epilog
memL  = 1		; Large model for multithread

.xlist
include cmacros.inc
.list

CBTEMPSTACK     = 1024h
                  ; 1024 is just an arbitrary size that we picked which is
                  ; large enough for the C5.1 free and _endthread calls.

sBegin	Data

TmpStacks   label byte

	    db	CBTEMPSTACK dup(0)
TmpStack1   label byte

	    db	CBTEMPSTACK dup(0)
TmpStack2   label byte

sEnd	Data

sBegin	Code		; We'll just put this in the default code segment _TEXT

assumes cs,_TEXT
assumes ds,DGROUP

;----------------------------- Public  Function ----------------------------;
;
;
; TmpStack - Switch to one of two, small temporary stacks so that a thread
;	     can free its stack just before it exits.
;
;	     Our one parameter controls which temporary stack we use.
;	     1 and 2 are valid. Any other value sets SS to 0 so that we
;	     GP fault and show that something's wrong.
;
;	     The idea here is to provide the thread a means of freeing
;	     its stack. To let some other thread free it we could use
;	     DosEnterCritSec and the fact that terminating a thread does
;	     an implicit DosExitCritSec, but this requires the use of an
;	     additional semaphore and possibly posting a message. An easier
;	     method is to just switch to another stack big enough to do the
;	     free of the main stack and end the thread. This is exactly what
;	     TmpStack does. It also shows a method of providing more than one
;	     stack for use by multiple threads. Multiple threads could also
;	     use a semaphore or DosEnterCritSec to protect a single stack.
;
;	     The intended use is:
;
;		    /* Switch us onto a small temporary stack just big enough
;		       to let us make the DOS call to free our existing stack
;		       and then exit the thread. */
;		    TmpStack(1);
;		    DosFreeSeg(ThreadStack1);
;		    ThreadStack1 = NULL;
;		    DosExit(EXIT_THREAD,0);
;		}
;
;---------------------------------------------------------------------------;

cProc	TmpStack,<FAR,PUBLIC,PASCAL>

    parmW   StackNum		; The stack to switch to.

cBegin	<nogen>

	pop	bx		; Save our return address
	pop	cx

	pop	ax		; Get the stack number

	mov	dx,seg TmpStacks; Get the address of the data segment

	dec	ax
	jz	SetTmpStack1	; Use the first stack

	dec	ax
	jz	SetTmpStack2	; Use the second stack

	xor	dx,dx		; Mark it as bad so we'll GP fault later
	jmp	short SetStack

SetTmpStack1:
	mov	sp,DataOFFSET TmpStack1
	jmp	short SetStack

SetTmpStack2:
	mov	sp,DataOFFSET TmpStack2

SetStack:
	mov	ss,dx

	push	cx		; Restore our return address
	push	bx

	retf

cEnd	<nogen>

sEnd	Code

end
