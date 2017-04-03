#include "common.h"

#define CONNECT 1
#define RECEIVE 0

typedef struct receiver_thread_args_s{
  int socket;
}receiver_thread_args_t;  //change name beacuse used both by lsiten_thread and usrl_recv_thread


void* usr_list_recv_thread_routine(void *args);
void* listen_thread_routine(void *args);
void send_msg(int socket, char *buf);
int recv_msg(int socket, char *buf, size_t buf_len);
void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command);
void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command);
int get_username(char* username);
GHashTable* usr_list_init();
