// Petr Valenta, FIT VUT 2015

#ifndef H2OH
#define  H2OH


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h> // O_RDWR
#include <time.h> // time()
#include <sys/wait.h> // wait()


#define N  arg[0]
#define GH arg[1]
#define GO arg[2]
#define B  arg[3]

#define MOLECULE_SIZE 3


#define SEM_COUNT 9
enum semaphores
{
        mutex,
        mutex2,
        finish,
        oxyQueue,
        hydroQueue,
        obarrier,
        hbarrier,
        begin_barrier,
        output,
};

#define MEM_COUNT 6
enum sharedmemory
{
        counter,
        hcounter,
        ocounter,
        finish_ready,
        bonding_ready,
        bonded,
};


int error(int errn, char * errmsg);
// ukonci program s chybovou hodnotou errn a vypise chybovou hlasku errmsh

void oxygen_generator(int n, int atom_delay, int bond_delay);
// generuje n procesu oxygen

void hydrogen_generator(int n, int atom_delay, int bond_delay);
// generuje n procesu hydrogen

void oxygen(int n, int bond_delay);
//

void hydrogen(int n, int bond_delay);
//

void wait_all();
// ceka na ukonceni ostatnich procesu atomu

void clean();
// dealokace sdilene pameti, semaforu a zavreni souboru

void bond(int n, char a, int bond_delay);
// ceka na begin_bonding u ostatnich atomu z molekuly (aby nemohlo predchazet bonded begin bondingu)

void semaphores();
// alokace sdilene pameti, semaforu

#endif
