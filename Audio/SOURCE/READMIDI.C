/* READMIDI
** Read data in from the Roland MPU-401 midi interface and write it out
** as a file.
*/


#include	"types.h"
#include	"\include\sys\types.h"
#include 	"\include\sys\stat.h"
#include	"fcntl.h"



void		ClearBuffer();

#define	BUFFERSIZE	0x4000

#define	DATAPORT		0x330
#define	STATPORT		0x331
#define	COMPORT		0x331
#define	DSR			0x80
#define	DRR			0x40

#define	ACK			0xfe
#define	TIMER_OVERFLOW	0xf8
#define	ALL_END		0xfc
#define	MIDI_SYS_MSG	0xff
#define	EOX			0xf7

#define	IsStatus(b)	((b) & 0x80 == 0x80)
#define	MidiCommand(b)	(((b) >> 4) & 7)
#define	Channel(b)	((b) & 0x0f)


TEXT		progName[] = "readmidi";
BOOL		done;
UBYTE	runningStatus;
COUNT	dataBytes[] = {2, 2, 2, 2, 1, 1, 2};

extern	STRPTR	malloc();
STRPTR	buffer, bp, TopOfBuffer;

main(argc, argv)
COUNT	argc;
STRPTR	argv[];
{
	FILE_HANDLE	fd;
	char	*workPtr;
	if (argc != 2) {
		printf("use: %s filename\n", progName);
		exit(10);
		}

	if ((fd = open(argv[1],O_BINARY | O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE)) == NO_HANDLE) {
		printf("%s: can't open %s\n", progName, argv[1]);
		exit(10);
		}

	if ((buffer = malloc(BUFFERSIZE)) == NULL) {
		printf("%s: not enough memory\n", progName);
		exit(10);
		}
	bp = buffer;
	printf("buffer at 0x%x\n", buffer);

	ClearBuffer();

	MainLoop();
 	TopOfBuffer = bp;
	if (TopOfBuffer != buffer) {
		TopOfBuffer = bp - 1;
		workPtr = TopOfBuffer;
		while ((*workPtr & 0x00ff) != 0x00b0 && workPtr > buffer) 
			--workPtr;
		TopOfBuffer = workPtr - 1;
		printf("buffer 0x%x top 0x%x \n", buffer, TopOfBuffer);
	
		write(fd, buffer, ((TopOfBuffer - buffer)));
		printf("file written\n");
		}

	close(fd);
	free(buffer);
}

void
ClearBuffer()
{
	bp = buffer;
	while(bp <= (buffer + BUFFERSIZE)) {
		*bp = 0xaa;
		++bp;
		}
	bp = buffer;
}


