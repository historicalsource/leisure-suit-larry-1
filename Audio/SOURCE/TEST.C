/*	test calling dos functions	*/

#include	<dos.h>

int	GetKey();

main()

{
	int	i;
	while(GetKey() == 0)
		printf("waiting\n");

}


int
GetKey()

{
 	int	work;
	work = bdos(6,0x00ff,0) & 0x00ff;
	return(work);

}
