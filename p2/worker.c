#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
    int shmid;    //shared memory id
    key_t shmkey; //shared memory key
    int *clock;
    int duration = 0;
    int time_out[2];
    int ch_pid;

    ch_pid = getpid();

    time_out[0] = 0;
    time_out[1] = 0;

    duration = atoi(argv[1]);
    shmkey = 110594;

    //create 512 bytes of shared memory. 0775 is permissions like chmod
    shmid = shmget(shmkey, 2, IPC_CREAT | 0775);

    //shmid < 0 implies error so perror and exit
    if (shmid < 0)
    {
        perror("shmget");
        exit(1);
    }
    clock = (int *)shmat(shmid, NULL, 0);

    time_out[0] = clock[0];
    time_out[1] = clock[1] + duration;

    if (time_out[1] >= 1000000000)
    {
        time_out[0] = time_out[0] + 1;
        time_out[1] = time_out[1] - 1000000000;
    }

    while (1)
    {
        if (clock[0] >= time_out[0] && clock[1] >= time_out[1])
        {
            printf("PID:%6d Clock:%3ds %10dns terminated\n", ch_pid, clock[0], clock[1]);
            shmdt(clock);
            exit(1);
        }
    }

    return 0;
}