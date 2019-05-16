#ifndef MSG_H
#define MSG_H

typedef struct {
  long mtype;
  int pid;
  int address;
  int action;
} msg_t;

enum actions {READ, WRITE, TERMINATE};

//mytpe that oss will be looking for
const int ossChannel = 20;

#endif