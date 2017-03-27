#ifndef __COMMON_H__
#define __COMMON_H__

#define SERVER_PORT 2015
#define LOCAL_IP "127.0.0.1"
#define CLIENT_THREAD_LISTEN_PORT 1025
#define CLIENT_THREAD_RECEIVER_PORT 1026
#define USERLIST_BUFF_SIZE 40
#define USRNAME_BUF_SIZE 17

//Glib hash manipulation macros
#define INSERT g_hash_table_insert

//server commands macros
#define AVAILABLE   'a'
#define UNAVAILABLE 'u'
#define MODIFY      'm'
#define DELETE      'd'
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
  char* client_ip;
  char a_flag;
}usr_list_elem_t;


#endif
