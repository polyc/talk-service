#include "common.h"

typedef struct receiver_thread_args_s{
  int socket;
}receiver_thread_args_t;  //change name beacuse used both by lsiten_thread and usrl_recv_thread


void* usr_list_recv_thread_routine(void *args);
void* listen_thread_routine(void *args);
void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command);
void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command);
int get_username(char* username);
GHashTable* usr_list_init();
