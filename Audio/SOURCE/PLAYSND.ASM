;PLAYSND
;Assembly level routines for 'play'.

include	macro.ah

public	_SetTimer
public	_ResetTimer
public	TimerIntHandler
public	_StartSound
public	PlayIt
public	_SoundOff
public	PlayNote
public	ConvertFreqToPC
public	_GetMachineType



dgroup	group	_data


SetInt		equ	25h
GetInt		equ	35h

timeInt		equ	8


timerPort	equ	40h	;address of counter port for interval timer
EOI		equ	20h	;end-of-interrupt to 8259


sndPort		equ	0c0h
portB		equ	061h

NUM_CHANNELS	equ	4
CHAN1_SNDOFF	equ	0fh

jrID		equ	0fdh

PC		equ	0
JR		equ	1
TD		equ	2



;This is the counter to load the programmable interval timer with in order
;to get interrupts at a rate of 60/second.  Clock to the PIT is 1.19318 MHz.
TIMER_DIVISOR	equ	19886
TIMER_COUNT	equ	3	;issue BIOS interrupt every 3 interrupts



ROMSeg	segment at 0f000h
org	0fffeh
sysID		db	?
ROMSeg	ends



_DATA	segment	public byte 'data'

extrn	_machine:word
extrn	_playSound:word
extrn	_done:word

;Definition of the sound node
soundNode	struc

sNext		dw	?
sNum		dw	?
sPtr		dw	?
sChan		dw	NUM_CHANNELS dup(?)

soundNode	ends

channel		dw	NUM_CHANNELS dup(?)
count		dw	NUM_CHANNELS dup(?)
channelActive	dw	NUM_CHANNELS dup(?)

;Codes which turn off the various channels
terminateCode	dw	09fh, 0bfh, 0dfh, 0ffh

maxChannel	dw	?

oldTimer	dd	?
timerCounter	db	TIMER_COUNT

channelsPlaying	db	0

tdStr		db	"Tandy"

_DATA	ends




_TEXT	segment	public byte 'code'
assume	cs:_TEXT,ds:dgroup

_SetTimer	proc	near
;Set the address of the new timer interrupt routine.

	;First, save the address of the current handler
	push	es

	dos	GetInt,timeInt
	mov	word ptr oldTimer,bx
	mov	word ptr oldTimer[2],es

	pop	es
	push	ds

	;Change the timer interrupt frequency to 60/second
	mov	al,36h			;timer 0, LSB, MSB, mode 3
	out	timerPort+3,al
	mov	ax,TIMER_DIVISOR
	out	timerPort,al
	xchg	ah,al
	out	timerPort,al

	;Point to our interrupt
	mov	dx,offset _TEXT:TimerIntHandler
	mov	ax,cs
	mov	ds,ax
	dos	SetInt,timeInt

	pop	ds

	return

_SetTimer	endp





_ResetTimer	proc	near
;Set the timer interrupt back to its original handler

	;Set the timer frequency back to normal (18.2/second)
	mov	al,36h			;timer 0, LSB, MSB, mode 3
	out	timerPort+3,al
	xor	al,al
	out	timerPort,al
	out	timerPort,al

	lds	dx,oldTimer
	dos	SetInt,timeInt
	mov	ax,es
	mov	ds,ax

	return

_ResetTimer	endp




TimerIntHandler	proc	near
;This routines gets the interrupt from the timer chip, deals with
;any sounds that need playing, and passes the interrupt to the normal
;BIOS interrupt handler every third interrupt (a rate of 20/second).

	push	ax
	push	ds
	mov	ax,dgroup
	mov	ds,ax

	cmp	_playSound,0
	.if	not_equal
		call	PlayIt
		.end

	dec	byte ptr timerCounter
	.if	nonzero
		;Nothing to do -- issue the end-of-interrupt
		mov	al,EOI
		out	20h,al
	.else
		;Time to call the BIOS routine (which will issue EOI for us)
		mov	timerCounter,TIMER_COUNT
		pushf
		call	oldTimer
		.end
	pop	ds
	pop	ax
	iret


TimerIntHandler	endp





_StartSound	proc	near
;Start the sound whose node is pointed to by sNode.

	enter	sNode

	;Move the channel pointers into our local pointers
	mov	si,sNode
	add	si,sChan
	mov	di,offset dgroup:channel
	mov	cx,NUM_CHANNELS
	rep	movsw
	
	;Clear all the channel counters
	mov	ax,1
	mov	di,offset dgroup:count
	mov	cx,NUM_CHANNELS
	rep	stosw		

	;Set all channels active
	mov	di, offset dgroup:channelActive
	mov	cx,NUM_CHANNELS
	rep	stosw

	;Set the maximum channel # for the machine_
	cmp	_machine,PC
	.if	equal
		mov	maxChannel,2
	.else
		mov	maxChannel,NUM_CHANNELS * 2
		.end

	;Gate sound chip to audio if a pcjr
	cmp	_machine,PC
	.if	not_equal
		in	al,portB
		or	al,60h
		out	portB,al
		.end

	mov	_playSound,1

	exit

_StartSound	endp




PlayIt		proc	near
;Play the next set of frequencies in the sound pointed to by 'sndPtr'.
;This is entered as interrupt code with only ax and ds saved.

	push	bx
	push	cx
	push	dx
	push	si

	xor	bx,bx			;bx will be the channel index
	mov	channelsPlaying,bl	;initialize channels playing count
	do
		;Don't play this channel if it's finished
		cmp	channelActive[bx],1
		.if	not_equal
			add	bx,2
			continue
			.end

		;See if it's time to send a new note out on this channel
		dec	count[bx]
		.if	zero
			;Get the pointer to channel data
			mov	si,channel[bx]

			;Load the count for this channel and see if we're done
			lodsw
			cmp	ax,0ffffh
			.if	equal
				;Turn the channel off
				dec	channelActive[bx]
				mov	al,byte ptr terminateCode[bx]
				call	PlayNote
				break
				.end
			mov	count[bx],ax

			;Get the note and volume
			lodsw
			mov	dx,ax
			lodsb
			mov	cl,al

			mov	channel[bx],si	;save ptr to channel data

			push	bx
			call	PlayNote
			pop	bx
			.end

		;This channel is playing -- increment the number
		;of channels which are
		inc	channelsPlaying

		add	bx,0002h
		cmp	bx,maxChannel
	while	less

	cmp	channelsPlaying,0
	.if	equal
		call	SoundOffSub
		.end

	pop	si
	pop	dx
	pop	cx
	pop	bx

	return
	

PlayIt		endp





_SoundOff	proc	near

	enter
	call	SoundOffSub
	exit



SoundOffSub:
	mov	_playSound,0

	cmp	_machine,PC
	.if	equal
		mov	cl,CHAN1_SNDOFF
		call	PlayNote
	.else
		mov	al,09fh
		out	sndPort,al
		mov	al,0bfh
		out	sndPort,al
		mov	al,0dfh
		out	sndPort,al
		mov	al,0ffh
		out	sndPort,al
		.end

	mov	_done,1
	return

_SoundOff		endp





PlayNote	proc	near
;Note is in dx, volume in cl.

	cmp	_machine,PC
	.if	not_equal			;PCjr/Tandy sounds
		cmp	_machine,TD
		.if	equal
			call	ConvertTDVolume
			.end

		;Send the notes out
		cmp	dh,00h
		.if	not_equal
			mov	al,dh
			out	sndPort,al
			mov	al,dl
			out	sndPort,al
		.end
		mov	al,cl
		out	sndPort,al
	.else					;PC sounds
		and	cl,0fh
		cmp	cl,CHAN1_SNDOFF
		.if	equal
			in	al,portB
			and	al,0fch
			out	portB,al
		.else
			call	ConvertFreqToPC		;returns freq in bx
	
			;Start the note playing.
			mov	al,0b6h
			out	43h,al
	
			mov	ax,bx
			out	42h,al
			mov	al,ah
			out	42h,al
	
			in	al,portB
			or	al,3
			out	portB,al
			.end
		.end

	return


PlayNote	endp



ConvertTDVolume	proc	near
;Convert the volume in the cl to something a little lower for the loud
;Tandy sound channel.

	mov	ch,cl
	and	ch,90h
	cmp	ch,90h
	.if	equal
		mov	ch,cl
		and	ch,0fh
		cmp	ch,8
		.if	less
			and	cl,0f0h
			add	ch,3
			or	cl,ch
			.end
		.end
	return

ConvertTDVolume	endp



ConvertFreqToPC	proc	near
;Convert the note codes in the dx to a frequency divisor in the bx

	;Get low bits into bl
	and	dh,00fh
	mov	bl,dh

	;Add the high bits
	xor	dh,dh
	and	dl,03fh
	mov	cl,4
	shl	dx,cl
	add	bx,dx

	;Multiply by 12 to get note for the PC
	shl	bx,1
	shl	bx,1		;* 4
	mov	dx,bx
	shl	bx,1		;* 8
	add	bx,dx		;+ -> * 12

	return

ConvertFreqToPC	endp




_GetMachineType	proc	near

	enter

	;Point to the ROM
	assume	es:ROMSeg
	push	es
	mov	ax,ROMSeg
	mov	es,ax
	
	;Default to PC or compatible
	mov	ax,PC

	;See if we're on a PCjr
	mov	bl,es:sysID		;get system id byte
	cmp	bl,jrID			;is it a PCjr?
	.if	equal
		mov	ax,JR
		jmp	GetMachExit
		.end
	
	;Test for the Tandy 1000 by scanning from f000:c000 to f000:c400 for the
	;word 'Tandy'.
	mov	di,0c000h
	do
		lea	si,tdStr
		push	di
		mov	cx,5
		rep	cmpsb
		pop	di
		.if	equal			;found 'Tandy',
			mov	ax,TD		;  so set up for Tandy 1000
			break
			.end
		inc	di
		cmp	di,0c400h
	while	less
	
GetMachExit:
	pop	es
	assume	es:dgroup
	exit


_GetMachineType	endp






_TEXT	ends
	end

