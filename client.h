#include common.h

#define CLIENT_IP "..." //add client IP address

typedef struct listen_thread_args_s{
  int socket;
  struct sockaddr_in* addr;
}listen_thread_args_t;  //change name beacuse used both by lsiten_thread and usrl_recv_thread


void* usr_list_recv_thread_routine(void *args);

void* listen_thread_routine(void *args);
