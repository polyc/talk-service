#include "server.h"

//client-process/server-thread communication routine
void* client-process/server-thread_connection_handler(void* arg){
  thread_args_t* args = (thread_args_t*)arg;

  int ret, recv_bytes;

  //user list element, single slot receive buffer
  char buf[sizeof(usr_list_server_elem_t)];
  size_t buf_len = sizeof(buf);

  //quit command buffer
  char* quit_command = SERVER_COMMAND;
  size_t quit_command_len = strlen(quit_command);

  while(1){
    //read message from client
    while ((recv_bytes = recv(args->socket, buf, buf_len, 0)) < 0) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "Cannot read from socket");
    }

    printf("message read\n");

    // quit command check
    if (recv_bytes == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;
  }

  //close client_desc
  ret = close(args->socket);
  ERROR_HELPER(ret, "Cannot close socket for incoming connection");

  free(args->addr);
  free(args);
  pthread_exit(NULL);
}

int int main(int argc, char const *argv[]) {
  int ret, server_desc, client_desc;

  struct sockaddr_in server_addr = {0};
  int sockaddr_len = sizeof(struct sockaddr_in);

  // initialize socket for listening
  socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  ERROR_HELPER(socket_desc, "Could not create socket");

  server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

  //We enable SO_REUSEADDR to quickly restart our server after a crash
  int reuseaddr_opt = 1;
  ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

  // bind address to socket
  ret = bind(socket_desc, (struct sockaddr*) &server_addr, sockaddr_len);
  ERROR_HELPER(ret, "Cannot bind address to socket");

  // start listening
  ret = listen(socket_desc, MAX_CONN_QUEUE);
  ERROR_HELPER(ret, "Cannot listen on socket");

  // we allocate client_addr dynamically and initialize it to zero
  struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

  // loop to manage incoming connections spawning handler threads
    while (1) {
      client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
      if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
      ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

      pthread_t thread;

      // put arguments for the new thread into a buffer
        thread_args_t* thread_args = malloc(sizeof(thread_args_t));
        thread_args->socket = client_desc;
        thread_args->thread_id = 0; //new_thread_id(),not written yet;
        thread_args->addr = client_addr;

        ret = pthread_create(&thread, NULL, connection_handler, (void*)thread_args);
        PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

        //new buffer for new incoming connection
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }

    exit(EXIT_SUCCESS);
}
