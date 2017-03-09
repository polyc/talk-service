
#include <netinet/in.h>

// listen thread struct
typedef struct listen_thread_s{
  int socket;
  struct sockaddr_in* address;
} listen_thread_t;

// send/receive user list buffer slot's typical handled element
typedef struct user_list_buffer_s{
  char[16] user_name;
  struct listen_thread_t listen_thread;
  short user_elem_pos;
  char a_flag;
}user_list_buffer_t;
