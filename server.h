#define MAX_CONN_QUEUE 5
#define SERVER_QUIT "QUIT"

//data structure passed to threads on creation
typedef struct thread_args_s{
  int socket;
  unsigned int thread_id;
  struct sockaddr_in* addr;
  GHashTable* user_list;
  char* client_user_name;
} thread_args_t;

typedef struct sender_thread_args_s{
}sender_thread_args_t;

//void create_user_list_element(usr_list_elem_t* element, char* ip, thread_args_t* args, char* buf);

void* connection_handler(void* arg);
int recv_msg(int socket, char *buf, size_t buf_len);
void* sender_routine(void* arg);
GHashTable* usr_list_init();
GHashTable* thread_ref_init();
