/* PLAY
 * Play a sound.
 *
 *	compile: MS-DOS
 *
 */


#include	<types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<io.h>
#include	<fcntl.h>
#include	<conio.h>
#include	<x.h>



#define	PC		0
#define	JR		1
#define	TD		2



/* sound node structure
 */
#define	NUM_CHANNELS	4
typedef	struct sndstrc{
	struct	sndstrc	*next;
			COUNT	num;
			MEMPTR	ptr;
			MEMPTR	channel[NUM_CHANNELS];
	}	SNDNODE;


BOOL		c[NUM_CHANNELS];
WORD		machine;
BOOL		done, playSound;
UBYTE	off[] = {0xff, 0xff, 0xff, 0xff, 0xff};

TEXT		usageStr[] = "filename";
ARG		switches[] = {
	'1', GA_BOOL, (int *) &c[0],
		"play voice 1",
	'2', GA_BOOL, (int *) &c[1],
		"play voice 2",
	'3', GA_BOOL, (int *) &c[2],
		"play voice 3",
	'4', GA_BOOL, (int *) &c[3],
		"play voice 4",
	0, 0, 0, 0
	};

extern	TEXT		progName[];


extern	int		GetMachineType(void);
extern	void		SetTimer(void);
extern	void		StartSound(SNDNODE *);
extern	void		SoundOff(void);
extern	void		ResetTimer(void);






void
main(argc, argv)
COUNT	argc;
STRPTR	argv[];
{
	register	MEMPTR		sp;
			UWORD		length;
			COUNT		i;
			FILE_HANDLE	fd;
			SNDNODE		sNode;

	argc = getargs(argc, argv);

	/* If no voices were specified, play all voices.
	 */
	if (!c[0] && !c[1] && !c[2] && !c[3])
		c[0] = c[1] = c[2] = c[3] = TRUE;

	if (argc != 2)
		ShowUsage();

	/* Get the sound file
	 */
	if ((fd = open(argv[1], (int) (O_RDONLY | O_BINARY))) == NO_HANDLE)
		CantOpen(argv[1]);
	length = (UWORD) filelength(fd);
	sNode.ptr = malloc(length);
	read(fd, sNode.ptr, length);
	close(fd);

	/* Get pointers to the channels
	 */
	for (i = 0, sp = sNode.ptr ; i < NUM_CHANNELS ; ++i, sp += 2)
		sNode.channel[i] = sNode.ptr + *sp + 0x100 * *(sp+1);

	/* Get the machine type and turn off the channels we don't want to
	 * play.
	 */
	machine = GetMachineType();
	if (machine == PC) {
		/* Point channel 1 to the first channel which is set to be played.
		 */
		for (i = 0 ; i < NUM_CHANNELS ; ++i) {
			if (c[i]) {
				sNode.channel[0] = sNode.channel[i];
				break;
				}
			}
		}
	else {
		for (i = 0 ; i < NUM_CHANNELS ; ++i)
			if (!c[i])
				sNode.channel[i] = off;
		}

	/* Play the sound until user hits ESC or sound is done.
	 */
	SetTimer();
	StartSound(&sNode);
	while (!done && (!kbhit() || getch() != 0x1b))
		;
	SoundOff();
	ResetTimer();

	free(sNode.ptr);
}

