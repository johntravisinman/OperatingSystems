#ifndef OSS_H
#define OSS_H

#include "resourcedescriptor.h"
#include "simtime.h"

#define TRUE 1
#define FALSE 0

//Shared memory keys and IDs
int descriptorID;
const int DESCRIPTOR_KEY = 110594;
int simClockID;
const int SIM_CLOCK_KEY = 110197;
//Mesage Queue key and ID
int msqid;
const int MSG_Q_KEY = 052455;
//Log file pointer and verbose printing option
FILE *logFile;
int verbose = FALSE;


#endif