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
#include <strings.h>

#include "client.h"
#include "common.h"

/*

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
*/

void print_elem_list(const char* buf, int x){
  int j;

  fprintf(stdout, "Username[Element: %d]: ", x);
  for(j=0; j<40; j++){
    if(buf[j]=='-'){ //if end of string break
      fprintf(stdout, "\n");
      j++;
      break;
    }
    fprintf(stdout, "%c", buf[j]);
  }

  fprintf(stdout, "IP[Element: %d]: ", x);
  for(j; j<40; j++){
    if(buf[j]=='-'){ //if end of string break
      fprintf(stdout, "\n");
      j++;
      break;
    }
    fprintf(stdout, "%c", buf[j]);
  }

  fprintf(stdout, "Availability[Element: %d]: ", x);
  for(j; j<40; j++){
    if(buf[j]=='-'){ //if end of string break
      fprintf(stdout, "\n");
      j++;
      break;
    }
    fprintf(stdout, "%c", buf[j]);
  }

  fprintf(stdout, "Position in user list[Element: %d]: ", x);
  for(j; j<40; j++){
    if(buf[j]=='-'){ //if end of string break
      fprintf(stdout, "\n");
      break;
    }
    fprintf(stdout, "%c", buf[j]);
  }

}


void* usr_list_recv_thread_routine(void *args){

  int ret;

  //getting arguments from args used for connection
  receiver_thread_args_t* arg = (receiver_thread_args_t*)args;

  //address structure for user list sender thread
  struct sockaddr_in usrl_sender_address = {0};
  socklen_t usrl_sender_address_len = sizeof(usrl_sender_address);


  //allocating address struct for incoming connection from server
  struct sockaddr_in* server_addr = calloc(1, sizeof(struct sockaddr_in));

  //struct for thead user list receiver thread bind function
  struct sockaddr_in thread_addr = {0};
  thread_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
  thread_addr.sin_family      = AF_INET;
  thread_addr.sin_port        = htons(CLIENT_THREAD_RECEIVER_PORT); // don't forget about network byte order!

  //binding user list receiver thread address to user list receiver thread socket
  ret = bind(arg->socket, (const struct sockaddr*)&thread_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error while binding address to user list receiver thread socket");

  //user list receiver thread listening for incoming connections
  ret = listen(arg->socket, 1);
  ERROR_HELPER(ret, "Cannot listen on user list receiver thread socket");

  fprintf(stderr, "flag 5\n");

  //accepting connection on user list receiver thread socket
  int rec_socket = accept(arg->socket, (struct sockaddr*) &usrl_sender_address, &usrl_sender_address_len); // SOCKET!!!!!!!!!!!
  ERROR_HELPER(ret, "Cannot accept connection on user list receiver thread socket");

  fprintf(stderr, "flag 6\n");

  //number of future elemnts in buf
  int elem_buf_len = 0;
  char* buf = (char*)calloc(USERLIST_BUFF_SIZE,sizeof(char));

  int bytes_read;

  //receiving user list element from server
  //while(1){
    bytes_read = 0;
    bzero(buf, USERLIST_BUFF_SIZE);

    //making sure to read all bytes of the message

    //number of modifications to receive
    while (bytes_read <= USERLIST_BUFF_SIZE) {
        ret = recv(rec_socket, buf + bytes_read, 1, 0);

        fprintf(stderr, "flag 7");

        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Error while receiving number of modifications in user list from server");

        if (buf[bytes_read] == '\n') break; // end of message

        bytes_read++;
    }

    buf[bytes_read] = '\0';
    elem_buf_len = bytes_read;

    //int num_modifiche = atoi(buf); //number of modifications

    fprintf(stderr, "flag 8\n");

    int i;
    for(i =0; i<1; i++){
      bzero(buf, USERLIST_BUFF_SIZE);
      bytes_read = 0;
      elem_buf_len = 0;

      //making sure to read all bytes of the message
      while (bytes_read <= USERLIST_BUFF_SIZE) {
          ret = recv(rec_socket, buf + bytes_read, 1, 0);

          fprintf(stderr, "flag 9\n");

          if (ret == -1 && errno == EINTR) continue;
          ERROR_HELPER(ret, "Error while receiving number of modifications in user list from server");

          if (buf[bytes_read] == '\n') break; // end of message

          bytes_read++;
      }

      fprintf(stderr, "flag 10\n");

      //buf[bytes_read] = '\0';
      elem_buf_len = bytes_read;

      //print elements sent from server (only for test)
      //send elements to function user list element handler
      print_elem_list(buf, i);

    }// end of for loop

    fprintf(stderr, "flag 11\n");

  //} //end of while(1)
  pthread_exit(NULL);

} //end of thread routine


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

  //address structure for listen thread socket
  struct sockaddr_in incoming_client_addr = {0};
  serv_addr.sin_family              = AF_INET;
  serv_addr.sin_port                = htons(CLIENT_THREAD_LISTEN_PORT);
*/

  //socket descriptor for user list receiver thread
  int usrl_recv_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(usrl_recv_socket, "Error while creating user list receiver thread socket descriptor");



/*
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
*/


  //user list receiver thread
  //
  //creating parameters for user list receiver thread funtion
  receiver_thread_args_t* usrl_recv_args = (receiver_thread_args_t*)malloc(sizeof(receiver_thread_args_t));
  usrl_recv_args->socket = usrl_recv_socket;

  //creating and spawning user list receiver thread with parameters
  pthread_t thread_usrl_recv;
  ret = pthread_create(&thread_usrl_recv, NULL, usr_list_recv_thread_routine, (void*)usrl_recv_args);
  PTHREAD_ERROR_HELPER(ret, "Unable to create user list receiver thread");


  //connection to server
  ret = connect(socket_desc, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to server");

  fprintf(stderr, "flag 2\n");

  //sending buffer init data for user list
  //creating buffer for username and availability flag
  char username_buf[17];
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

  //detatching from user list receiver thread
  ret = pthread_join(thread_usrl_recv, NULL);
  PTHREAD_ERROR_HELPER(ret, "Unable to detatch from user list receiver thread");
  //
  //detached from user list receiver thread

  // close socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "Cannot close socket");

  //while(1){} //so that the client doesnt close the connection with the server

  exit(EXIT_SUCCESS);

}
