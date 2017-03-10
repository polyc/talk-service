//included libraries
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

//data structure passed to threads on creation
typedef struct thread_args_s{
  unsigned int process_id;
  unsigned int thread_id;
} thread_args_t;

// client listen thread's info-struct into generic user list
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
