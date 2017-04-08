#include "common.h"

typedef struct client_thread_args_s{
  int socket;
}client_thread_args_t;  //change name beacuse used both by lsiten_thread and usrl_recv_thread

//thread routines
void* usr_list_recv_thread_routine(void *args);
void* listen_routine(void *args);
void* connect_routine(void *args);
void* recv_routine(void* args);
void* send_routine(void* args);

//functions
void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command);
void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command);
int get_username(char* username);
void chat_session(char* username, int socket);
GHashTable* usr_list_init();
