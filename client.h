typedef struct receiver_thread_args_s{
  int socket;
}receiver_thread_args_t;  //change name beacuse used both by lsiten_thread and usrl_recv_thread


void* usr_list_recv_thread_routine(void *args);
void* listen_thread_routine(void *args);
void userList_handle(char* elem, int len);
void print_elem_list(const char* buf, int x);
