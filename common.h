#ifndef __COMMON_H__
#define __COMMON_H__

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 2015
#define CLIENT_THREAD_LISTEN_PORT 1025
#define CLIENT_THREAD_RECEIVER_PORT 1026
#define USERNAME_BUF_SIZE 17
#define USERLIST_BUFF_SIZE 7 + USERNAME_BUF_SIZE + INET_ADDRSTRLEN
#define POP_TIMEOUT 2000000

//Glib hash manipulation macros
#define INSERT    g_hash_table_insert
#define REPLACE   g_hash_table_replace
#define CONTAINS  g_hash_table_contains
#define REMOVE    g_hash_table_remove
#define LOOKUP    g_hash_table_lookup
#define FOR_EACH  g_hash_table_foreach

//Glib AsyncQueue macros
#define REF g_async_queue_ref
#define UNREF g_async_queue_unref
#define PUSH g_async_queue_push
#define POP g_async_queue_timeout_pop

//server commands macros
#define AVAILABLE   'a'
#define UNAVAILABLE 'u'
#define DISCONNECT  'c'
#define NEW         'n'
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
