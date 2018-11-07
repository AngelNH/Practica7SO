
/* ---------------------------------------------- */
/* ---------------  PROCESOS  ------------------- */
/* ---------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

#define BRINCO 4*1024
#define vaddress(X) (int) (X-base)

extern char *base;
extern int idproc;

void proc0()
{
    char *ptr=NULL;
    int i;
    char c;

    ptr=base;

	for(i=0;i<6;i++)
	{
    	sprintf(ptr,"Escritura del proceso %d en la pagina %d\n",idproc,i);
		sleep(1);
		ptr+=4096;
	}

    ptr=base;
	for(i=0;i<6;i++)
	{
    	printf("%s",ptr);
		sleep(1);
		ptr+=4096;
	}

	return;
}


void proc1()
{
    char *ptr=NULL;
    int i;
    char c;

    ptr=base;

	for(i=0;i<6;i++)
	{
    	sprintf(ptr,"Escritura del proceso %d en la pagina %d\n",idproc,i);
		sleep(1);
		ptr+=4096;
	}

    ptr=base;
	for(i=0;i<6;i++)
	{
    	printf("%s",ptr);
		sleep(1);
		ptr+=4096;
	}

	return;
}


void proc2()
{
    char *ptr=NULL;
    int i;
    char c;

    ptr=base;

	for(i=0;i<6;i++)
	{
    	sprintf(ptr,"Escritura del proceso %d en la pagina %d\n",idproc,i);
		sleep(1);
		ptr+=4096;
	}

    ptr=base;
	for(i=0;i<6;i++)
	{
    	printf("%s",ptr);
		sleep(1);
		ptr+=4096;
	}

	return;
}


void proc3()
{
    char *ptr=NULL;
    int i;
    char c;

    ptr=base;

	for(i=0;i<6;i++)
	{
    	sprintf(ptr,"Escritura del proceso %d en la pagina %d\n",idproc,i);
		sleep(1);
		ptr+=4096;
	}

    ptr=base;
	for(i=0;i<6;i++)
	{
    	printf("%s",ptr);
		sleep(1);
		ptr+=4096;
	}

	return;
}
