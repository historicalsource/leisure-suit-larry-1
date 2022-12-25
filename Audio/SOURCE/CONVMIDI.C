/* CONVMIDI
** Read data in from the Roland MPU-401 midi interface and write it out
** as a file compatible with the Quest interpretor.
*/

/* works with 3 options
1 - only one file in. fit it onto one voice.
2 - only one file in. Dynamically split it to 3 voices.
3 - 3 files in. Fit each into only one voice.
*/

#include	"types.h"
#include	"\include\sys\types.h"
#include 	"\include\sys\stat.h"
#include	"fcntl.h"

struct	FileInfo
{
	unsigned	char	*bp;
	int	size;
}	f;

struct	Note
{
	int	length;
	int	freq;
	int	atten;
	int	OldPitch;
	int	NoteInProgress;
	int	FirstTime;
	int	size, osize;
	unsigned	char	*buffer, *bp, *obuffer;
	int	voice;
	int	AccountedFor;
}	N[3];


void		Convert();
void		HandleByte();
void		PutNote();
void		NoteOff();
void		NoteOn();
void		TerminateRest();
struct	FileInfo		ReadFile();
void		GetOutBuffs();
void		FreeBuffs();
void		FlushBuffs();			/* writes output buffers */
void		ProcessInput();

int	od;
int	i, j;
int	FilesIn;					/* recording from one file or 3 */
int	VoiceMode;				/* split overlaps to seperate voices */
int	OverallTime;
int	ignoreNextNote;
int	throwAway;
int	debugging;

#define	BUFFERSIZE	0x1000
#define	SINGLE		0
#define	MULTIPLE		1
#define	MONO			0
#define	POLY			1

TEXT		progName[] = "convmidi";

int		chan[] = 		{0x8000, 0xa000, 0xc000};
unsigned	char		at[] = 		{0x90, 0xb0, 0xd0};

COUNT	frequency[108];

/* = {
					0x174,
					0x271,
					0x66e,
					0xc6b,
					0x569,
					0x67,
					0xd64,
					0xc62,
					0xd60,
					0xf5e,
					0x35d,
					0x95b,
					0x5a,
					0x958,
					0x357,
					0xe55,
					0xa54,
					0x853,
					0x652,
					0x651,
					0x650,
					0x74f,
					0x94e,
					0xc4d,
					0x4d,
					0x44c,
					0x94b,
					0xf4a,
					0x54a,
					0xc49,
					0x349,
					0xb48,
					0x348,
					0xb47,
					0x447,
					0xe46,
					0x846,
					0x246,
					0xc45,
					0x745,
					0x245,
					0xe44,
					0x944,
					0x544,
					0x144,
					0xd43,
					0xa43,
					0x743,
					0x443,
					0x143,
					0xe42,
					0xb42,
					0x942,
					0x742,
					0x442,
					0x242,
					0x42,
					0xe41,
					0xd41,
					0xb41};


*/

extern	STRPTR	malloc();
STRPTR	buffer, bp, TopOfBuffer, work;

main(argc, argv)
COUNT	argc;
STRPTR	argv[];
{
	int	fdat;
	unsigned	int	work, work2, work3;
	throwAway = FALSE;
	ignoreNextNote = 0;
	if (argc != 3 && argc != 4) {
		printf("use: %s filename mode 1=mono, single 2=mono, multiple 3=poly, multiple\n",
			 progName);
		exit(10);
		}

	if (argc == 4) {
		debugging = TRUE;
		}
	else {
		debugging = FALSE;
		}

	if (*argv[2] == '1') {
		FilesIn = SINGLE;
		VoiceMode = MONO;
		}
	else {
		if (*argv[2] == '2') {
			FilesIn = SINGLE;
			VoiceMode = POLY;
			}
		else {
			if (*argv[2] == '3') {
				FilesIn = MULTIPLE;
				VoiceMode = MONO;
				}
			else {
				printf("Invalid Mode\n");
				exit(10);
				}
			}
		}

	fdat = open("freq.dat",O_BINARY | O_RDWR);	/*	read frequency table	*/
	read(fdat, &frequency[0], 108*2);
	for (i=0; i<108; i++) {
		work = frequency[i];
		work2 = frequency[i];
		work3 = frequency[i];
		work &= 0x0f00;
		work >>= 4;
		work2 &= 0x00f0;
		work2 >>= 4;
		work3 &= 0x000f;
		work3 <<= 8;
		frequency[i] = work | work2 | work3;
		}
	close(fdat);

	f = ReadFile(argv[1], '1');
	N[0].buffer = f.bp;
	N[0].size = f.size;
	if (FilesIn == MULTIPLE) {
		f = ReadFile(argv[1], '2');
		N[1].buffer = f.bp;
		N[1].size = f.size;
		f = ReadFile(argv[1], '3');
		N[2].buffer = f.bp;
		N[2].size = f.size;
		}

	GetOutBuffs();
	Convert();
	FlushBuffs();
	FreeBuffs();
}

void
Convert()
{
	struct	Note	*ptr;
	N[0].voice = 0;
	N[1].voice = 1;
	N[2].voice = 2;
	ptr = &N[0];
	ProcessInput(ptr);
	if (FilesIn == MULTIPLE) {
		ptr = &N[1];
		ProcessInput(ptr);
		ptr = &N[2];
		ProcessInput(ptr);
		}
}

void
ProcessInput(ptr)
struct	Note	*ptr;
{
	unsigned	char	byte;
	ptr->bp = ptr->buffer;
	ptr->FirstTime = TRUE;
	ptr->NoteInProgress = FALSE;
	while (ptr->bp < (ptr->buffer+ptr->size))  {
		byte = *ptr->bp;
		HandleByte(byte, ptr);
		}
}

void
HandleByte(byte, ptr)
unsigned	char	byte;
struct	Note	*ptr;
{
	int	delay, pitch, type;
	if (debugging)
		printf("prior to 0x%x time was 0x%x\n", byte, OverallTime);
	byte &= 0xff;
	if (byte == 0xf8) {
		OverallTime += 240;
		ptr->bp++;
		return;
		}

	delay = byte;
	++ptr->bp;
	pitch = *ptr->bp;
	++ptr->bp;
	type = *ptr->bp;
	++ptr->bp;

	OverallTime += delay;
	if (0xc0 != (0xc0 & pitch))
		if (ptr->NoteInProgress == TRUE && pitch == ptr->OldPitch) {
			NoteOff(pitch, type, ptr);
			}
		else {
			NoteOn(pitch, type, ptr);
			}

}

void
NoteOn(pitch, type, ptr)
int	pitch;
int	type;	/* really how hard the key was struck */
struct	Note	*ptr;
{
	struct	Note	*wp;
	int	NoteBounced;
	NoteBounced = FALSE;
	if (ptr->NoteInProgress == TRUE) {	/* are we in the middle of a note */
		if (VoiceMode == POLY) {			/* can we bounce the note */
			if (ptr->voice == 2) {		/* can't at last voice */
 				NoteOff(pitch, type, ptr);
				}
			else {
				wp = &N[ptr->voice+1];
				if (debugging)
					printf("recursing\n");
				NoteOn(pitch, type, wp);
				NoteBounced = TRUE;
									/* call note on with next voice */
				}
			}
		else {
			if (debugging)
				printf("ignoreNextNote 0x%x pitch 0x%x\n", ignoreNextNote, pitch);
			if (ptr->OldPitch != ignoreNextNote) {
				if (debugging)
					printf("Overlap!!!\n");
				ignoreNextNote = pitch;
				NoteOff(pitch, type, ptr);
								/* terminate prior note */
				}
			else {
				ignoreNextNote = 0;
				throwAway = TRUE;
				}
			}
		}
	else {
		TerminateRest(ptr);
								/* Terminate any Rest in progress */
		}

	if (NoteBounced == FALSE && throwAway != TRUE) {
		if (debugging)
			printf("NoteOn = 0x%x 0x%x \n", ptr->voice, pitch);
		/* if (pitch < 12) {
			printf("too low!!!!!");
			}
		if (pitch > 96) {
			printf("too high!!!!");
			} */
		ptr->freq = frequency[pitch];	
		ptr->OldPitch = pitch;
		ptr->freq |= chan[ptr->voice];
		type >>= 3;
		type = 0xf - type;			/* make 0-15 be 15-0 */
		if (ptr->voice == 1) {
			type = 2;		
			}
		else {
			if (ptr->voice == 2) {
				type = 4;
				}
			else {
				type = 0;
				}
			}
		ptr->atten = at[ptr->voice] | type;
		ptr->NoteInProgress = TRUE;
		}
	throwAway = FALSE;
}

void
NoteOff(pitch, type, ptr)
int	pitch, type;
struct	Note	*ptr;
{
	struct	Note	*wp;

	if (VoiceMode == POLY 
	&& ptr->voice != 2 
	&& (pitch != ptr->OldPitch)) {			/* turning off another voice */
		wp = &N[ptr->voice+1];
		if (debugging)
			printf("recursing to NoteOff\n");
		NoteOff(pitch, wp);
		}
	else {
		if (debugging)
			printf("NoteOff = 0x%x 0x%x \n", ptr->voice, ptr->OldPitch);
		if (FilesIn == MULTIPLE && ptr->FirstTime == TRUE) {
			ptr->FirstTime = FALSE;
			ptr->NoteInProgress = FALSE;
			OverallTime = 0;
			return;	/* flush the first note */
			}

/*			if (pitch != ptr->OldPitch)
			return;
*/

		if (debugging)
			printf("NOteOff overall 0x%x accounted 0x%x\n", OverallTime, ptr->AccountedFor);
		ptr->length = OverallTime - ptr->AccountedFor;
		ptr->AccountedFor = OverallTime;
		if (debugging)
			printf("ending a note of length 0x%x\n", ptr->length);
		if (ptr->length != 0) {	/* no zero length notes */
			PutNote(ptr);
			}
		ptr->NoteInProgress = FALSE;
		ptr->OldPitch = 0;			/* show note over */
		ptr->length = 0;
		}

}		

void
TerminateRest(ptr)
struct	Note	*ptr;
{
	if (ptr->FirstTime == TRUE) {
		if (FilesIn == SINGLE) {
			ptr->FirstTime = FALSE;	/* don't skip first note */
			if (ptr->voice == 0)
				OverallTime = 0;
			}
		else 
			OverallTime = 0;
		return;	/* flush the first rest */
		}
	if (debugging)
		printf("Rest = 0x%x \n", ptr->voice);
	ptr->length = OverallTime - ptr->AccountedFor;
	ptr->AccountedFor = OverallTime;
	ptr->freq = 0 | chan[ptr->voice];
	ptr->atten = 0x0f | at[ptr->voice];
	if (ptr->length != 0) 
		PutNote(ptr);
}

void
PutNote(ptr)
struct	Note	*ptr;
{
	int	i;
	unsigned	char	*bp, *np;
	if ((ptr->atten & 0x0f) == 0x0f) 
		if (debugging)
			printf("rest 0x%x 0x%x\n", ptr->voice, ptr->length);
	else 
		if (debugging)
			printf("note 0x%x 0x%x 0x%x\n", ptr->voice, ptr->OldPitch, ptr->length);


	bp = ptr->obuffer;
	bp += ptr->osize;
	np = ptr;
	for(i = 0; i < 5; i++) {
		*(bp++) = *(np++);
		}
	ptr->osize += 5;
}

struct	
FileInfo	ReadFile(fn, suffix)
unsigned	char	suffix;
unsigned	char	fn[];
{
	struct	FileInfo	f;
	unsigned	char		work[32];
	unsigned	char	*bp, *buffer, *wp;
	int	descriptor;
	strcpy(work, fn);
	if (FilesIn == MULTIPLE) {
		wp = &work[0];
		while (*wp != 0x00) 
			wp++;
		*wp++ = '.';
		*wp++ = suffix;
		*wp = 0x00;
		}

	if ((descriptor = open(work,O_BINARY | O_RDWR)) < 0) {
		printf("%s: can't open %s\n", progName, work);
		exit(10);
		}

	if ((buffer = malloc(BUFFERSIZE)) == NULL) {
		printf("%s: not enough memory\n", progName);
		exit(10);
		}
	bp = buffer;
	while(read(descriptor, bp++, 1) > 0) 
		;
	--bp;
	if (debugging)
		printf("read %d bytes\n", bp - buffer);
	f.size = bp - buffer;
	f.bp = buffer;
	close(descriptor);
	return(f);
}
 
void
GetOutBuffs()
{
	N[0].obuffer = malloc(BUFFERSIZE);
	N[1].obuffer = malloc(BUFFERSIZE);
	N[2].obuffer = malloc(BUFFERSIZE);

}


void
FreeBuffs()
{		
	free(N[0].obuffer);
	free(N[1].obuffer);
	free(N[2].obuffer);
}

void
FlushBuffs()
{
	/* writes all three output buffers */

	struct {
		int	PtrChan1;
		int	PtrChan2;
		int	PtrChan3;
		int	PtrChan4;
		} 		Ptrs;

	Ptrs.PtrChan1 = 8;
	Ptrs.PtrChan2 = Ptrs.PtrChan1 + N[0].osize + 2;
	Ptrs.PtrChan3 = Ptrs.PtrChan2 + N[1].osize + 2;
	Ptrs.PtrChan4 = Ptrs.PtrChan3 + N[2].osize + 2;
	od = open("output", O_BINARY | O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);
	write(od, &Ptrs, 8);

	for(i=0; i <3; i++) {
		write(od, N[i].obuffer, N[i].osize);
		j = 0xffff;
		write(od, &j, 2);
		}
	j = 0xffff;
	write(od, &j, 2);

	close(od);
}


