I=include
B=bin
S=sources
L=lib
T=todo

all: $B/procesos $B/procesosVM $B/createswap

$B/procesos: $S/procesos.c $L/mmu.o $L/pagefault.o
	gcc -o $B/procesos $S/procesos.c $L/mmu.o $L/pagefault.o -I$I

$B/procesosVM: $S/procesos.c $L/mmu.o $L/pagefaultVM.o
	gcc -o $B/procesosVM $S/procesos.c $L/mmu.o $L/pagefaultVM.o -I$I

$L/pagefault.o: $T/pagefault.c $I/mmu.h
	gcc -o $L/pagefault.o -c $T/pagefault.c -I$I

$L/pagefaultVM.o: $T/pagefaultVM.c $I/mmu.h
	gcc -o $L/pagefaultVM.o -c $T/pagefaultVM.c -I$I

$L/mmu.o: $S/mmu.c $I/semaphores.h $I/mmu.h
	gcc -o $L/mmu.o -c $S/mmu.c -I$I

$B/createswap: $S/createswap.c
	gcc -o $B/createswap $S/createswap.c -I$I
		
clean:
	rm $B/createswap $L/mmu.o $L/pagefault.o $L/pagefaultVM.o $B/procesos $B/procesosVM $B/swap

