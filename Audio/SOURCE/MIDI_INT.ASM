;MIDI_INT
;Set up interrupts for the Roland MPU-401 midi interface
;The interface uses IRQ2 (interrupt 0xa).  We will grab this interrupt
;and increment a counter to signal that data is ready from the interface.


include	macro.ah

public	_MainLoop
public	_HandleInput
public	PutOut
public	IntHandler

cgroup	group	code
dgroup	group	data


GetVector	equ	35h
SetVector	equ	25h
MIDI_INT	equ	0ah
video		equ	10h
outchar		equ	0ah

STATPORT	equ	331h
DATAPORT	equ	330h
COMPORT		equ	331h
SCRBASE		equ	0b800h
ERROR_OCCURRED	equ	0ffh
DSR		equ	80h
DRR		equ	40h
ACK		equ	0feh
GOTFC		equ	0ffh

extrn	_bp:word

data	segment public 'data'

oldIntHandler	dd	0
screen		dd	01e0h
Error		db	0
DoneRecording	db	0

data	ends




code	segment public 'code'
assume	cs:cgroup, ds:dgroup, es:dgroup

SetInterrupt_	proc	near


	;Get and save the old interrupt handler
	push	es
	dos	GetVector,MIDI_INT
	mov	word ptr oldIntHandler,bx
	mov	word ptr oldIntHandler[2],es
	pop	es

	;Set the new handler
	push	ds
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset cgroup:IntHandler
	dos	SetVector,MIDI_INT
	pop	ds
	mov	al,0c1h

	ret

SetInterrupt_	endp

ResetInterrupt_	proc	near


	pusha

	mov	bx,dgroup
	mov	ds,bx

	mov	dx,word ptr oldIntHandler
	mov	ax,word ptr oldIntHandler[2]
	mov	ds,ax
	dos	SetVector,MIDI_INT

	popa

	ret

ResetInterrupt_	endp

_MainLoop	proc	near

	pusha
	call	SetInterrupt_
	mov	al,041h				;a
	call	Display
	mov	al,043h				;c
	call	Display
	call	SetUp
	cmp	Error,ERROR_OCCURRED
	.if	equal
		jmp	Error_Exit
	.end
loop:
	mov	ah,1
	int	22
	jz 	loop
	mov	al,45h				;e
	call	Display
Error_Exit:
	call	Reset
	mov	al,46h				;f
	call	Display
	call	ResetInterrupt_
	mov	al,47h				;g
	call	Display
	popa	    
	ret

_MainLoop	endp

Display	proc	near
	mov	bx,dgroup
	mov	ds,bx
	mov	bx,SCRBASE
	mov	es,bx
	mov	di,word ptr screen
	stosb
;	inc	word ptr screen
;	inc	word ptr screen
	ret
Display	endp

Setup	proc	near
	pusha
	mov	al,08ch
	call	PutOut
	mov	al,0e0h
	call	PutOut
	mov	al,01eh			;32 timebase
	mov	dx,DATAPORT
	out	dx,al
	mov	al,22h
	call	PutOut
	popa
	ret

Setup	endp

Reset	proc	near
	pusha
	mov	al,011h
	call	PutOut
	mov	bx,dgroup
	mov	ds,bx
ResetWaitLoop:
	cmp	DoneRecording,GOTFC
	jnz	ResetWaitLoop
	popa
	ret

Reset	endp

PutOut	proc	near	
	push	ax
	mov	al,042h
	call	Display
	pop	ax
	push	ax
statloop:
	mov	dx,STATPORT
	in	al,dx
	and	al,DRR
       	jnz	statloop	;loop until its zero
	mov	al,041h
	call	Display
	cli
	pop	ax
	mov	dx,COMPORT
	out	dx,al
dataloop:
	mov	dx,STATPORT
	in	al,dx
	and	al,DSR
	jnz	dataloop
	mov	dx,DATAPORT
	in	al,dx
	mov	bx,dgroup
	cmp	al,ACK
	jz	PutOut_Exit
	mov	Error,ERROR_OCCURRED
PutOut_Exit:
	sti
	ret

PutOut	endp
	
	
IntHandler	proc	near
	cli
	pusha
	mov	al,48h			;h
	call	Display
	;This is our interrupt -- increment the message count.

intloop:
	mov	dx,STATPORT
	in	al,dx
	and	al,DSR
	jnz	intloop

	mov	al,49h			;i
	call	Display

	mov	dx,DATAPORT
	in	al,dx
	mov	ah,00h

	call	_HandleInput

	mov	al,4ah			;j
	call	Display
	popa
	sti
	iret

IntHandler	endp

_HandleInput	proc	near

;process midi byte which is in al

;must be a timing byte of some sort
	
	call	putbuff
	push	ax
	mov	al,4bh			;k
	call	Display
	pop	ax
	cmp	al,0f8h		;was it just a timing mark?
	.if 	equal
		ret		;if so, we're all done
	.else
dumpit:
		call	getbyte
		push	ax
		and	al,080h
		cmp	al,080h		;is it a command
		.if	equal
			pop	ax
			push	ax
			and	al,0f0h
			cmp	al,0f0h		;is it a command
			.if	equal
				mov	bx,dgroup
 				mov	ds,bx
  				dec	_bp
   				mov	DoneRecording,GOTFC
	    			pop	ax
     				ret		;all done
			.end
			pop	ax
			jmp  dumpit
		.end
		pop	ax
		mov	bl,al
		and	bl,0f0h
		cmp	bl,90h
		.if	equal			;throw away on/off
;			call	putbuff		;save midi channel select
			call	getbyte		;get pitch
		.end
		call	putbuff		;must be a pitch on/off
		call	getbyte		;get velocity
		call	putbuff		;stuff it
	.end
	ret

getbyte:

;read a byte from the dataport
	mov	al,5ah		;z
	call	Display

	mov	dx,STATPORT   	;test the dataport not busy
getbyte_loop:
	in	al,dx
	and	al,DSR
	.if	nonzero
		jmp	getbyte_loop
	.else
		mov	dx,DATAPORT
		in	al,dx
	.end
	push	ax
	push	ax
	and	al,0f0h
	mov	cl,4
	shr	al,cl
	or	al,30h
	call	Display
	pop	ax
	and	al,0fh
	or	al,30h
	call	Display
	pop	ax
	ret

putbuff:

;stick a byte into the next available location in the buffer
	mov	bx,dgroup
	mov	ds,bx
	mov	es,bx
	mov	di,_bp	;get next available loc
	stosb
	inc	_bp
	ret

_HandleInput	endp


code	ends
	end

