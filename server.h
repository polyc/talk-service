#define MAX_CONN_QUEUE 5
#define SERVER_QUIT "QUIT"

//data structure passed to threads on creation
typedef struct thread_args_s{
  int socket;
  unsigned int thread_id;
  char* addr; //dotted form
} thread_args_t;

//void create_user_list_element(usr_list_elem_t* element, char* ip, thread_args_t* args, char* buf);

void* connection_handler(void* arg);
