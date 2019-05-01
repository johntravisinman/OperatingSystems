#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "times.h"

void fork_handler(int n, int s, char *in_fname, char *out_fname, char *target_name);
void help_message();
void check_args(int *n, int *s);
static void alarm_handler();
struct times read_file_to_times(FILE* in);
FILE *my_file_open(char *filename, char *options, char *error_stmt);

FILE *OUT_FILE;

int main(int argc, char *argv[])
{
    int opt;     //getopt option
    int n = 4;   //hold n opting value set to default
    int s = 2;   //hold s option value set to default
    //Input and output filenames set to defaults
    char *in_fname = "input.dat";
    char *out_fname = "output.dat";

    printf("\n");
    // No arguments given
    if (argc < 2)
    {
        printf("No arguments given\n");
        printf("Using default filenames\n");
        printf("\n");
    }
    // getopt loop
    while ((opt = getopt(argc, argv, "hi:o:n:s:")) != -1)
    {
        switch (opt)
        {
        case 'h': //Help Message
            help_message();
            return 0;
        case 'i': //Input filename given
            in_fname = optarg;
            break;
        case 'o': //Output filename given
            out_fname = optarg;
            break;
        case 'n': //n argument given
            n = atoi(optarg);
            break;
        case 's': //s argument given
            s = atoi(optarg);
            break;
        default:
            printf("No arguments given\n");
            printf("Using default filenames\n");
            printf("\n");
            break;
        }
    }
    //check values of n and s
    check_args(&n, &s);
    // Displaying filenames and options
    printf("Input File:  %s\n", in_fname);
    printf("Output File: %s\n", out_fname);
    printf("\n");
    printf("n = %d\n", n);
    printf("s = %d\n", s);
    printf("\n");

    fork_handler(n, s, in_fname, out_fname, argv[0]);

    return 0;
}

void fork_handler(int n, int s, char *in_fname, char *out_fname, char *target_name)
{
    pid_t pid; //process id returned from fork()
    pid_t wait_id; //process id returned from waitpid()
    //Array of start times. clock is defined in clock.h
    struct times start_time;
    int *clock;    //will replace shared_mem as an array of 2 ints for seconds and nanoseconds
    int clock_inc; //clock increment
    int ps_total = 0; //total child processes created
    int ps_running = 0; //count of running processes
    int ps_terminated = 0; //count of terminated processes

    FILE *in_file;
    char file_buffer[256];

    int shmid;             //shared memory id
    key_t shmkey = 110594; //shared memory key

    //build error string for perror or printing
    char error_stmt[64];
    strcpy(error_stmt, target_name);
    strcat(error_stmt, ": Error");
    // Opening files
    in_file = my_file_open(in_fname, "r", error_stmt);
    OUT_FILE = my_file_open(out_fname, "a", error_stmt);
    //create 2 bytes of shared memory. 0775 is permissions like chmod
    shmid = shmget(shmkey, 2, IPC_CREAT | 0775);
    //shmid < 0 implies error so perror and exit
    if (shmid < 0)
    {
        perror("shmget");
        exit(1);
    }
    //attach to shared memory
    clock = (int *)shmat(shmid, NULL, 0);
    clock[0] = 0;
    clock[1] = 0;
    
    //Grab first line of file and set clock increment
    fgets(file_buffer, sizeof(file_buffer), in_file);
    clock_inc = atoi(file_buffer);
    //Grab first set of start times and duration
    start_time = read_file_to_times(in_file);

    //call alarm_handler after 2 seconds
    signal(SIGALRM, alarm_handler);
    signal(SIGINT, alarm_handler);
    alarm(2); //designates that it should be 2 seconds
    
    while (n > ps_terminated)//loop until all n children have been forked and terminated
    {
        //Increment clock
        clock[1] += clock_inc;
        if (clock[1] > 999999999)
        {
            clock[0] += 1;
            clock[1] -= 1000000000;
        }
        //If it is past or at the start time for the next process...
        if(clock[0] >= start_time.s && clock[1] >= start_time.ns)
        {
            //...and we have called less than n total children have have less than s children currently
            if(ps_total < n && ps_running < s)
            {
                //fork off a child
                pid = fork();
                if (pid < 0) //fork() returns < 0 indicates error
                {
                    perror("Fork Error");
                    exit(EXIT_FAILURE);
                }
                else if (pid == 0) //child process
                {
                    execl("./worker", "worker", start_time.dur, out_fname, (char *)NULL);
                }
                fprintf(OUT_FILE, "PID:%6d Clock:%3ds %10dns dur: %sns\n", pid, clock[0], clock[1], start_time.dur);
                ps_total++;
                start_time = read_file_to_times(in_file);
	        if(start_time.s == -1 && start_time.ns == -1)
                {
                    n = ps_total;
                }
            }
        }
        wait_id = waitpid(-1, NULL, WNOHANG);
        if(wait_id > 0)
        {
            fprintf(OUT_FILE, "PID:%6d Clock:%3ds %10dns terminated\n", wait_id, clock[0], clock[1]);
            ps_terminated++;
        }
        
    }
    //print the clock
    fprintf(OUT_FILE, "Finished:%3ds %10dns\n", clock[0], clock[1]);
    /*CLEANUP*/
    shmdt(clock);                  //detach from shared memory
    shmctl(shmid, IPC_RMID, NULL); //delete shared memory
    fclose(in_file);               //close files
    fclose(OUT_FILE);
}

void help_message()
{
    printf("This program takes the following possible arguments\n");
    printf("\n");
    printf("  -h           : to display this help message\n");
    printf("  -i filename  : to designate the input file\n");
    printf("  -o filename  : to designate the output file\n");
    printf("  -n x         : to designate the max total number of child processes\n");
    printf("  -s x         : to designate the number of child processes that should exist at one time\n");
    printf("\n");
    printf("If no arguments are given then the following default values are used:\n");
    printf("\n");
    printf("  Input File:  \'input.dat\'\n");
    printf("  Output File: \'output.dat\'\n");
    printf("  n = 4\n");
    printf("  s = 2\n");
    printf("\n");
    return;
}

void check_args(int *n, int *s)
{
    int valid = 1;
    if (*n <= 0)
    {
        printf("n argument should be greater than 0\n");
        valid = -1;
    }
    if (*s <= 0)
    {
        printf("s argument should be greater than 0\n");
        valid = -1;
    }
    if (valid < 0)
    {
        exit(EXIT_FAILURE);
    }
    if(*n > 20)
    {
        printf("n argument should be no greater 20\n");
        printf("setting n = 20\n");
        printf("\n");
        *n = 20;
    }
    if(*s > 20)
    {
        printf("s argument should be no greater 20\n");
        printf("setting s = 20\n");
        printf("\n");
        *s = 20;
    }
    return;
}

static void alarm_handler()
{
    key_t shmkey = 110594; //shared memory key
    int shmid;             //shared memory id
    int *clock; //pointer to shared memory
    shmid = shmget(shmkey, 2, IPC_CREAT | 0775);
    clock = (int *)shmat(shmid, NULL, 0);
    fprintf(OUT_FILE, "Killed:%3ds %10dns\n", clock[0], clock[1]);
    fclose(OUT_FILE);
    shmctl(shmid, IPC_RMID, NULL); //delete shared memory
    kill(0, SIGKILL);
    exit(EXIT_SUCCESS);
}

struct times read_file_to_times(FILE* in)
{
    char file_buf[128];
    char* tok;
    struct times start_time = {-1, -1, "-1"};

    if(fgets(file_buf, sizeof(file_buf), in) != NULL)
    {
        tok = strtok(file_buf, " ");
        start_time.s = atoi(tok);
        tok = strtok(NULL, " ");
        start_time.ns = atoi(tok);
        tok = strtok(NULL, " ");
        if(isspace(tok[strlen(tok) - 1])) //remove trailing newline from file
        {                                     //prevent duration = "123456\n"
            tok[strlen(tok) - 1] = '\0';  //want duration = "123456"
        }
        strcpy(start_time.dur, tok);
    }
    return start_time;
}

FILE *my_file_open(char *filename, char *options, char *error_stmt)
{
    FILE *fp = fopen(filename, options);
    if (fp == NULL) //Handle error opening file
    {
        printf("Filename: \'%s\'\n", filename);
        perror(error_stmt);
        printf("\n");
        exit(EXIT_FAILURE);
    }
    return fp;
}
