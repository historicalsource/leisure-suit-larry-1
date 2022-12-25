#ifndef  EXEC_TYPES_H
#define  EXEC_TYPES_H



/* #define	GLOBAL	extern
*/
#define	STATIC	static
#define	REGISTER	register

#define	VOID		void

typedef	long				LONG;
typedef	unsigned long		ULONG;
typedef	unsigned long		LONGBITS;
typedef	short			WORD;
typedef	unsigned short		UWORD;
typedef	unsigned short		WORDBITS;
typedef	char				BYTE;
typedef	unsigned char		UBYTE;
typedef	unsigned char		BYTEBITS;
typedef	unsigned char		*STRPTR;
typedef	STRPTR			*APTR;
typedef	unsigned char		*MEMPTR;
typedef	MEMPTR			ADDRESS;


typedef	short			SHORT;
typedef	unsigned short		USHORT;



typedef	float			FLOAT;
typedef	double			DOUBLE;
typedef	short			COUNT;
typedef	unsigned short		UCOUNT;
typedef	short			BOOL;
typedef	unsigned char		TEXT;
typedef	COUNT			ENUM;

typedef	WORD				FILE_HANDLE;
#define	NO_HANDLE			-1

typedef	LONG				TIME_VAL;


#define	TRUE		1
#define	FALSE	0
#define	NULL		((STRPTR) 0)

#define	forever	for (;;)

#define	BYTEMASK	0xFF

#define	LIBRARY_VERSION	31

#endif

