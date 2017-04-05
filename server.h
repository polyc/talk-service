#define MAX_CONN_QUEUE 5
#define SERVER_QUIT "QUIT"

//data structure passed to threads on creation
typedef struct thread_args_s{
  int socket;
  char* client_ip;
  char* client_user_name; //key for both hash table
} thread_args_t;

typedef struct sender_thread_args_s{
  sem_t* chandler_sender_sync;
}sender_thread_args_t;

//void create_user_list_element(usr_list_elem_t* element, char* ip, thread_args_t* args, char* buf);

void* connection_handler(void* arg);
void* sender_routine(void* arg);
void stringify_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command);
void receive_and_execute_command(thread_args_t* args, char* buf_command);
GHFunc send_list_on_client_connection(gpointer key, gpointer value, gpointer user_data);
