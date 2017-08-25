#include "common.h"

typedef struct client_thread_args_s{
  int socket;
}client_thread_args_t;

typedef struct chat_session_args_s{
  int socket;
  char* username;
  pthread_t* thread_id;
}chat_session_args_t;

typedef struct read_updates_args_s{
  GAsyncQueue* buf_modifications;
  int server_socket;
}read_updates_args_t;

#define MSG_LEN 256
#define MAIN_CMD_LEN 9

//thread routines
void* usr_list_recv_thread_routine(void *args);
void* listen_routine(void *args);
void* connect_routine(void *args);
void* recv_routine(void* args);
void* send_routine(void* args);

//functions
void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command);
void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command);
int get_username(char* username, int socket);
int chat_session(char* username, int socket);
void main_interrupt_handler(int dummy);
GHashTable* usr_list_init();
