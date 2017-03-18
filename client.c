#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "client.h"
#include "common.h"
/*

//function to process user list element and adding it to user list
void userList_handle(char* elem, int len){


}


//thread listen routine
void* listen_thread_routine(void *args){

  struct listen_thread_args_t* arg = args;

  int socket = arg.socket;
  struct sockaddr_in* address = arg.addr;

  //binding listen thread address to listen thread socket
  ret = bind(socket_listen_thread_desc, (const struct sockaddr*)&address, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error while binding address to listen thread socket");

  //thread_listen listening for incoming connections
  ret = listen(socket, 1);
  ERROR_HELPER(ret, "Cannot listen on listen_thread socket");


  *
  *
  *  P2P chat implementation
  *
  *
  *


}

void* usr_list_recv_thread_routine(void *args){

  struct listen_thread_args_t* arg = args;

  //getting arguments from args used for connection
  int socket = arg.socket;
  struct sockaddr_in* thread_addr = arg.addr;

  //allocating address struct for incoming connection from server
  struct sockaddr_in* server_addr = calloc(1, sizeof(sockaddr_in));

  //binding user list receiver thread address to user list receiver thread socket
  ret = bind(socket, (const struct sockaddr*)&thread_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error while binding address to user list receiver thread socket");

  //user list receiver thread listening for incoming connections
  ret = listen(socket, 1);
  ERROR_HELPER(ret, "Cannot listen on user list receiver thread socket");

  //accepting connection on user list receiver thread socket
  ret = accept(socket, (struct sockaddr*) server_addr, (socklen_t*) sizeof(sockaddr_in));
  ERROR_HELPER(ret, "Cannot accept connection on user list receiver thread socket");

  //allocating buffer to write user list element
  char* buffer_elem[USERLIST_BUFF_SIZE];
  int elem_buf_len = 0;

  //receiving user list element from server
  while(1){
    int bytes_read = 0;
    char* buf[USERLIST_BUFF_SIZE];

    //making sure to read all bytes of the message
    while (bytes_read <= USERLIST_BUFF_SIZE) {
        ret = recv(socket, buf + bytes_read, 1, 0);

        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Error while receiving user element from server");

        if (buf[bytes_read] == '\n') break; // end of message

        bytes_read++;
    }

    buf[bytes_read] = '\0';
    elem_buf_len = bytes_read;

    //processing user list element with userList_handle function
    userList_handle(buf, elem_buf_len);
  }


}

*/

int main(int argc, char* argv[]){

  int ret;
  fprintf(stderr, "flag 0\n");

  //getting username from argv
  char* username = argv[1];
  strcat(username, "\n"); //concatenating "\n" for server recv function

  //socket descriptor to connect to server
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Error while creating client socket descriptor");

  //data structure for the connection to the server
  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family         = AF_INET;
  serv_addr.sin_port           = htons(SERVER_PORT);
  serv_addr.sin_addr.s_addr    = inet_addr(SERVER_IP);

  fprintf(stderr, "flag 1\n");

  /*
  //socket descriptor for listen thread
  int socket_listen_thread_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_listen_thread_desc, "Error while creating client listen socket descriptor");

  //socket descriptor for user list receiver thread
  int usrl_recv_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(usrl_recv_socket, "Error while creating user list receiver thread socket descriptor");

  //address structure for listen thread socket
  struct sockaddr_in incoming_client_addr = {0};
  serv_addr.sin_family              = AF_INET;
  serv_addr.sin_port                = htons(CLIENT_THREAD_LISTEN_PORT);

  //address structure for user list receiver thread socket
  struct sockaddr_in usrl_sender_address = {0};
  usrl_sender_address.sin_family         = AF_INET;
  usrl_sender_address.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);



  //thread listen
  //
  //creating parameters for listen thread funtion
  listen_thread_args_t* t_listen_args = (listen_thread_args_t*) malloc(sizeof(listen_thread_args_t));
  t_listen_args.socket = socket_listen_thread_desc;
  t_listen_args.addr   = incoming_client_addr;

  //creating and spawning thread listen with parameters
  pthread_t thread_listen;
  ret = pthread_create(&thread_listen, NULL, listen_thread_routine, &t_listen_args);
  PTHREAD_ERROR_HELPER(ret, "Unable to create listen_thread");

  //detatching from listen_thread
  ret = pthread_detach(thread_listen);
  PTHREAD_ERROR_HELPER(ret, "Unable to detatch from listen_thread");
  //
  //detached from thread listen


  //user list receiver thread
  //
  //creating parameters for user list receiver thread funtion
  listen_thread_args_t* usrl_recv_args = (listen_thread_args_t*)malloc(sizeof(listen_thread_args_t));
  t_listen_args.socket = usrl_recv_socket;
  t_listen_args.addr   = usrl_sender_address;

  //creating and spawning user list receiver thread with parameters
  pthread_t thread_usrl_recv;
  ret = pthread_create(&thread_usrl_recv, NULL, usr_list_recv_thread_routine, &usrl_recv_args);
  PTHREAD_ERROR_HELPER(ret, "Unable to create user list receiver thread");

  //detatching from user list receiver thread
  ret = pthread_detach(thread_usrl_recv);
  PTHREAD_ERROR_HELPER(ret, "Unable to detatch from user list receiver thread");
  //
  //detached from user list receiver thread

  */

  //connection to server
  ret = connect(socket_desc, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to server");

  fprintf(stderr, "flag 2\n");

  //sending buffer init data for user list
  //creating buffer for username and availability flag
  char username_buf[16];
  strncpy(username_buf, username, strlen(username));

  //sending username to server
  ret = send(socket_desc, username_buf, strlen(username), 0);

  //making sure all bytes have been sent
  int bytes_left = strlen(username);
  int bytes_sent = 0;

  fprintf(stderr, "flag 3\n");

  while (bytes_left > 0){
      ret = send(socket_desc, username_buf + bytes_sent, bytes_left, 0);

      if (ret == -1 && errno == EINTR){
        continue;
      }
      ERROR_HELPER(ret, "Error while sending username to server");

      bytes_left -= ret;
      bytes_sent += ret;
  }

  fprintf(stderr, "flag 4\n");
  // close socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "Cannot close socket");


  exit(EXIT_SUCCESS);

}
