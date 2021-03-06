#include "common.h"

#define MAX_CONN_QUEUE 5
#define MAX_GET_USERNAME_INACTIVITY 120
#define MAX_INACTIVITY 600
#define GENERIC_THREAD_TERM -1

//get_username() MACROS
#define TRY_AGAIN -2

//data structure passed to connection handler threads on creation
typedef struct thread_args_s{
  int socket;
  char* client_ip;
  char* client_user_name; //key for userlist hash table
  char* mailbox_key;//key for mailbox hash table
} thread_args_t;

//data structure passed to sender threads on creation
typedef struct sender_thread_args_s{
  sem_t* sender_stop;//sender termination notify
  sem_t* sender_sync;//sync between chandlers and senders
  int* threads_term;//notify chandler a sigpipe
  char* client_ip;
  char* mailbox_key;
}sender_thread_args_t;

//struct for push_entry function
typedef struct push_entry_args_s{
  char* message;
  char* sender_username;
}push_entry_args_t;

//struct for send_list
typedef struct send_list_args_s{
  int* socket;
  int* threads_term; ////notify chandler a sigpipe (it's the same var in sender_thread_args_s)
}send_list_args_t;


void _initSignals();
void _initMainSemaphores();
void intHandler();
void* connection_handler(void* arg);
void* sender_routine(void* arg);
int get_username(thread_args_t* args, usr_list_elem_t* new_element);
void serialize_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command);
usr_list_elem_t* getTargetElement(char* target_buf);
void push_all(push_entry_args_t* args);
void notify(char* message_buf, char* element_username, char* mod_command, usr_list_elem_t* element_to_update);
int execute_command(thread_args_t* args, char* message_buf, usr_list_elem_t* element_to_update, char* target_buf);
void send_list_on_client_connection(gpointer key, gpointer value, gpointer user_data);
void update_availability(usr_list_elem_t* element_to_update, char buf_command);
void remove_entry(char* elem_to_remove, char* mailbox_to_remove);
void remove_mailbox(char* mailbox_to_remove);
void push_entry(gpointer key, gpointer value, gpointer user_data);
void cleanup_client(thread_args_t* args);
void push_to_mailboxes(char* message);
char* parse_username(char* src, char*dest, char message_type);
int connection_accepted(char* response);
