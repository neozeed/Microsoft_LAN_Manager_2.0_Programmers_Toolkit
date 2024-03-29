;	CALCMAND.ASM - Mandelbrot/Julia Set calculation Routines

;	The routines in this code perform Mandelbrot and Julia set
;	calculations using 32-bit integer math as opposed to the 
;	"traditional" floating-point approach.

;	This code relies on several tricks to run as quickly as it does.

;	One can fake floating point arithmetic by using integer
;	arithmetic and keeping track of the implied decimal point
;	if things are reasonable -- and in this case, they are.
;	I replaced code that looked like: z = x*y with code that
;	looks like:
;			ix = x * ifudge			(outside the loops)
;			iy = y * ifudge
;			....
;			iz = (ix * iy) / ifudge		(inside the loops)
;	(and keep remembering that all the integers are "ifudged" bigger)

;	The 386 has native 32-bit integer arithmetic, and (briefly) keeps
;	64-bit values around after 32-bit multiplies.   If the result is
;	divided down right away, you've got 64-bit arithmetic.   You just
;	have to ensure that the result after the divide is <= 32 bits long.
;	CPUs predating the 386 have to emulate 32-bit arithmetic using
;	16-bit arithmetic, which is significantly slower.

;	Dividing is slow -- but shifting is fast, and we can select our
;	"fudge factor" to be a power of two, permitting us to use that
;	method instead.   In addition, the 386 can perform 32-bit wide
;	shifting -- and even 64-bit shifts with the following logic:
;			shdr	eax,edx,cl
;			shr	edx,cl
;	so we make sure that our "fudge factor" is a power of 2 and shift
;	it down that way.


;					Bert Tyler


.MODEL	medium,c

.8086

.DATA

FUDGEFACTOR	equ	29

; ************************ External variables *****************************

	extrn	creal:dword, cimag:dword ; Julia Set Constant
	extrn	maxit:word             	; maximum iterations, colors


; ************************ Internal variables *****************************


x		dd	0		; temp value: x
y		dd	0		; temp value: y
a		dd	0		; temp value: a
b		dd	0		; temp value: b
lm		dd	4 shl FUDGEFACTOR ; Mandelbrot Bail-Out value
k		dw	0		; local counter

.CODE

; ***************** Function calcmand() **********************************

calcmand proc	uses di si es

	push	es			; save the original ES value

.386					; 386-specific code starts here

code32bit:
	mov	esi,creal		; use ESI for real
	mov	edi,cimag		; use EDI for imag
        mov     ax, maxit
        mov     k, ax                   ; and k is our iteration count

;	This is the main processing loop.  Here, every T-state counts...

kloop:					; for (k = 0; k <= maxit; k++)

	mov	eax,esi			; compute (x * x)
	imul	esi			;  ...
	shrd	eax,edx,FUDGEFACTOR	; ( / fudge)
	shr	edx,FUDGEFACTOR-1	; (complete 64-bit shift and check
	cmp	edx,0			;  for any overflow/sign reversals)
	jne	short kloopend		; bail out if too high
	mov	ecx,eax			; save this for below

	mov	eax,edi			; compute (y * y)
	imul	edi			;  ...
	shrd	eax,edx,FUDGEFACTOR	; ( / fudge)
	shr	edx,FUDGEFACTOR-1	; (complete 64-bit shift and check
	cmp	edx,0			;  for any overflow/sign reversals) 
	jne	short kloopend		; bail out if too high

	mov	ebx,ecx			; compute (x*x - y*y) / fudge
	sub	ebx,eax			;  for the next iteration

	add	ecx,eax			; compute (x*x + y*y) / fudge
	jo	short kloopend		; bail out if too high
	js	short kloopend		;  ...

	mov	eax,edi			; compute (y * x)
	imul	esi			;  ...
	shrd	eax,edx,FUDGEFACTOR-1	;  ( * 2 / fudge)
	add	eax,cimag               ;     (above) + b
	mov	edi,eax			;  save this as y

;	(from the earlier code)		; compute (x*x - y*y) / fudge
	add	ebx,creal		;       + a
	mov	esi,ebx			; save this as x


	dec	k			; while (k < maxit) (dec to 0 is faster)
	jz	short kloopend		; while (k < maxit) ...

	cmp	ecx,lm			; while ( lr <= lm)
	jbe	kloop			;  ...

.8086					; 386-specific code ends here

kloopend:
	mov	ax,maxit		; compute color
	sub	ax,k			;  (first, re-compute "k")
wedone:					; restore everything and return.
	pop	es			; restore the original ES value
	ret				; and return.

calcmand endp

        end
