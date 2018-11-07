#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mmu.h>

#define NUMPROCS 4
#define PAGESIZE 4096
#define PHISICALMEMORY 12*PAGESIZE 
#define TOTFRAMES PHISICALMEMORY/PAGESIZE
#define RESIDENTSETSIZE PHISICALMEMORY/(PAGESIZE*NUMPROCS)

extern char *base;
extern int framesbegin;
extern int idproc;
extern int systemframetablesize;
extern int ptlr;

extern struct SYSTEMFRAMETABLE *systemframetable;
extern struct PROCESSPAGETABLE *ptbr;


int getfreeframe();
int searchvirtualframe();
int getfifo();

int pagefault(char *vaddress)
{
    int i;
    int frame,vframe; 
    long pag_a_expulsar;
    int fd;
    char buffer[PAGESIZE];
    int pag_del_proceso;

    // A partir de la dirección que provocó el fallo, calculamos la página
    pag_del_proceso=(long) vaddress>>12;


    // Si la página del proceso está en un marco virtual del disco
    {

		// Lee el marco virtual al buffer

        // Libera el frame virtual
    }


    // Cuenta los marcos asignados al proceso
    i=countframesassigned();

    // Si ya ocupó todos sus marcos, expulsa una página
    if(i>=RESIDENTSETSIZE)
    {
		// Buscar una página a expulsar
		
		// Poner el bitde presente en 0 en la tabla de páginas
        
        // Si la página ya fue modificada, grábala en disco
        {
			// Escribe el frame de la página en el archivo de respaldo y pon en 0 el bit de modificado
        }
		
        // Busca un frame virtual en memoria secundaria
		// Si no hay frames virtuales en memoria secundaria regresa error
		{
            return(-1);
        }
        // Copia el frame a memoria secundaria, actualiza la tabla de páginas y libera el marco de la memoria principal
    }

    // Busca un marco físico libre en el sistema
	// Si no hay marcos físicos libres en el sistema regresa error
    {
        return(-1); // Regresar indicando error de memoria insuficiente
    }

    // Si la página estaba en memoria secundaria
    {
        // Cópialo al frame libre encontrado en memoria principal y transfiérelo a la memoria física
    }
   
	// Poner el bit de presente en 1 en la tabla de páginas y el frame 


    return(1); // Regresar todo bien
}

