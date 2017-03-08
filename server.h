
#include <netinet/in.h>
#include common.h


//user list

//user list receiver thread struct
typedef struct usr_list_receiver_thread_s{
  int socket;
  struct sockaddr_in* address;
} usr_list_receiver_thread_t;

//user list element
typedef struct usr_list_server_elem_s{
  char[16] user_name;
  struct listen_thread_t listen_thread;
  struct usr_list_receiver_thread_t recv_thread;
  char a_flag;
}usr_list_server_elem_t;
