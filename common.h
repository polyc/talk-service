#ifndef __COMMON_H__
#define __COMMON_H__

#define SERVER_PORT 2015
#define SERVER_IP "127.0.0.1" //add IP address of the server
#define CLIENT_THREAD_LISTEN_PORT 1025
#define CLIENT_THREAD_RECEIVER_PORT 1026
#define USERLIST_BUFF_SIZE 50

#define AVAILABLE   'a'
#define UNAVAILABLE 'u'
#define QUIT        'q'

#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {             \
        if (cond) {                                               \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));  \
            exit(EXIT_FAILURE);                                   \
        }                                                         \
    } while(0)

#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)


//user list sender/receiver thread's single slot buffer
typedef struct user_list_buffer_s{
  char user_name[16];
  char* client_ip;
  short user_elem_pos;
  char a_flag;
}user_list_buffer_t;

//user list

//user list typical element
typedef struct usr_list_elem_s{
  char user_name[16];
  char* client_ip;
  char a_flag;
}usr_list_elem_t;


#endif
