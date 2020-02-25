// Petr Valenta, FIT VUT 2015

#include "h2o.h"

const char *shmn[6] =
{
        "/xvalen20_counter",
        "/xvalen20_hcounter",
        "/xvalen20_ocounter",
        "/xvalen20_finish",
        "/xvalen20_bonding",
        "/xvalen20_bonded"
};  // nazev shared memory objectu

int *shm[6];  //  pole ukazatelu na promene
int mShm[6]; // pole intu pro filedescriptory z shm_open
sem_t *semaphore[9]; // pole semaforu

FILE *fp = NULL;

int max = 0;

int main(int argc, char const *argv[])
{
        semaphores(); // inicializace semaforu a promenych

        fp = fopen("h2o.out","w");
        if(fp==NULL)
        {
                clean();
                error(1, "Can't open file h2o.out");
        }

        setvbuf(fp,NULL,_IOLBF,0); // bez line bufferingu je zprehazeny output


        int arg[4];
        //arg 0   N  - pocet procesu reprezentujicich kyslik
        //arg 1   GH - max hodnota v ms, novy vodik
        //arg 2   GO - max hodnota v ms, novy kyslik
        //arg 3   B  - max hodnota v ms, bond

        if (argc != 5)
        {
                if (fp)
                        fclose(fp);
                clean();
                error(1, "Wrong argument count");
        }
        for (int i = 0; i < argc - 1; i++)
        {
                errno = 0;
                arg[i] = strtol(argv[i+1], NULL, 0);
                if (errno != 0)
                {
                        if (fp)
                                fclose(fp);
                        clean();
                        error(1, "Wrong argument");
                }
        }

        if (N < 1 || GH < 0 || GH > 5000 || GO < 0 || GO > 5000 || B < 0 || B > 5000)
        {
                if (fp)
                        fclose(fp);
                clean();
                error(1, "Argument out of bonds");
        }

        max = 3*N; // na tolik atomu cekame pri ukonceni

        // fork dvou procesu - generatoru kysliku a generatoru vodiku
        pid_t pid = fork();
        if (pid == 0)
        {
                oxygen_generator(N, GO, B);
        }
        else if (pid > 0)
        {
                pid = fork();
                if (pid == 0)
                {
                        hydrogen_generator(N, GH, B);
                }
                else if (pid < 0)
                {
                        if (fp)
                                fclose(fp);
                        clean();
                        error(2, "Hydrogen generator fork has been unsuccessful");
                }
        }
        else
        {
                if (fp)
                        fclose(fp);
                clean();
                error(2, "Oxygen generator fork has been unsuccessful");
        }

        //cekani na skonceni procesu
        wait(NULL);
        wait(NULL);
        clean();

        exit(EXIT_SUCCESS);
}


void oxygen_generator(int n, int atom_delay, int bond_delay)
{
        srand(time(NULL) & getpid()); // seed generatoru nahodnych cisel
        pid_t pid;

        for (int i = 0; i < n; i++)
        {
                usleep((rand()%(atom_delay+1)) * 1000); // delay

                pid = fork();  //fork kysliku
                if (pid == 0)
                {
                        oxygen(i+1, bond_delay);
                }
                else if (pid < 0)
                {
                        if (fp)
                                fclose(fp);
                        clean();
                        error(2, "Oxygen fork has been unsuccessful");
                }
        }

        //cekani na procesy
        while (wait(NULL) > 0);

        if (fp)
                fclose(fp);
        exit(EXIT_SUCCESS);
}

void hydrogen_generator(int n, int atom_delay, int bond_delay)
{
        srand(time(NULL)&getpid()); // seed generatoru nahodnych cisel
        pid_t pid;

        n = 2*n; // vodik = 2 * kyslik

        for (int i = 0; i < n; i++)
        {
                usleep((rand()%(atom_delay+1)) * 1000); // delay

                pid = fork(); // fork vodiku
                if (pid == 0)
                {
                        hydrogen(i+1, bond_delay);
                }
                else if (pid < 0)
                {
                        if (fp)
                                fclose(fp);
                        clean();
                        error(2, "Hydrogen fork has been unsuccessful");
                }
        }

        //cekani na procesy
        while (wait(NULL) > 0);

        if (fp)
                fclose(fp);
        exit(EXIT_SUCCESS);
}

void oxygen(int n, int bond_delay)
{
        sem_wait(semaphore[output]);
        fprintf(fp,"%d: O %d: started\n",++(*shm[counter]), n);
        sem_post(semaphore[output]);

        sem_wait(semaphore[obarrier]); //bariera pro kysliky, uzavre se po pruchodu jednim kyslikem, otevre se po bondingu
        sem_wait(semaphore[mutex]);

        (*shm[ocounter])++;
        if (*shm[hcounter] >= 2)
        {
                sem_wait(semaphore[output]);
                fprintf(fp,"%d: O %d: ready\n",++(*shm[counter]), n);
                sem_post(semaphore[output]);

                sem_post(semaphore[hydroQueue]);
                (*shm[hcounter]) -= 1;
                sem_post(semaphore[hydroQueue]);
                (*shm[hcounter]) -= 1;
                sem_post(semaphore[oxyQueue]);
                (*shm[ocounter]) -= 1;
        }
        else
        {
                sem_wait(semaphore[output]);
                fprintf(fp,"%d: O %d: waiting\n",++(*shm[counter]), n);
                sem_post(semaphore[output]);

                sem_post(semaphore[mutex]);
        }

        sem_wait(semaphore[oxyQueue]);


        bond(n, 'O', bond_delay);


        sem_wait(semaphore[mutex2]);

        (*shm[bonded])++;
        if(*shm[bonded] == MOLECULE_SIZE)
        {
                sem_post(semaphore[hbarrier]);
                sem_post(semaphore[hbarrier]);
                sem_post(semaphore[obarrier]);
                *shm[bonded] = 0;
        }
        sem_post(semaphore[mutex2]);

        sem_post(semaphore[mutex]);

        wait_all();

        sem_wait(semaphore[output]);
        fprintf(fp,"%d: O %d: finished\n",++(*shm[counter]), n);
        sem_post(semaphore[output]);

        if (fp)
                fclose(fp);
        exit(EXIT_SUCCESS);
}



void hydrogen(int n, int bond_delay)
{

        sem_wait(semaphore[output]);
        fprintf(fp,"%d: H %d: started\n",++(*shm[counter]), n);
        sem_post(semaphore[output]);

        sem_wait(semaphore[hbarrier]);
        sem_wait(semaphore[mutex]);

        (*shm[hcounter])++;
        if (*shm[hcounter] >= 2 && *shm[ocounter] >= 1)
        {

                sem_wait(semaphore[output]);
                fprintf(fp,"%d: H %d: ready\n",++(*shm[counter]), n);
                sem_post(semaphore[output]);

                sem_post(semaphore[hydroQueue]);
                (*shm[hcounter]) -= 1;
                sem_post(semaphore[hydroQueue]);
                (*shm[hcounter]) -= 1;
                sem_post(semaphore[oxyQueue]);
                (*shm[ocounter]) -= 1;
        }
        else
        {

                sem_wait(semaphore[output]);
                fprintf(fp,"%d: H %d: waiting\n",++(*shm[counter]), n);
                sem_post(semaphore[output]);


                sem_post(semaphore[mutex]);
        }


        sem_wait(semaphore[hydroQueue]);

        bond(n, 'H', bond_delay);

        sem_wait(semaphore[mutex2]);
        (*shm[bonded])++;
        if(*shm[bonded] == MOLECULE_SIZE)
        {
                sem_post(semaphore[hbarrier]);
                sem_post(semaphore[hbarrier]);
                sem_post(semaphore[obarrier]);
                *shm[bonded] = 0;
        }
        sem_post(semaphore[mutex2]);

        wait_all();

        sem_wait(semaphore[output]);
        fprintf(fp,"%d: H %d: finished\n",++(*shm[counter]), n);
        sem_post(semaphore[output]);


        if (fp)
                fclose(fp);

        exit(EXIT_SUCCESS);
}

void wait_all()
{
        sem_wait(semaphore[mutex2]);

        (*shm[finish_ready])++;

        if (*shm[finish_ready] == max)
                for (int i = 0; i < max; i++)
                        sem_post(semaphore[finish]);

        sem_post(semaphore[mutex2]);
        sem_wait(semaphore[finish]);
}

void bond(int n, char a, int bond_delay)
{
        sem_wait(semaphore[output]);
        fprintf(fp,"%d: %c %d: begin bonding\n",++(*shm[counter]), a, n);
        sem_post(semaphore[output]);

        sem_wait(semaphore[mutex2]);
        (*shm[bonding_ready])++;
        if (*shm[bonding_ready] == MOLECULE_SIZE)
        {
                usleep((rand()%(bond_delay+1)) * 1000);

                for (int i = 0; i < MOLECULE_SIZE; i++)
                {
                        sem_post(semaphore[begin_barrier]);
                        //sem_post(barrier);
                        (*shm[bonding_ready])--;

                }
        }
        sem_post(semaphore[mutex2]);

        sem_wait(semaphore[begin_barrier]);

        sem_wait(semaphore[output]);
        fprintf(fp,"%d: %c %d: bonded\n",++(*shm[counter]), a, n);
        sem_post(semaphore[output]);


}

int error(int errn, char * errmsg)
{
        fprintf(stderr, "ERROR %d: %s\n", errn, errmsg);
        exit(errn);
}

void clean()
{
        int scontrol = 0;
        int mcontrol = 0;

        for(int i = 0; i < SEM_COUNT; i++)
        {
                if(munmap(semaphore[i],sizeof(sem_t)) < 0)
                        scontrol = 1;

                if(sem_destroy(semaphore[i]) < 0)
                        scontrol = 1;
        }


        for(int i = 0; i < MEM_COUNT; i++)
        {
                if(munmap(shm[i], sizeof(int)) < 0)
                        mcontrol = 2;

                if(shm_unlink(shmn[i]) < 0)
                        mcontrol = 2;

                if(close(mShm[i]) < 0)
                        mcontrol = 2;
        }

        if (fp)
                if (fclose(fp) != 0)
                        error(2, "Can't close file h2o.out");

        if (scontrol)
                error(2, "Can't unmap semaphores");

        if (mcontrol)
                error(2, "Can't unmap variables");
}

void semaphores()
{
        int initValue;
        int scontrol = 0;
        int mcontrol = 0;

        for(int i=0; i<SEM_COUNT; i++)
        {
                if((semaphore[i] = mmap(NULL,sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED,0,0)) == MAP_FAILED)
                        scontrol = 1;
                if (i == hbarrier)
                {
                        initValue=2;
                }
                else if (i == oxyQueue || i == hydroQueue || i == begin_barrier || i == finish)
                {
                        initValue=0;
                }
                else initValue=1;
                if(sem_init(semaphore[i],1,initValue) < 0)
                        scontrol = 1;
        }

        for(int i=0; i<MEM_COUNT; i++)
        {
                if((mShm[i]=shm_open(shmn[i], O_RDWR|O_CREAT, 0600)) < 0)
                        mcontrol = 2;
                if(ftruncate(mShm[i],sizeof(int)) < 0)
                        mcontrol = 2;
                if((shm[i]= mmap(NULL,sizeof(int),PROT_READ|PROT_WRITE,MAP_SHARED,mShm[i],0)) == MAP_FAILED)
                        mcontrol = 2;
                *shm[i]=0;
        }

        if (scontrol)
                error(2,"Can't initialize semaphores");
        if (mcontrol)
                error(2,"Can't initialize global variables");

}
