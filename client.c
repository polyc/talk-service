#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "common.h"

void* listen_thread_routine(void *args){
  struct listen_thread_args_t* arg = args;
  int socket = arg.socket;

  //thread_listen listening for incoming connections
  ret = listen(socket, 1);
  ERROR_HELPER(ret, "Cannot listen on listen_thread socket");
}


int main(){

  int ret;

  //socket descriptor to connect to server
  int socket_server_desc = 0;
  socket_server_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_server_desc, "error while creating client socket descriptor");

  //data structure for the connection to the server
  struct sockaddr_in serv_addr;
  serv_addr.sin_len      = sizeof(sockaddr_in);
  serv_addr.sin_family   = AF_INET;
  serv_addr.sin_port     = htons(SERVER_PORT);
  serv_addr.sin_addr     = SERVER_IP;

  //socket descriptor for listen thread
  int socket_listen_thread_desc = 0;
  socket_listen_thread_desc  = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_listen_thread_desc, "error while creating client listen socket descriptor");

  //data structure for listen thread
  struct sockaddr_in listen_address = {0};
  serv_addr.sin_len     = sizeof(sockaddr_in);
  serv_addr.sin_family  = AF_INET;
  serv_addr.sin_port    = htons(SERVER_PORT);

  ret = bind(socket_listen_thread_desc, (const struct sockaddr*)&listen_address, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "error while binding address to listen thread socket");

  //listen thread data to pass to server
  struct listen_thread_t listen_thread_data;
  listen_thread_data.socket  = socket_listen_thread_desc;
  listen_thread_data.address = listen_address;


  //thread listen
  //
  //creating parameters for listen thread funtion
  listen_thread_args_t* t_listen_args = malloc(sizeof(listen_thread_args_t*));
  t_listen_args.socket = socket_listen_thread_desc;

  //creating and spawning thread listen with parameters
  pthread_t thread_listen;
  ret = pthread_create(&thread_listen, NULL, listen_thread_routine, &t_listen_args);
  PTHREAD_ERROR_HELPER(ret, "unable to create listen_thread");

  //detatching from listen_thread
  ret = pthread_detach(thread_listen);
  PTHREAD_ERROR_HELPER(ret, "unable to detatch from listen_thread");
  //
  //detached from thread listen


  //temporary receiver thread data structure to pass to server
  struct usr_list_receiver_thread_t receiver_thread_data;
  receiver_thread_data.socket  = 0;
  receiver_thread_data.address = NULL;

  //client data structure to send to server
  struct usr_list_server_elem_t client_data;
  client_data.user_name     = "regibald_94";
  client_data.listen_thread = listen_thread_data;
  client_data.recv_thread   = receiver_thread_data; //not initiated will create null for now
  client_data.a_flag        = AVAILABLE;

  //connection to server
  ret = connect(socket_server_desc, (const struct sockaddr)&serv_addr, sizeof(serv_addr));
  ERROR_HELPER(ret, "error trying to connect to server");

  //sending init data to server
  ret = send()

}
