//included libraries
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>


#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {
        if (cond) {
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));
            exit(EXIT_FAILURE);
        }                                                           
    } while(0)

#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)


//data structure passed to threads on creation
typedef struct thread_args_s{
  unsigned int thread_id;
} thread_args_t;

//user list sender/receiver thread's single slot buffer
typedef struct user_list_buffer_s{
  char[16] user_name;
  struct listen_thread_t listen_thread;
  short user_elem_pos;
  char a_flag;
}user_list_buffer_t;

//user list
// client listen thread's info-struct into generic user element
typedef struct listen_thread_s{
  int socket;
  struct sockaddr_in* address;
} listen_thread_t;

//client receiver thread's info-struct into generic user element
typedef struct usr_list_receiver_thread_s{
  int socket;
  struct sockaddr_in* address;
} usr_list_receiver_thread_t;

//user list typical element
typedef struct usr_list_server_elem_s{
  char[16] user_name;
  struct listen_thread_t listen_thread;
  struct usr_list_receiver_thread_t recv_thread;
  char a_flag;
}usr_list_server_elem_t;
