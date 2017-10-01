#include "common.h"

typedef struct client_thread_args_s{
  int socket;
}client_thread_args_t;

typedef struct read_updates_args_s{
  GAsyncQueue* read_updates_mailbox;
}read_updates_args_t;

#define MSG_LEN 256
#define MAIN_CMD_LEN 9

//thread routines
void* usr_list_recv_thread_routine(void *args);
void* read_updates(void* args);

//functions
void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command);
void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command);
int get_username(char* username, int socket);
void list_command();
void send_message(int socket);
void connect_to(int socket, char* target_client);
void responde(int socket);
static void print_userList(gpointer key, gpointer elem, gpointer data);
GHashTable* usr_list_init();
