#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "simtime.h"
#include "memorymanagement.h"

FILE *logFile;

int simClockID;
const int SIM_CLOCK_KEY = 110197;
int msqid;
const int MSG_Q_KEY = 052455;

void oss(simtime_t *simClock, int maxActive);
void init_mem_management(int *pids, frame_t *frameTable, pagetable_t *pageTables, int maxActive);
void handle_args(int argc, char* argv[], int *maxActive);
static void time_out();
void cleanup();
FILE *open_file(char *fname, char *opts, char *error);
simtime_t *get_shared_simtime(int key);
void create_msqueue();
int get_simPid(int *pids, int size);

int main(int argc, char *argv[])
{
  int maxActive = 18;
  simtime_t *simClock;
  //Seed rand
  srand(time(0));
  //Two second time out
  signal(SIGALRM, time_out);
  alarm(2);
  //Handle command line arguments
  handle_args(argc, argv, &maxActive);
  //Set up shared simulated clock
  simClock = get_shared_simtime(SIM_CLOCK_KEY);
  simClock->s = 0;
  simClock->ns = 0;

  oss(simClock, maxActive);

  return 0;
}

void oss(simtime_t *simClock, int maxActive) {
  
  /*****SETUP*****/
  
  int i, j;//loop iterators
  int activeProcesses = 0;//counter of active processes 
  simtime_t nextSpawn = { .s = 0, .ns = 0 };//simulated time for next spawn
  //Array of taken sim pids. index = simpid, value at index = actual pid
  int *pids = (int *)malloc(sizeof(int) * maxActive);
  // Frame Table. array of frames
  frame_t *frameTable = (frame_t *)malloc(sizeof(frame_t) * TOTAL_MEMORY);
  // Array of page tables. one page table for each process
  pagetable_t *pageTables = (pagetable_t *)malloc(sizeof(pagetable_t) * maxActive);
  // Initialize the array of pids and memory management structures
  init_mem_management(pids, frameTable, pageTables, maxActive);
  // Set up the message queue
  create_msqueue();
  
  /*****OSS*****/

  fprintf(logFile, "Begin OS Simulation\n");
  // while(1) {
  //   //If oss should spawn a new child process
  //   if (activeProcesses < maxActive && less_or_equal_sim_times(nextSpawn, *simClock) == 1) {
  //     //Spawn
      
  //   }
    
  // }
  
  // Should never get here in final version but clean up just in case
  // Free pids
  free(pids);
  // Free tables
  free(frameTable);
  free(pageTables);
  // Close log
  fclose(logFile);
  // Delete sim clock
  shmctl(simClockID, IPC_RMID, NULL);
  // Delete message queue
  msgctl(msqid, IPC_RMID, NULL);
  return;
}

// Initialize the array of pids and memory management structures
void init_mem_management(int *pids, frame_t *frameTable, pagetable_t *pageTables, int maxActive) {
  int i, j;
  // Initialize all pids to available ( = -1)
  for(i = 0; i < maxActive; i++) {
    pids[i] = -1;
  }
  // Initialize frame table
  for( i = 0; i < TOTAL_MEMORY; i++) {
    frameTable[i].ref = 0x0;
    frameTable[i].dirty = 0x0;
    frameTable[i].pid = -1;//indicates frame is empty
  }
  // Initialize page tables
  for( i = 0; i < maxActive; i ++) {
    for( j = 0; j < PROCESS_SIZE; j++) {
      pageTables[i].pages[j].framePos = -1;
    }
  }
}

//Handle command line arguments using getopt
void handle_args(int argc, char *argv[], int *maxActive) {
  int opt;
  char *logName = "log.txt";
  if (argc < 2) {
    printf("No arguments given\n");
    printf("Using default values\n");
  }
  while ((opt = getopt(argc, argv, "hn:o:")) != -1) {
    switch (opt) {
      case 'h':
        printf("This program takes the following possible arguments\n");
        printf("\n");
        printf("  -h           : to display this help message\n");
        printf("  -n x         : x = maximum concurrent child processes\n");
        printf("  -o filename  : to specify log file\n");
        printf("\n");
        printf("Defaults:\n");
        printf("  Log File: log.txt\n");
        printf("         n: 18\n");
        printf("  Non-verbose printing\n");
        exit(EXIT_SUCCESS);
      case 'n':
        *maxActive = atoi(optarg);
        break;
      case 'o':
        logName = optarg;
        break;
      default:
        printf("No arguments given\n");
        printf("Using default values\n");
        break;
    }
  }
  if (*maxActive > 18) {
    printf("n must be <= 18\n");
    exit(EXIT_SUCCESS);
  } else if (*maxActive <= 0) {
    printf("n must be >= 1\n");
    exit(EXIT_SUCCESS);
  }
  printf("Log File: %s\n", logName);
  printf("       n: %d\n", *maxActive);
  // Open log file
  logFile = open_file(logName, "w", "./oss: Error: ");
  return;
}

//SIGALRM handler
static void time_out() {
  fprintf(stderr, "Timeout\n");
  cleanup();
  exit(EXIT_SUCCESS);
}

//clean up for abnormal termination
void cleanup() {
  fclose(logFile);
  shmctl(simClockID, IPC_RMID, NULL);
  msgctl(msqid, IPC_RMID, NULL);
  kill(0, SIGTERM);
  exit(EXIT_SUCCESS);
}

/*fopen with simple error check*/
FILE *open_file(char *fname, char *opts, char *error) {
  FILE *fp = fopen(fname, opts);
  if (fp == NULL) {  // error opening file
    perror(error);
    cleanup();
  }
  return fp;
}

//return a pointer to simtime_t object in shared memory
simtime_t *get_shared_simtime(int key) {
  simtime_t *simClock;
  simClockID = shmget(key, sizeof(simtime_t), IPC_CREAT | 0777);
  if (simClockID < 0) {
    perror("./oss: Error: shmget for simulated clock");
    cleanup();
  }
  simClock = shmat(simClockID, NULL, 0);
  if (simClock < 0) {
    perror("./oss: Error: shmat for simulated clock");
    cleanup();
  }
  return simClock;
}

//set the msqid
void create_msqueue() {
  msqid = msgget(MSG_Q_KEY, 0666 | IPC_CREAT);
  if (msqid < 0) {
    perror("./oss: Error: msgget ");
    cleanup();
  }
  return;
}

//return index of free spot in pids array, -1 if no spots
int get_simPid(int *pids, int size) {
  int i;
  for (i = 0; i < size; i++)
    if (pids[i] == -1) {
      return i;
    }
  return -1;
}