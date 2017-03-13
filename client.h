#include common.h

//user list typical element
typedef struct usr_list_client_elem_s{
  char[16] user_name;
  struct listen_thread_t listen_thread;
  char a_flag;
}usr_list_client_elem_t;

typedef struct listen_thread_args_s{
  int socket;
  struct sockaddr_in* addr;
}listen_thread_args_t;
