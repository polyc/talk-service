#ifndef __COMMON_H__
#define __COMMON_H__

//included libraries
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define SERVER_PORT 1024
#define SERVER_IP "...." //add IP address of the server
#define CLIENT_THREAD_LISTEN_PORT "1025"
#define CLIENT_THREAD_RECEIVER_PORT "1026"

#define AVAILABLE   'a'
#define UNAVAILABLE 'u'
#define QUIT        'q'

#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {
        if (cond) {
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));
            exit(EXIT_FAILURE);
        }
    } while(0)

#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)


//user list sender/receiver thread's single slot buffer
typedef struct user_list_buffer_s{
  char[16] user_name;
  char* client_ip;
  short user_elem_pos;
  char a_flag;
}user_list_buffer_t;

//user list

//user list typical element
typedef struct usr_list_server_elem_s{
  char[16] user_name;
  char* client_ip;
  char a_flag;
}usr_list_server_elem_t;


#endif
