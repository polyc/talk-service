#include common.h

#define CLIENT_IP "..." //add client IP address


//user list typical element
typedef struct usr_list_client_elem_s{
  char[16] user_name;
  struct listen_thread_t listen_thread;
  char a_flag;
}usr_list_client_elem_t;

typedef struct listen_thread_args_s{
  int socket;
  struct sockaddr_in* addr;
}listen_thread_args_t;  //change name beacuse used both by lsiten_thread and usrl_recv_thread


void* usr_list_recv_thread_routine(void *args);

void* listen_thread_routine(void *args);
