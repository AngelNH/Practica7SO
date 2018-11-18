#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

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
extern int ptlr;//tamaño de tabla de paginas

extern struct SYSTEMFRAMETABLE *systemframetable;
extern struct PROCESSPAGETABLE *ptbr;


int getfreeframe();
int searchvirtualframe();
int getfifo();

//funciones mias
int getPageToFree();
void logSecundaria(int frame);

int pagefault(char *vaddress)
{
    int i;
    int frame,vframe; 
    long pag_a_expulsar;
    int fd;
    char buffer[PAGESIZE];
    int pag_del_proceso;
    //nuestras
    char emptyBuffer[PAGESIZE];
    char auxBuffer[PAGESIZE];
    
    strcpy(emptyBuffer,"");

    // A partir de la dirección que provocó el fallo, calculamos la página
    pag_del_proceso=(long) vaddress>>12;


    // Si la página del proceso está en un marco virtual del disco
    
    if(ptbr[pag_del_proceso].presente == 0 && 
        ptbr[pag_del_proceso].framenumber != -1){

		// Lee el marco virtual al buffer
        readblock(buffer,ptbr[pag_del_proceso].framenumber); //checar como enviar buffer.
        // Libera el frame virtual
        writeblock(emptyBuffer,ptbr[pag_del_proceso].framenumber);//limpia pAddress asi que CHECAR 
    }
    


    // Cuenta los marcos asignados al proceso
    i=countframesassigned();

    // Si ya ocupó todos sus marcos, expulsa una página
    if(i>=RESIDENTSETSIZE)
    {
		// Buscar una página a expulsar
		int free = getPageToFree();
        printf("PAGINA LIBRE ----------------- %d\n",free);
		// Poner el bitde presente en 0 en la tabla de páginas
        ptbr[free].presente = 0;
        printf("PRESENTE DE PAGINA %d ----------------- %d\n",free,ptbr[free].presente);
        // Si la página ya fue modificada, grábala en disco
        if(ptbr[free].modificado == 1){
            printf("PAGINA MODIFICADA -----------------\n");
            
			// Escribe el frame de la página en el archivo de respaldo y pon en 0 el bit de modificado
            saveframe(ptbr[free].framenumber);
            logSecundaria(ptbr[free].framenumber);
            printf("FRAME A GUARDAR ----------------- %x -- %d\n",ptbr[free].framenumber,ptbr[free].modificado);
            ptbr[free].modificado = 0;
            //GUARDAR AUX------------------------------------------------
            strcpy(auxBuffer,systemframetable[ptbr[free].framenumber].paddress);
            printf("AUX BUFFER----------------- %p \n",systemframetable[ptbr[free].framenumber].paddress);
            //auxBuffer = systemframetable[ptbr[free].framenumber].paddress;
        }
        
		
        // Busca un frame virtual en memoria secundaria
        int frameVirtual = searchvirtualframe();
        printf("FRAME LIBRE ----------------- %d\n",frameVirtual);//vamos bien
		// Si no hay frames virtuales en memoria secundaria regresa error
        if(frameVirtual == -1)
		{
            return(-1);
        }
        // Copia el frame a memoria secundaria, actualiza la tabla de páginas y libera el marco de la memoria principal
        copyframe(frameVirtual,ptbr[free].framenumber);
        writeblock(auxBuffer,frameVirtual);//pendiente
        loadframe(ptbr[free].framenumber);
        systemframetable[ptbr[free].framenumber].assigned = 0;
        ptbr[free].framenumber = frameVirtual;
    }

    // Busca un marco físico libre en el sistema
    int mainFrame = getfreeframe();
    printf("MAIN FRAME ----------------- %s \n",systemframetable[mainFrame].paddress);
	// Si no hay marcos físicos libres en el sistema regresa error
    if(mainFrame == -1)
    {
        return(-1); // Regresar indicando error de memoria insuficiente
    }

    // Si la página estaba en memoria secundaria
    if(ptbr[pag_del_proceso].presente == 0 && 
        ptbr[pag_del_proceso].framenumber != -1)
    {
        // Cópialo al frame libre encontrado en memoria principal y transfiérelo a la memoria física
        copyframe(ptbr[pag_del_proceso].framenumber,mainFrame);
        writeblock(buffer,mainFrame);//quien sabe
        loadframe(mainFrame);
        ptbr[pag_del_proceso].framenumber = mainFrame;
        // Poner el bit de presente en 1 en la tabla de páginas y el frame 
        ptbr[pag_del_proceso].presente = 1;
        systemframetable[mainFrame].assigned = 1;
            
    } else {
        ptbr[pag_del_proceso].framenumber = mainFrame;
        // Poner el bit de presente en 1 en la tabla de páginas y el frame 
        ptbr[pag_del_proceso].presente = 1;
        systemframetable[mainFrame].assigned = 1;
    }

    return(1); // Regresar todo bien
}


int getPageToFree(){
    int i;
    int menor = ptbr[0].tlastaccess;
    int id=0;
    int assigned=0;// bandera para la primera asignación
    for(i=0;i<ptlr;i++){
        if(ptbr[i].presente){
            menor = ptbr[i].tlastaccess;
            assigned=1;
            id = i;
        }
        
        if(menor>ptbr[i].tlastaccess && ptbr[i].presente == 1 && assigned == 1){
            menor = ptbr[i].tlastaccess;
            id = i;
        }
    }
    return id;
}
int searchvirtualframe(){
    int i;
    char auxBuffer[PAGESIZE];
    for(i=framesbegin*2;i<systemframetablesize+framesbegin;i++){
           readblock(auxBuffer,i);
         printf("MEMORIA VIRTUAL FRAME %d----------------- %p \n",i,auxBuffer);
        if(strcmp(auxBuffer,"")==0){
            
            return i;
        }
        
    }
    return -1;
}
int getfreeframe()
{
    int i;
    // Busca un marco libre en el sistema
    for(i=framesbegin;i<systemframetablesize+framesbegin;i++)
        if(!systemframetable[i].assigned)
        {
            systemframetable[i].assigned=1;
            break;
        }
    if(i<systemframetablesize+framesbegin)
        systemframetable[i].assigned=1;
    else
        i=-1;
    return(i);
}
void logSecundaria(int frame){
    char auxBuffer[PAGESIZE];
     readblock(auxBuffer,frame);
    printf("LOG -- MEMORIA VIRTUAL FRAME %d----------------- %p ------------------ \n",frame,auxBuffer);
}

