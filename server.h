#include "common.h"

#define MAXUSERS 10
#define SERVER_QUIT "QUIT"

void* client-process/server-thread_connection_handler(void* arg);

//data structure passed to threads on creation
typedef struct thread_args_s{
  int socket;
  unsigned int thread_id;
  struct sockaddr_in* addr;
} thread_args_t;
