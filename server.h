#include "common.h"

#define MAXUSERS 10
#define MAX_CONN_QUEUE 5
#define SERVER_QUIT "QUIT"

void create_user_list_element(struct usr_list_elem_t* element, char* client_ip, thread_args_t* args);

void* client-process/server-thread_connection_handler(void* arg);

//data structure passed to threads on creation
typedef struct thread_args_s{
  int socket;
  unsigned int thread_id;
  char* addr; //dotted form
} thread_args_t;
