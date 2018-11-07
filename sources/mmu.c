#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <semaphores.h>
#include <mmu.h>

#define VERSION "mmu versión 18.0629.00\n"

#define PAGESIZE 4096
#define PHISICALMEMORYSIZE 48*1024
#define SYSTEMFRAMETABLESIZE PHISICALMEMORYSIZE/PAGESIZE
#define TOTFRAMES SYSTEMFRAMETABLESIZE
#define MAXPROC 4
#define PROCESSPAGETABLESIZE 2*SYSTEMFRAMETABLESIZE/MAXPROC 
#define TABLESSIZE 2*PAGESIZE// 2*SYSTEMFRAMETABLESIZE*sizeof(struct SYSTEMFRAMETABLE)+ptlr*MAXPROC



struct SYSTEMFRAMETABLE *systemframetable;
struct PROCESSPAGETABLE *gprocesspagetable;
struct PROCESSPAGETABLE *ptbr;

int systemframetablesize = SYSTEMFRAMETABLESIZE;
int ptlr = PROCESSPAGETABLESIZE;
int framesbegin;
int framesend;
int idproc;
char *base = NULL;
long starttime;
int totalpagefaults=0;
int debugmode=0;
int exmut,semdebug;

// -----------------------------------------
// < ----- Redefinición de funciones ----- >
// -----------------------------------------

void *getbaseaddr();
void initprocesspagetable();
void freeprocessmem();
unsigned long thisinstant();
void settimer();
void exiterror();

int copyframe(int sframe,int dframe);
int writeblock(char *buffer, int dblock);
int readblock(char *buffer, int sblock);
int loadframe(int frame);
int saveframe(int frame);


void proc0();
void proc1();
void proc2();
void proc3();
// -------------------------------
// < ----- Signal Handlers ----- >
// -------------------------------
void detachallpages(int sig)
{
    // Desconecta la memoria compartida
    int i;
    char *ptr;

    for(i=0;i<ptlr;i++)
    {
        if(ptbr[i].presente  && ptbr[i].attached)
        {
            ptbr[i].attached=0;
            ptr=base+i*PAGESIZE;
            if(shmdt(ptr) == -1)
                fprintf(stderr,"Detachallpages, Proceso %d, Error en el shmdt %p, página=%i\n",idproc,ptr,i);
        }
    }
    return;
}

void seg_handler(int sig,siginfo_t *sip,void *notused)
{
    int i;
    int pag_del_proceso;
    char *vaddress;
    int flags;
    int result;
    char *ptr,*page_ptr;
    

    ptr=sip->si_addr;
    vaddress=(char *)((long) sip->si_addr - (long) base);
    pag_del_proceso=(long) vaddress / PAGESIZE;

    if(pag_del_proceso>=ptlr)
    {
        fprintf(stderr,"Error: dirección fuera del espacio asignado al proceso\n");
        exiterror();
    }


    if(ptbr[pag_del_proceso].modificado)
        flags=SHM_RND;
    else  
        flags=SHM_RND | SHM_RDONLY;

    // Si la página está presente y conetcada, trataron de modificarla
    // Poner el bit de modificado en 1
    if(ptbr[pag_del_proceso].presente && 
       ptbr[pag_del_proceso].attached)
    {
        if(debugmode)
        {
			semaphore_wait(semdebug);
            printf("---------------------------\nPage fault handler\n");
            printf("Página modificada de la dirección=%lX\n",(long) vaddress);
            printf("Proceso=%d Página=%d\n",idproc,pag_del_proceso);
			semaphore_signal(semdebug);
        }
        ptbr[pag_del_proceso].modificado=1;
        page_ptr=base+pag_del_proceso*PAGESIZE;
        shmdt(page_ptr);
        flags=SHM_RND;
    }

   
    // Poner la marca de tiempo
    ptbr[pag_del_proceso].tlastaccess=thisinstant();

    // Fallo de página cuando la página no está presente
    if(! ptbr[pag_del_proceso].presente)
    {

        if(debugmode)
        {
			semaphore_wait(semdebug);
            printf("---------------------------\nPage fault handler\n");
            printf("Direccion que provocó el fallo=%lX\n",(long) vaddress);
            printf("Proceso=%d Página=%d\n",idproc,pag_del_proceso);
			semaphore_signal(semdebug);
        }
        
        // Establece el tiempo de llegada de la página
        ptbr[pag_del_proceso].tarrived=ptbr[pag_del_proceso].tlastaccess;
        // Cuenta los fallos de página por proceso
        totalpagefaults++;
        // Llama a la rutina de fallos de página
        semaphore_wait(exmut);
        result=pagefault(vaddress);
        semaphore_signal(exmut);
        if(result==-1)
        {
            fprintf(stderr,"ERROR, no hay memoria disponible para el proceso %d, proceso abortado.\n",idproc);
            exiterror();
        }

        if(debugmode)
        {
			semaphore_wait(semdebug);
            printf("Proceso=%d, Página %X cargada en el marco %X\n",idproc,pag_del_proceso,ptbr[pag_del_proceso].framenumber);
            printf("Tabla de páginas Proc -> Pag Pr Mo Fr\n");
            for(i=0;i<ptlr;i++)
                printf("                    %d ->   %d  %d  %d %2X\n",idproc,
                                  i,
                                  ptbr[i].presente,
                                  ptbr[i].modificado,
                                  ptbr[i].framenumber);
			semaphore_signal(semdebug);
        }
    }

    // Liberar los marcos que no están asigados a páginas
    for(i=0;i<ptlr;i++)
        if(!ptbr[i].presente && ptbr[i].attached)
        {
            if(debugmode)
			{
				// semaphore_wait(semdebug);
                printf("Proceso %d, expulsa página %d\n",idproc,i);
				// semaphore_signal(semdebug);
			}
            ptbr[i].modificado=0;
            ptbr[i].attached=0;
            if(shmdt(base + i*PAGESIZE)==-1)
            {
                fprintf(stderr,"Error en el shmdt");
            }
        }

    //  Mapear la página con la memoria compartida
    if(ptbr[pag_del_proceso].presente)
    {
        ptbr[pag_del_proceso].attached=1;

        if ((ptr=shmat(systemframetable[ptbr[pag_del_proceso].framenumber].shmidframe, ptr, flags)) ==NULL)
        {
            fprintf(stderr,"Error al conectarse con memoria compartida\n");
            exiterror();
        }  

    }

    return;
}


void bus_handler(int i)
{
    printf("bus error handler\n");
}

// -----------------------------------
// < ----- Inicio del programa ----- >
// -----------------------------------

int main(int argc, char *argv[])
{
    int i,x;
    int statval;
    void *tablesarea = (void *)0;
    void *framesptr = (void *)0;
    void *thisframeptr = (void *)0;
    long endtime,tottime;
    int shmidtables,shmidframes;
    struct shmid_ds shmbuf;
    struct timeval ts;
    struct sigaction act;

    if(argc>2)
    {
        fprintf(stderr,"Error en los argumentos\nUso: procesos [/debug|/version]\n");
        exit(1);
    }
    if(argc==2)
    {
        if(strcmp(argv[1],"/debug")==0)
            debugmode=1;
 
        else if (strcmp(argv[1],"/version")==0)
        {
            printf(VERSION);
            exit(0);
        }
        else
        {
            fprintf(stderr,"Error en los argumentos\nUso: procesos [/debug|/version]\n");
            exit(1);
        }        
    }
    // Toma el tiempo de inicio
    gettimeofday(&ts,NULL);
    starttime=ts.tv_sec*1000000+ts.tv_usec;

    // Establecer handler para fallos de página
    act.sa_sigaction=seg_handler;
    sigaddset(&act.sa_mask,SIGSEGV);
    sigaddset(&act.sa_mask,SIGALRM);
    act.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&act,0);

    // Establecer el handler para el timer
    act.sa_handler=detachallpages;
    sigaddset(&act.sa_mask,SIGSEGV);
    sigaddset(&act.sa_mask,SIGALRM);
    sigaction(SIGALRM,&act,0);

    act.sa_handler=bus_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags=0;
    sigaction(SIGBUS,&act,0);

    // Establecer apuntadores
    tablesarea=getbaseaddr();
    gprocesspagetable=tablesarea+PAGESIZE;
    framesptr=tablesarea+2*PAGESIZE;
    base=framesptr+PHISICALMEMORYSIZE;
        


    // Poner tablas en memoria compartida
    shmidtables=shmget((key_t) 0x1234,TABLESSIZE,0666|IPC_CREAT);
    if(shmidtables==-1)
    {
        fprintf(stderr,"Error en el shmget (1)\n");
        exit(EXIT_FAILURE);
    }
	// tablesarea apunta a la dirección donde estará systemframetable y gprocesspagetable
    tablesarea=shmat(shmidtables,tablesarea,0);
    if(tablesarea==(void *)-1)
    {
        fprintf(stderr,"Error en el shmat (1)\n");
        exit(EXIT_FAILURE);
    }

	semdebug=semget((key_t) 3457,1,0600|IPC_CREAT);
    set_semvalue(1,semdebug);

    if(debugmode)
    {
		semaphore_wait(semdebug);
        printf("System Frame Table area = %p\n",tablesarea);
        printf("Proccess Page Table area = %p\n",gprocesspagetable);
        printf("Frames area = %p\n",framesptr);
        printf("Base = %p\n",base);
		semaphore_signal(semdebug);
    }

    
    framesbegin=(long) framesptr/PAGESIZE;
    framesend=framesbegin+SYSTEMFRAMETABLESIZE;
    printf("Primer marco %X\n",framesbegin);

    systemframetable=(struct SYSTEMFRAMETABLE *) tablesarea-framesbegin;
	
    ptbr=gprocesspagetable;

    // Crea la tabla de marcos disponibles del sistema
    for(i=framesbegin;i<framesend;i++)
    {
        if ((shmidframes = shmget(0x2234+i, PAGESIZE, SHM_RND | IPC_CREAT | 0666)) <0)
        {
            printf("Error could not create shared memory\n");
            exit(1);
        }
        systemframetable[i].assigned=0;
        systemframetable[i].shmidframe=shmidframes;
        thisframeptr=framesptr+PAGESIZE*(i-framesbegin);
        thisframeptr=shmat(systemframetable[i].shmidframe,thisframeptr,SHM_RND);
        if(thisframeptr==NULL)
        {
            fprintf(stderr,"Error en el shmat\n");
            exit(EXIT_FAILURE);
        }

        systemframetable[i].paddress=(char *) thisframeptr;
        if(debugmode)
		{
			// semaphore_wait(semdebug);
            printf("Frame %X, Dirección %p %p \n",i,thisframeptr,systemframetable[i].paddress);
			// semaphore_signal(semdebug);
		}
    }
    // Para los marcos virtuales
    for(i=framesbegin;i<framesend;i++)
        systemframetable[SYSTEMFRAMETABLESIZE+i].assigned=0;

    // Un semaforo por si las moscas
    exmut=semget((key_t) 3456,1,0600|IPC_CREAT);
    set_semvalue(1,exmut);

	
    // Crea los procesos
    for(idproc=0;idproc<MAXPROC;idproc++)
    {
        if(fork()==0)
        {
            settimer();
            
            // base=getbaseaddr();
            
            initprocesspagetable();

            switch(idproc)
            {
                case 0:
                    proc0();
                    break;
                case 1:
                    proc1();
                    break;
                case 2:
                    proc2();
                    break;
                case 3:
                    proc3();
                    break; 
            }


            freeprocessmem();
            printf("Termina proceso %d, Total de fallos de página = %d\n",idproc,totalpagefaults);
            exit(0);
        }
        ptbr+=ptlr;
     
    }

    // Espera a que terminen los procesos
    for(i=0;i<MAXPROC;i++)
        wait(&statval);

    gettimeofday(&ts,NULL);
    endtime=ts.tv_sec*1000000+ts.tv_usec;
    tottime=endtime-starttime;
    
    printf("Tiempo total de ejecución %1.6f\n",(float) tottime/1000000);

    // Eliminar memoria compartida
    for(i=framesbegin;i<framesbegin+SYSTEMFRAMETABLESIZE;i++)
    {
        if(shmctl(systemframetable[i].shmidframe,IPC_RMID,&shmbuf)==-1)
            fprintf(stderr,"Error al eliminar frame # %d",i);
    } 

    if(shmdt(tablesarea)==-1)
        fprintf(stderr,"Error en el shmdt final\n");
    if(shmctl(shmidtables,IPC_RMID,&shmbuf)==-1)
        fprintf(stderr,"Error al eliminar memoria compartida\n");

    // Eliminar el semáforo
    del_semvalue(exmut);
	del_semvalue(semdebug);

}

// -------------------------
// < ----- Funciones ----- >
// -------------------------

   
void *getbaseaddr()
{
    void *ptr;
    // get the pointer to the end
    ptr = sbrk(0);

    // round it to page boundary

    ptr = (char *) (((unsigned long)ptr) + (((unsigned long)SHMLBA) - (((unsigned long)ptr) % ((unsigned long)SHMLBA))));
    return(ptr);
}

int countframesassigned()
{
    int i,j=0;
    //  Cuenta los marcos asignados al proceso
    if(debugmode)
	{
		// semaphore_wait(semdebug);
        printf("Páginas del proceso -->");
		// semaphore_signal(semdebug);
	}
    for(i=0;i<ptlr;i++)
        if(ptbr[i].presente)
        {
            if(debugmode)
			{
				semaphore_wait(semdebug);
                printf(" %d ",i);
				semaphore_signal(semdebug);
			}
            j++;
        }
    if(debugmode)
	{
		// semaphore_wait(semdebug);
        printf("\n"); 
		// semaphore_signal(semdebug);
	}
    return(j);
}


void initprocesspagetable()
{
    int i;
    for(i=0;i<ptlr;i++)
    {
        ptbr[i].presente=0;
        ptbr[i].framenumber=NINGUNO;
        ptbr[i].modificado=0;
        ptbr[i].attached=0;
    }
    return;
}


void freeprocessmem()
{   
    int i;
    char *ptr;
    struct itimerval itimer, otimer;

    // Detiene el timer
    itimer.it_interval.tv_sec=0;
    itimer.it_interval.tv_usec=0;
    itimer.it_value.tv_sec=0;
    itimer.it_value.tv_usec=0;

    // Libera los marcos de la memoria
    if(setitimer(ITIMER_REAL,&itimer,&otimer)<0)
    {
        fprintf(stderr,"Error en el settimer");
        exit(1);
    }

    
    if(debugmode)
	{
		semaphore_wait(semdebug);
        printf("Proceso %d, libera los marcos -->",idproc);
		semaphore_signal(semdebug);
	}

    for(i=0;i<ptlr;i++)
    {
        if(ptbr[i].presente)
        {
            ptbr[i].presente=0;
            systemframetable[ptbr[i].framenumber].assigned=0;
            if(debugmode)
			{
				semaphore_wait(semdebug);
                printf(" %X ",ptbr[i].framenumber);
				semaphore_signal(semdebug);
			}

            if(ptbr[i].attached)
            {
                ptbr[i].attached=0;
                ptr=base+i*PAGESIZE;
                if(shmdt(ptr) == -1)
                    fprintf(stderr,"Error en el shmdt %p, página=%i\n",ptr,i);
            }
        }
    }
    if(debugmode)
	{
		semaphore_wait(semdebug);
        printf("\n");
		semaphore_signal(semdebug);
	}
    return;
}


unsigned long thisinstant()
{
    struct timeval ts;
    gettimeofday(&ts,NULL);
    return(starttime-ts.tv_sec*1000000+ts.tv_usec);
}

void settimer()
{
    struct itimerval itimer, otimer;

    // Inicia el timer
    itimer.it_interval.tv_sec=0;
    itimer.it_interval.tv_usec=10000;
    itimer.it_value.tv_sec=0;
    itimer.it_value.tv_usec=10000;

    if(setitimer(ITIMER_REAL,&itimer,&otimer)<0)
    {
        fprintf(stderr,"Error en el settimer");
        exit(1);
    } 

    return;
}

void exiterror()
{
    freeprocessmem();
    exit(1);
}


// Funciones para lectura/escritura de frames
int copyframe(int sframe,int dframe)
{
	int fd;
    char buffer[PAGESIZE];
	
	fd=open("swap",O_RDWR);
    lseek(fd,(sframe-framesbegin)*PAGESIZE,0);
    read(fd,buffer,PAGESIZE);
    lseek(fd,(dframe-framesbegin)*PAGESIZE,0);
    write(fd,buffer,PAGESIZE);
	close(fd);
	
    return(1);
}

int writeblock(char *buffer, int dblock)
{
	int fd;
	fd=open("swap",O_RDWR);
	usleep(200000);
    lseek(fd,(dblock-framesbegin)*PAGESIZE,0);
    write(fd,buffer,PAGESIZE);  // buffer tiene el contenido del marco virtual
	close(fd);
	return(1);
}

int readblock(char *buffer, int sblock)
{
	int fd;
	fd=open("swap",O_RDWR);
	usleep(200000);
    lseek(fd,(sblock-framesbegin)*PAGESIZE,0);
    read(fd,buffer,PAGESIZE);  // buffer tiene el contenido del marco virtual
	close(fd);
	return(1);
}

int loadframe(int frame)
{
	readblock(systemframetable[frame].paddress,frame);
	return(1);
}


int saveframe(int frame)
{
	writeblock(systemframetable[frame].paddress,frame);
	return(1);
}

