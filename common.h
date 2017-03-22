#ifndef __COMMON_H__
#define __COMMON_H__

#define SERVER_PORT 2015
#define SERVER_IP "127.0.0.1" //add IP address of the server
#define CLIENT_THREAD_LISTEN_PORT 1025
#define CLIENT_THREAD_RECEIVER_PORT 1026
#define USERLIST_BUFF_SIZE 40
#define USRNAME_BUF_SIZE 17

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


//user list typical element
typedef struct usr_list_elem_s{
  char user_name[16];
  char* client_ip;
  char a_flag;
}usr_list_elem_t;


#endif
