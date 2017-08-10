#define MAX_CONN_QUEUE 5
#define SERVER_QUIT "QUIT"
#define MESSAGE_SIZE USERNAME_BUF_SIZE + 3
//data structure passed to threads on creation
typedef struct thread_args_s{
  int socket;
  char* client_ip;
  char* client_user_name; //key for userlist hash table
  sem_t* chandler_sync;
  sem_t* sender_sync;
  char* mailbox_key;//key for mailbox hash table
  int id;
} thread_args_t;

typedef struct sender_thread_args_s{
  sem_t* chandler_sync;//sync between chandlers and senders
  sem_t* sender_sync;
  char* client_ip;
  GAsyncQueue* mailbox;//mailbox for message received from chandlers
  char* mailbox_key;
  int id;
}sender_thread_args_t;

//void create_user_list_element(usr_list_elem_t* element, char* ip, thread_args_t* args, char* buf);

void* connection_handler(void* arg);
void* sender_routine(void* arg);
void get_and_check_username(int socket, char* username);
void serialize_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command);
int execute_command(thread_args_t* args, char* message_buf, usr_list_elem_t* element_to_update, char* target_buf, int* connected);
void send_list_on_client_connection(gpointer key, gpointer value, gpointer user_data);
void update_availability(usr_list_elem_t* element_to_update, char* buf_command);
void remove_entry(char* elem_to_remove, char* mailbox_to_remove);
void remove_mailbox(char* mailbox_to_remove);
void push_entry(gpointer key, gpointer value, gpointer user_data/*parsed message*/);
void cleanup_client(thread_args_t* args);
void push_to_mailboxes(char* message);
char* parse_username(char* src, char*dest);
