#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

int isPalin(char str[]);
FILE* file_open(char*, char*);
void set_time();

int main(int argc, char* argv[]){
  int shmid;
  key_t shmkey = 110594;
  char (*strings)[100][80];//arr of strings that will be attached to shared mem
  int index;//assigned string index
  int pid;//this process' id
  FILE *fp;//output file pointer
  char *fnames[2] = {"nopalin.out", "palin.out"};//output file names
  int i;//loop iterator
  sem_t *sem;//semaphore to protect the output files
  char *sem_name = "palin_sem";
  char time[10];//hold time string
  int isPalin_arr[5] = {0, 0, 0, 0, 0};//holds pseudo-boolean value 
                                   //for if the five strings are palindromes
  pid = getpid();//get process id
  index = atoi(argv[1]);//get passed arg as int
  srand(index);//seed rand with the given index
  
  /*Shared Memory*/  
  shmid = shmget(shmkey, 100 * 80, IPC_CREAT | 0775);
  if(shmid < 0) {//handle error
    perror("./palin: Error: shmget");
    exit(EXIT_FAILURE);
  }
   
  strings = shmat(shmid, NULL, 0);//attach to shared mem
  if(strings < 0) {//handle error
    perror("./palin: Error: shmat");
    exit(EXIT_FAILURE);
  }

  /*Determine if palindromes*/
  for(i = 0; i < 5; i++) {
    if((index - i) < 0)
      break;
    else {
      isPalin_arr[i] = isPalin((*strings)[index-i]);
    }
  }

  /*Semaphore & Critical section*/
  sem = sem_open(sem_name, 0);//open semaphore created by master
  if(sem == SEM_FAILED) {
    perror("./palin: Error: sem_open");
    exit(EXIT_FAILURE);
  }

  for(i = 0; i < 5; i++) {
    if((index - i) < 0)
      break;
    sleep(rand() % 4);//sleep for 0-3 sec
    set_time(time);
    fprintf(stderr, "%-9s %-6d Entering the critical section: sem_wait()\n", time, pid);
    sem_wait(sem);//wait for turn
    /** CRITICAL SECTION **/
    set_time(time);
    fprintf(stderr, "%-9s %-6d Inside the critical section\n", time, pid);
    sleep(2);//sleep for 2 more sec
    fp = file_open(fnames[isPalin_arr[i]], "a");//open the correct file for this string
    fprintf(fp, "%-6d %03d %s\n", pid, (index - i), (*strings)[index-i]);//write to file
    fclose(fp);//done with file so close
    sleep(2);//sleep for 2 more sec
    set_time(time);
    fprintf(stderr, "%-9s %-6d Exiting the critical section: sem_post()\n", time, pid);
    /** END CRITICAL SECTION **/
    sem_post(sem);//done with crit section so signal the semaphore
  }
  exit(EXIT_SUCCESS);
}

/*check if given string is palindrome. 1 = true, 0 = false*/
int isPalin(char str[]) {
  int lo = 0;
  int hi = strlen(str) - 1;

  if(str[hi] == '\n') {//remove the trailing endline char
    str[hi] = '\0';
    hi--;
  }
  while(hi > lo) {
    if(str[lo++] != str[hi--])
      return 0;
  }
  return 1;
}

/*fopen with error check*/
FILE* file_open(char* fname, char* opts) {
  char error[128];
  sprintf(error, "./palin: Error: failed opening %s", fname);
  FILE* fp = fopen(fname, opts);
  if(fp == NULL) {
    perror(error);
    exit(EXIT_FAILURE);
  }
  return fp;
}

/*set given string to current time ex "23h59m59s", "00h00m00s", etc*/
void set_time(char* str) {
  time_t raw_time;
  struct tm* info;
  time(&raw_time);
  info = localtime(&raw_time);
  sprintf(str, "%02dh%02dm%02ds", info->tm_hour, info->tm_min, info->tm_sec);
  return;
}
