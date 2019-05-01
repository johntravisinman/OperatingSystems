#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "msg.h"
#include "oss.h"
#include "resourcedescriptor.h"
#include "simtime.h"

simtime_t *get_shared_simtime();
int send_release(int rid, int pid, int simPid);
int send_request(int rid, int pid, int simPid);
void send_terminate(int pid, int simPid);
void wait_for_resource(int pid);


int main(int argc, char *argv[]) {
  if (argc < 2) {
    perror("Usage: ./user msqid simPid\n");
    exit(EXIT_SUCCESS);
  }
  simtime_t *simClock;
  simtime_t arrivalTime;
  simtime_t duration;
  //percentage chance of outcomes
  const int requestChance = 55;  //release the rest of the time
  const int terminateChance = 50;
  int pid;//real process id
  int rid;//resource id
  int simPid;//simulated process id
  int heldResources[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int resourceCounter = 0;
  int i;  //loop iterator
  int response;
  int diceroll; //holds a random value for comparing to request and terminate chance

  pid = getpid();
  msqid = atoi(argv[1]);
  simPid = atoi(argv[2]);
  simClock = get_shared_simtime();
  arrivalTime = *simClock;
  //more pseudo-randomness for processes launched close together
  srand(time(0) + (simClock->ns / 100000) + pid);
  
  while(1) {
    //calclate duration in system
    duration = subtract_sim_times((*simClock), arrivalTime);
    if ((duration.s >= 1) && (simClock->ns % 250000000 == 0)) {
      //check if should terminate
      diceroll = (rand() % 100) + 1;
      if(diceroll <= terminateChance) {
        //tell oss we are terminating
        send_terminate(pid, simPid);
        //actually terminate
        return 0;
      }
    }
    diceroll = (rand() % 100) + 1;
    if (diceroll <= requestChance) {
      //request
      rid = rand() % 20;//random process
      response = send_request(rid, pid, simPid);
      if (response == granted) {
        resourceCounter++;
        heldResources[rid]++;
      } else if (response == denied) {
        //denied
        wait_for_resource(pid);
        resourceCounter++;
        heldResources[rid]++;
      } else {
        fprintf(stderr, "./user: Error: Unknown Response: %d\n", response);
        EXIT_FAILURE;
      }
    } else if (heldResources > 0) {
      //release
      //find a resource to release
      for (i = 0; i < MAX_RESOURCES; i++) {
        if (heldResources[i] > 0) {
          rid = i;
          break;
        }
      }
      response = send_release(rid, pid, simPid);
      if (response == granted) {
        resourceCounter--;
        heldResources[rid]--;
      }
    }
  }
  return 0;
}
//return a pointer to simtime_t object in shared memory
simtime_t *get_shared_simtime() {
  simtime_t *simClock;
  simClockID = shmget(SIM_CLOCK_KEY, sizeof(simtime_t), IPC_CREAT | 0777);
  if (simClockID < 0) {
    perror("./user: Error: shmget for simulated clock");
    exit(EXIT_FAILURE);
  }
  simClock = shmat(simClockID, NULL, 0);
  if (simClock < 0) {
    perror("./user: Error: shmat for simulated clock");
    exit(EXIT_FAILURE);
  }
  return simClock;
}
//send request to oss
int send_request(int rid, int pid, int simPid) {
  //create message
  msg_t msg = {.mtype = 1, //destination is oss
               .action = request, 
               .rid = rid, 
               .pid = simPid,
               .sender = pid};
  //send request message
  if ((msgsnd(msqid, &msg, sizeof(msg_t), 0)) == -1) {
    perror("./user: Error: msgsnd ");
    exit(EXIT_FAILURE);
  }
  //receive response 
  if ((msgrcv(msqid, &msg, sizeof(msg_t), pid, 0)) == -1) {
    perror("./user: Error: msgrcv ");
    exit(EXIT_FAILURE);
  }
  return msg.action;
}
//send release to oss
int send_release(int rid, int pid, int simPid) {
  //create message
  msg_t msg = {.mtype = 1, 
               .action = release, 
               .rid = rid, 
               .pid = simPid,
               .sender = pid};
  //send request message
  if ((msgsnd(msqid, &msg, sizeof(msg_t), 0)) == -1) {
    perror("./user: Error: msgsnd ");
    exit(EXIT_FAILURE);
  }
    //receive response 
  if ((msgrcv(msqid, &msg, sizeof(msg_t), pid, 0)) == -1) {
    perror("./user: Error: msgrcv ");
    exit(EXIT_FAILURE);
  }
  return msg.action;
}
//send terminate to oss
void send_terminate(int pid, int simPid) {
  //create message
  msg_t msg = {.mtype = 1, 
               .action = terminate, 
               .rid = -1, 
               .pid = simPid,
               .sender = pid};
  //send request message
  if ((msgsnd(msqid, &msg, sizeof(msg_t), 0)) == -1) {
    perror("./user: Error: msgsnd ");
    exit(EXIT_FAILURE);
  }
  return;
}
//just wait to receive a message
void wait_for_resource(int pid) {
  msg_t msg;
  if ((msgrcv(msqid, &msg, sizeof(msg_t), pid, 0)) == -1) {
    perror("./user: Error: msgrcv ");
    exit(EXIT_FAILURE);
  }
}