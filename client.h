typedef struct receiver_thread_args_s{
  int socket;
  char* IP;
}receiver_thread_args_t;  //change name beacuse used both by lsiten_thread and usrl_recv_thread


void* usr_list_recv_thread_routine(void *args);
void* listen_thread_routine(void *args);
void userList_handle(char* elem, int len);
void print_elem_list(const char* buf, int x);
void send_msg(int socket, char *buf);
int recv_msg(int socket, char *buf, size_t buf_len);
