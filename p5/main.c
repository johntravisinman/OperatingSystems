#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "msg.h"
#include "oss.h"
#include "resourcedescriptor.h"
#include "simtime.h"

static void time_out();
void oss(int maxConcurrent);
void handle_args(int argc, char *argv[], int *n);
FILE *open_file(char *fname, char *opts, char *error);
void cleanup();
resource_descriptor_t *get_shared_resource_descriptor();
simtime_t *get_shared_simtime();
void create_msqueue();
simtime_t get_next_process_time(simtime_t max, simtime_t currentTime);
int get_available_pid_index(int *pids, int size);
int spawn(char arg1[], char arg2[], int *running);
void send_msg(int dest, int action);
void handle_msg(msg_t msg, resource_descriptor_t *resourceDescriptor, simtime_t *simClock, int *pids, int *active, int *lines);
int deadlock(resource_descriptor_t *resourceDescriptor, int processes, simtime_t *simClock, int *pids, int *active, int *lines);
int req_lt_avail(int req[], int avail[], int shared[], int held[]);
void terminate_and_release(resource_descriptor_t *resourceDescriptor, int simPid, int *pids, int *active);

int main(int argc, char *argv[]) {
  int n = 18;
  handle_args(argc, argv, &n);
  srand(time(0));
  signal(SIGALRM, time_out);
  alarm(2);
  oss(n);
  return 0;
}

/**********************************  OSS  ****************************************/
void oss(int maxConcurrent) {
  //Shared Memory Structures
  simtime_t *simClock;
  resource_descriptor_t *resourceDescriptor;
  // Message Queue buffer
  msg_t msg;
  //Array of pids where the index of the pid is that pid's sim pid
  int *pids;
  //Exec arguments
  char msqidArg[10];
  char simPidArg[3];
  //Next time to spawn a process
  simtime_t nextProcess = {.s = 0, .ns = 0};  //spawn first process immediately
  //counter of active processes spawned by oss that are in the system
  int activeProcesses = 0;
  //Clock Increment
  int clockInc = 1000000;  // 1 ms = 1,000,000 ns
  //loop iterator
  int i;
  //Counter of lines written to the log file
  int lines = 0;
  //counter of resource descriptor prints
  int prints = 0;
  //whether or not the system detected deadlock;
  int deadlocked = FALSE;

  /*          Setup          */
  //Shared Memory
  simClock = get_shared_simtime();
  simClock->s = 0;
  simClock->ns = 0;
  resourceDescriptor = get_shared_resource_descriptor();
  init_resource_descriptor(resourceDescriptor);
  //Message Queue
  create_msqueue();
  sprintf(msqidArg, "%d", msqid);  //write msqid to string for exec arg
  //init pids array
  pids = (int *)malloc(sizeof(int) * maxConcurrent);
  for (i = 0; i < maxConcurrent; i++)
    pids[i] = -1;
  /*Begin Simulation and Resource Management*/
  prints++;
  lines += 9 + 2 * maxConcurrent;
  print_shared_vector(logFile, "Shared Resources", resourceDescriptor->sharedResourceVector, MAX_RESOURCES);
  print_resource_descriptor(logFile, (*resourceDescriptor), MAX_RESOURCES, maxConcurrent);
  //infinite loop that will only terminate using SIGALRM
  while (1) {
    //If there are less active processes than the maximum concurrent processes AND
    //it is time to schedule a new process (nextProcess <= simClock)...
    if (activeProcesses < maxConcurrent && less_or_equal_sim_times(nextProcess, (*simClock)) == 1) {
      //...spawn a process
      int simPid;
      int pid;
      simPid = get_available_pid_index(pids, maxConcurrent);
      if (simPid < 0) {  //Error
        fprintf(logFile, "%d.%09ds ./oss: Error: No available simPids\n", simClock->s, simClock->ns);
        fprintf(stderr, "./oss: Error: No available simPids\n");
        cleanup();
      }
      //write simpid to string for exec arg
      sprintf(simPidArg, "%d", simPid);
      //spawn a process and increment active processes counter
      pid = spawn(msqidArg, simPidArg, &activeProcesses);
      //store pid based on simPid
      pids[simPid] = pid;
      //schedule next process spawn time
      nextProcess = get_next_process_time((simtime_t){0, 500000000}, (*simClock));
      //Verbose logging
      if (verbose == TRUE && lines < 100000) {
        fprintf(logFile, "%d.%09ds ./oss: Spawned new process p%d\n", simClock->s, simClock->ns, simPid);
        fprintf(logFile, "%d.%09ds ./oss: Next process scheduled for %d.%09ds\n", simClock->s, simClock->ns, nextProcess.s, nextProcess.ns);
        lines += 2;
      }
    }
    //If there are any messages for oss in the queue
    else if ((msgrcv(msqid, &msg, sizeof(msg_t), 1, IPC_NOWAIT)) > 0) {
      //printf("rcv\n.mtype = %ld  .rid = %d  .action = %d  .pid = %d  .sender = %d\n\n", msg.mtype, msg.rid, msg.action, msg.pid, msg.sender);
      handle_msg(msg, resourceDescriptor, simClock, pids, &activeProcesses, &lines);
    }
    //weird calculation to print the state of the system every 20 printed lines but not including
    //lines filled with resource descriptors
    //calulation is because printing resource descriptor has large variation in the amount of lines
    //it writes so this keeps it consistent
    if ((lines - (9 + 2 * maxConcurrent) * prints) / 20 >= prints && lines < 100000) {
      prints++;
      lines += 9 + 2 * maxConcurrent;
      print_resource_descriptor(logFile, (*resourceDescriptor), MAX_RESOURCES, maxConcurrent);
    }
    //Every simulated second check for and handle deadlock
    if (simClock->ns == 0) {
      deadlocked = deadlock(resourceDescriptor, maxConcurrent, simClock, pids, &activeProcesses, &lines);
      //If a deadlock occurred print the system state
      if (deadlocked == TRUE && lines < 100000) {
        prints++;
        lines += 9 + 2 * maxConcurrent;
        print_resource_descriptor(logFile, (*resourceDescriptor), MAX_RESOURCES, maxConcurrent);
      }
    }

    // Increment the simulated clock
    increment_sim_time(simClock, clockInc);
  }  //END WHILE(1)
  //never get here
  return;
}
/********************************  END OSS  **************************************/

//SIGALRM handler
static void time_out() {
  fprintf(stderr, "Timeout\n");
  cleanup();
  exit(EXIT_SUCCESS);
}

//Handle command line args using get opt
void handle_args(int argc, char *argv[], int *n) {
  int opt;
  char *logName = "log.txt";
  if (argc < 2) {
    printf("No arguments given\n");
    printf("Using default values\n");
  }
  while ((opt = getopt(argc, argv, "hn:o:v")) != -1) {
    switch (opt) {
      case 'h':
        printf("This program takes the following possible arguments\n");
        printf("\n");
        printf("  -h           : to display this help message\n");
        printf("  -n x         : x = maximum concurrent child processes\n");
        printf("  -o filename  : to specify log file\n");
        printf("  -v           : to have verbose printing\n");
        printf("\n");
        printf("Defaults:\n");
        printf("  Log File: log.txt\n");
        printf("         n: 18\n");
        printf("  Non-verbose printing\n");
        exit(EXIT_SUCCESS);
      case 'n':
        *n = atoi(optarg);
        break;
      case 'o':
        logName = optarg;
        break;
      case 'v':
        verbose = TRUE;
        break;
      default:
        printf("No arguments given\n");
        printf("Using default values\n");
        break;
    }
  }
  if (*n > 18) {
    printf("n must be <= 18\n");
    exit(EXIT_SUCCESS);
  } else if (*n <= 0) {
    printf("n must be >= 1\n");
    exit(EXIT_SUCCESS);
  }
  printf("Log File: %s\n", logName);
  printf("       n: %d\n", *n);
  if (verbose)
    printf("Verbose Printing\n");
  else
    printf("Non-Verbose Printing\n");
  // Open log file
  logFile = open_file(logName, "w", "./oss: Error: ");
  fprintf(logFile, "Begin OS Simulation\n");
  return;
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

//clean up for abnormal termination
void cleanup() {
  fclose(logFile);
  shmctl(descriptorID, IPC_RMID, NULL);
  shmctl(simClockID, IPC_RMID, NULL);
  msgctl(msqid, IPC_RMID, NULL);
  kill(0, SIGTERM);
  exit(EXIT_SUCCESS);
}

//Return a pointer to a resource descriptor in shared memory
resource_descriptor_t *get_shared_resource_descriptor() {
  resource_descriptor_t *resourceDescriptor;
  descriptorID = shmget(DESCRIPTOR_KEY, sizeof(resource_descriptor_t), IPC_CREAT | 0777);
  if (descriptorID < 0) {
    perror("./oss: Error: shmget for resource descriptor");
    cleanup();
  }
  resourceDescriptor = shmat(descriptorID, NULL, 0);
  if (resourceDescriptor < 0) {
    perror("./oss: Error: shmat for resource descriptor");
    cleanup();
  }
  return resourceDescriptor;
}

//return a pointer to simtime_t object in shared memory
simtime_t *get_shared_simtime() {
  simtime_t *simClock;
  simClockID = shmget(SIM_CLOCK_KEY, sizeof(simtime_t), IPC_CREAT | 0777);
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

//Get a random time that the next process should spawn
//returns a simClock + random time between 0 and given max simtime
simtime_t get_next_process_time(simtime_t max, simtime_t currentTime) {
  simtime_t nextTime = {.ns = (rand() % (max.ns + 1)) + currentTime.ns,
                        .s = (rand() % (max.s + 1)) + currentTime.s};
  if (nextTime.ns >= 1000000000) {
    nextTime.s += 1;
    nextTime.ns -= 1000000000;
  }
  return nextTime;
}

//return index of free spot in pids array, -1 if no spots
int get_available_pid_index(int *pids, int size) {
  int i;
  for (i = 0; i < size; i++)
    if (pids[i] == -1) {
      return i;
    }
  return -1;
}

//spawn a process with the arg and increment running process counter
//essentially just fork() and execl()
int spawn(char arg1[], char arg2[], int *active) {
  int pid = fork();
  if (pid < 0) {  // error
    perror("./oss: Error: fork ");
    cleanup();
  } else if (pid == 0) {  // child
    execl("./user", "user", arg1, arg2, (char *)NULL);
  }
  (*active)++;
  return pid;
}

//Send a message to the process with the given pid (dest)
void send_msg(int dest, int action) {
  //only two important parts of an oss to child message...
  //the destination (pid of receiving process)
  //and the action sent to them (denied or granted)
  msg_t msg = {.mtype = dest,
               .action = action,
               .pid = -1,     //oss has no sim pid
               .rid = -1,     //msg not relavent to a specific resource
               .sender = 1};  //sender is oss so 1, for children it is real pid
  if (msgsnd(msqid, &msg, sizeof(msg_t), 0) == -1) {
    perror("./oss: Error: msgsnd ");
    cleanup();
  }
  //printf("./oss: snd\n.mtype = %ld  .rid = %d  .action = %d  .pid = %d  .sender = %d\n\n", msg.mtype, msg.rid, msg.action, msg.pid, msg.sender);
  return;
}

//Handle a message in the queue (request, release, terminate)
void handle_msg(msg_t msg, resource_descriptor_t *resourceDescriptor, simtime_t *simClock, int *pids, int *active, int *lines) {
  /*REQUEST*/
  if (msg.action == request) {
    //Verbose logging
    if (verbose == TRUE && (*lines) < 100000) {
      fprintf(logFile, "%d.%09ds ./oss: p%d requests r%d\n", simClock->s, simClock->ns, msg.pid, msg.rid);
      (*lines)++;
    }
    /*REQUEST FOR SHARED RESOURCE*/
    if (resourceDescriptor->sharedResourceVector[msg.rid] == 1) {
      //if not holding too many hsared resources already
      if (resourceDescriptor->allocationMatrix[msg.pid][msg.rid] < 5) {
        resourceDescriptor->allocationMatrix[msg.pid][msg.rid] += 1;
        //Tell requester that request was granted
        send_msg(msg.sender, granted);
        //Verbose logging
        if (verbose == TRUE && (*lines) < 100000) {
          fprintf(logFile, "%d.%09ds ./oss: p%d granted r%d\n", simClock->s, simClock->ns, msg.pid, msg.rid);
          (*lines)++;
        }
      } else { //requesting more shared resources than allowed so it deadlocked itself
        resourceDescriptor->requestMatrix[msg.pid][msg.rid] += 1;
        //Tell requester that request was denied
        send_msg(msg.sender, denied);
        //Verbose logging
        if (verbose == TRUE && (*lines) < 100000) {
          fprintf(logFile, "%d.%09ds ./oss: p%d denied r%d\n", simClock->s, simClock->ns, msg.pid, msg.rid);
          (*lines)++;
        }
      }
    } else if (resourceDescriptor->allocationVector[msg.rid] > 0) {
      /*Request for non-shareable resource and there is enough available*/
      resourceDescriptor->allocationVector[msg.rid] -= 1;
      resourceDescriptor->allocationMatrix[msg.pid][msg.rid] += 1;
      //Tell requester that request was granted
      send_msg(msg.sender, granted);
      //Verbose logging
      if (verbose == TRUE && (*lines) < 100000) {
        fprintf(logFile, "%d.%09ds ./oss: p%d granted r%d\n", simClock->s, simClock->ns, msg.pid, msg.rid);
        (*lines)++;
      }
    } else {
      /*Request for non-shareable resource and there is NOT enough available*/
      resourceDescriptor->requestMatrix[msg.pid][msg.rid] += 1;
      //Tell requester that request was denied
      send_msg(msg.sender, denied);
      //Verbose logging
      if (verbose == TRUE && (*lines) < 100000) {
        fprintf(logFile, "%d.%09ds ./oss: p%d denied r%d\n", simClock->s, simClock->ns, msg.pid, msg.rid);
        (*lines)++;
      }
    }
  } else if (msg.action == release) {
    if (resourceDescriptor->allocationMatrix[msg.pid][msg.rid] > 0) {
      //If not a shared resource
      if(resourceDescriptor->sharedResourceVector[msg.rid] == 0) {
        resourceDescriptor->allocationVector[msg.rid] += 1;
      }
      resourceDescriptor->allocationMatrix[msg.pid][msg.rid] -= 1;
      //Tell releaser that its release was granted
      send_msg(msg.sender, granted);
      //Verbose logging
      if (verbose == TRUE && (*lines) < 100000) {
        fprintf(logFile, "%d.%09ds ./oss: p%d released r%d\n", simClock->s, simClock->ns, msg.pid, msg.rid);
        (*lines)++;
      }
    } else {
      //should never get here
      send_msg(msg.sender, denied);
    }
  } else if (msg.action == terminate) {
    /*PROCESS WANTS TO TERMINATE*/
    int pid;
    int i;
    //update resource descriptor to show that all resources are released
    for (i = 0; i < MAX_RESOURCES; i++) {
      //add held resources back to available
      if (resourceDescriptor->sharedResourceVector[i] == 0) {
        resourceDescriptor->allocationVector[i] += resourceDescriptor->allocationMatrix[msg.pid][i];
      }
      resourceDescriptor->allocationMatrix[msg.pid][i] = 0;
      //should be 0 anyway but set requests to 0 just in case
      resourceDescriptor->requestMatrix[msg.pid][i] = 0;
    }
    //set pid to available
    pids[msg.pid] = -1;
    //decrement active process counter
    (*active) -= 1;
    pid = waitpid(msg.sender, NULL, 0);
    //Verbose logging
    if (verbose == TRUE && (*lines) < 100000) {
      fprintf(logFile, "%d.%09ds ./oss: p%d terminated\n", simClock->s, simClock->ns, msg.pid);
      (*lines)++;
    }
    printf("./oss: %d terminated\n", pid);
  } else {
    fprintf(logFile, "%d.%09ds ./oss: Error: Unknown Message Action: %d\n", simClock->s, simClock->ns, msg.action);
    fprintf(stderr, "./oss: Error: Unknown Message Action: %d\n", msg.action);
    cleanup();
  }
  return;
}

//Deadlock detection and resolution
//Deadlock detection algorithm takes heavy inspiration from example in the notes
int deadlock(resource_descriptor_t *resourceDescriptor, int processes, simtime_t *simClock, int *pids, int *active, int *lines) {
  int work[MAX_RESOURCES];
  int finish[processes];
  int deadlocked[processes];
  int deadlockCount = 0;
  int i;
  int p;
  for (i = 0; i < MAX_RESOURCES; i++) {
    work[i] = resourceDescriptor->allocationVector[i];
  }
  for (i = 0; i < processes; i++) {
    finish[i] = FALSE;
  }
  //Detection
  for (p = 0; p < processes; p++) {
    if (finish[p] == TRUE) {
      continue;
    }
    if ((req_lt_avail(resourceDescriptor->requestMatrix[p], resourceDescriptor->allocationVector,
                      resourceDescriptor->sharedResourceVector, resourceDescriptor->allocationMatrix[p])) == TRUE) {
      finish[p] = TRUE;
      for (i = 0; i < MAX_RESOURCES; i++) {
        work[i] += resourceDescriptor->allocationMatrix[p][i];
      }
      p = -1;
    }
  }
  for (p = 0; p < processes; p++) {
    if (finish[p] == FALSE) {
      deadlocked[deadlockCount++] = p;
    }
  }
  //Resolution
  if (deadlockCount > 0) {
    if ((*lines) < 100000) {
      fprintf(logFile, "%d.%09ds ./oss: Deadlock Detected\n", simClock->s, simClock->ns);
      (*lines)++;
    }
    for (i = 0; i < deadlockCount; i++) {
      //Verbose logging
      if ((*lines) < 100000) {
        fprintf(logFile, "\t./oss: p%d deadlocked\n", deadlocked[i]);
        (*lines)++;
      }
      terminate_and_release(resourceDescriptor, deadlocked[i], pids, active);
      //Verbose logging
      if ((*lines) < 100000) {
        fprintf(logFile, "\t./oss: p%d terminated\n", deadlocked[i]);
        (*lines)++;
      }
    }
    return TRUE;
  }
  return FALSE;
}
//Taken from notes
int req_lt_avail(int req[], int avail[], int shared[], int held[]) {
  int i = 0;
  for (i = 0; i < MAX_RESOURCES; i++) {
    if (shared[i] == 1 && req[i] > 0 && held[i] == 5) {
      break;
    } else if (req[i] > avail[i]) {
      break;
    }
  }
  if (i == MAX_RESOURCES) {
    return TRUE;
  } else {
    return FALSE;
  }
}
//send a kill signal to a deadlocked child and release its resources
void terminate_and_release(resource_descriptor_t *resourceDescriptor, int simPid, int *pids, int *active) {
  int i;
  //send kill signal to child
  kill(pids[simPid], SIGKILL);
  waitpid(pids[simPid], NULL, 0);
  //release all the resources
  for (i = 0; i < MAX_RESOURCES; i++) {
    //add held resources back to available
    if (resourceDescriptor->sharedResourceVector[i] == 0) {
      resourceDescriptor->allocationVector[i] += resourceDescriptor->allocationMatrix[simPid][i];
    }
    resourceDescriptor->allocationMatrix[simPid][i] = 0;
    resourceDescriptor->requestMatrix[simPid][i] = 0;
  }
  //make simpid available
  pids[simPid] = -1;
  (*active)--;
  return;
}