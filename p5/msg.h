#ifndef MSG_H
#define MSG_H

//possible values for msg action
//sent from child
const int request = 0;//resource request
const int release = 1;//resource release
const int terminate = 2;
//sent from oss
const int granted = 3;//granted the resource
const int denied = 4;//waiting for release 

typedef struct {
  long mtype;
  int rid;     //resource id [0-19]
  int action;  //0 = request, 1 = release
  int pid;
  int sender;//process id of the sender
} msg_t;

#endif