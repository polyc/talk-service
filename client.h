#include "common.h"

typedef struct client_thread_args_s{
  int socket;
}client_thread_args_t;

typedef struct read_updates_args_s{
  GAsyncQueue* read_updates_mailbox;
}read_updates_args_t;

#define MSG_LEN         256
#define MAIN_CMD_LEN    9
#define MAX_WAIT_SERVER 50

#define CMD_STRING "\n<<Client commands>>\n\n<<list:    display chat list>>\n<<connect: connect to client>>\n<<help:    display available commands>>\n<<exit:    exit program>>\n<<clear:   clear screen>>\n\n"
#define LIST       "list\n"
#define CONNECT    "connect\n"
#define HELP       "help\n"
#define CLEAR      "clear\n"

//thread routines
void* recv_updates(void *args);
void* read_updates(void* args);

//functions
int get_username(char* username, int socket);
static void print_userList(gpointer key, gpointer elem, gpointer data);
GHashTable* usr_list_init();
void _initSignals();
void _initSemaphores();
void intHandler();
void my_flush();
void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command);
void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command);
void list_command();
void send_message(int socket);
void connect_to(int socket, char* target_client);
void reply(int socket);
void display_commands();
