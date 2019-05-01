#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

/*Prototypes*/
void help_message();
void no_args_message();
FILE* open_file(char*, char*, char*);
int read_file_to_shm(FILE*, int);
void fork_handler(int, int);
void write_headers();
static void time_out();
static void sigint_handler();
void cleanup();

/*GLobals*/
key_t SHMKEY = 110594;

/*Main*/
int main(int argc, char* argv[]) {
  int opt; //getopt option
  char* in_name = "palin.in"; //input file name
  FILE* in_file;
  char* e_stmt = "./master: Error: ";
  int str_count;//count of the strings in shared memory
  int shmid;//shared memory id
  int n = 20;//max amount of processes allowed in the system at one time  
    
  if(argc < 2) { //No arguments
    no_args_message();
  }
  
  while((opt = getopt(argc, argv, "hf:n:")) != -1) { //getopt loop
    switch(opt) {
      case 'h':
        help_message();
        return 0;
      case 'f':
        in_name = optarg;
        break;
      case 'n':
        n = atoi(optarg);
        break;
      default:
        no_args_message();
        break;
    }//end switch
  }//end while
  //print option values
  printf("\nInput File: %s\n", in_name);
  printf("n = %d\n", n);
  //Get the shared memory id
  shmid = shmget(SHMKEY, 100 * 80, IPC_CREAT | 0775);
  if(shmid < 0) {//error
    perror("./master: Error: shmget ");
    exit(EXIT_FAILURE);
  }
  //set up signal handlers for 2 min timout and catching ctrl-C
  signal(SIGALRM, time_out);
  alarm(120);//120 seconds
  signal(SIGINT, sigint_handler);
  
  in_file = open_file(in_name, "r", e_stmt);//open input file
  str_count = read_file_to_shm(in_file, shmid);//read input file into shared mem
  printf("\nLines read: %d\nChildren to be spawned: %d\n", str_count, (str_count+4)/5);
  fclose(in_file);//close input file
  write_headers();//write a header to the output files "PID    IND STRING"
  fork_handler(str_count, n);//all the forks, execs, and waits occur here
  shmctl(shmid, IPC_RMID, NULL);//delete shared memory
  
  return 0;
}

/*Definitions*/
void help_message() {//Help message displayed when given -h option
  printf("\nThis program takes the following possible arguments\n");
  printf("\n");
  printf("  -h           : to display this help message\n");
  printf("  -f filename  : to use custom input file\n");
  printf("  -n x         : x = the amount of children allowed to exist at one time\n");
  printf("\n");
  printf("Default Values:\n");
  printf("  Input File: palin.in\n");
  printf("  Output Files: palin.out and nopalin.out\n");
  printf("  n = 20\n");
  printf("\n");
  return;
}
//
void no_args_message() {//Message displayed when default values are used
  printf("\nNo arguments given\n");
  printf("Using default values\n");
  return;
}

/*fopen with an error check*/
FILE* open_file(char* fname, char* opts, char* error) {
  FILE* fp = fopen(fname, opts);
  if(fp == NULL) {//error opening file
    perror(error);
    exit(EXIT_FAILURE);
  }
  return fp;
}
/*read lines from a given file pointer and copy them to shared memory with a given id*/
int read_file_to_shm(FILE* fp, int shmid) {
  char buf[80];//file buffer
  int count = 0;//string/line counter
  char (*strings)[100][80];//ptr to 2D arr of chars

  strings = shmat(shmid, NULL, 0);//attach
  if(strings < 0) {//error
    perror("./master: Error: shmat ");
    exit(EXIT_FAILURE);
  }
  while(fgets(buf, sizeof(buf), fp)) {//copy each line into shared memory
    strcpy((*strings)[count++], buf);
  }
  shmdt(strings);//detach
  return count;
}
/*Handles all the forks, execs, waits, and creates a binary semaphore*/
void fork_handler(int str_count, int n) {
  int running = 0;//count of child processes in the system
  int term = 0;//count of the total processes terminated
  int index = str_count - 1;//index arg passed to each child
  char arg[3];//string argument for exec
  pid_t pid;//process id returned from fork();
  sem_t* sem;//sempahore for protecting the output files
  char* sem_name = "palin_sem";
  
  if(n > 20)//check n arg
    n = 20;
  //create the semapore. starting semaphore value = 1
  sem = sem_open(sem_name, O_CREAT, 0644, 1);
  if(sem == SEM_FAILED) {
    perror("./master: Error: sem_open");
    exit(EXIT_FAILURE);
  }
  printf("\n%-9s %-6s %s\n", "TIME", "PID", "EVENT");//console header
  while(term < ((str_count+4)/5)) {//end loop when enough children have terminated 
    if(running < n && index >= 0) {//not too many processes and strings remaining 
      pid = fork();//spawn child process
      if(pid < 0) {//Forking error
        perror("./master: Error: Fork error");
        exit(EXIT_FAILURE);
      }
      else if(pid == 0) {//Child process
        sprintf(arg, "%d", index);//get index as string
        execl("./palin", "palin", arg, (char*)NULL);//exec ./palin arg
      }
      ++running;//increment running process counter
      index = index - 5;//lower the index by 5
    }
    pid = waitpid(-1, NULL, WNOHANG);//returns child pid if child terminated, else -1
    if(pid > 0) {//child terminated
      ++term;//increment process terminated counter
      --running;//decrement running process counter
    }
  }//end while
  sem_unlink(sem_name);//all children are done so destroy semaphore
  
  return;
}
/*writes a simple header to palin.out and nopalin.out*/
void write_headers() {
  FILE *fp;
  fp = open_file("palin.out", "a", "./palin: Error: failed opening palin.out");
  fprintf(fp, "\n%-6s %-3s %s\n", "PID", "IND", "STRING");
  fclose(fp);
  fp = open_file("nopalin.out", "a", "./palin: Error: failed opening no_palin.out");
  fprintf(fp,"\n%-6s %-3s %s\n", "PID", "IND", "STRING");
  fclose(fp);
}
/*SIGALRM handler*/
static void time_out() {
  printf("2 minute timeout\n");
  printf("Killing child processes\n");
  cleanup();
  exit(EXIT_SUCCESS);
}

/*SIGINT handler*/
static void sigint_handler() {
  printf("SIGINT signal recieved (ctrl-C)\n");
  printf("Killing child pocesses\n");
  cleanup();
  exit(EXIT_SUCCESS);
}
/*delete shared memory, terminate children, and unlink the semaphore*/
void cleanup() {
  int shmid;
  shmid = shmget(SHMKEY, 100 * 80, IPC_CREAT | 0775);
  shmctl(shmid, IPC_RMID, NULL);
  sem_unlink("palin_sem");
  kill(0, SIGKILL);
  return;
}
