#include common.h
#include <netinet/in.h>

//user list typical element
typedef struct usr_list_client_elem_s{
  char[16] user_name;
  struct listen_thread_t listen_thread;
  char a_flag;
}usr_list_client_elem_t;
