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
#include <fcntl.h>
#include <semaphore.h>
#include <glib.h>

#include "client.h"
#include "common.h"
#include "util.h"

sem_t* listen_sem;


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
  for(; j<40; j++){
    if(buf[j]=='-'){ //if end of string break
      fprintf(stdout, "\n");
      j++;
      break;
    }
    fprintf(stdout, "%c", buf[j]);
  }

  fprintf(stdout, "Availability[Element: %d]: ", x);
  for(; j<40; j++){
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
  struct sockaddr_in* usrl_sender_address = calloc(1, sizeof(struct sockaddr_in));
  socklen_t usrl_sender_address_len = sizeof(usrl_sender_address);


  //struct for thead user list receiver thread bind function
  struct sockaddr_in thread_addr = {0};
  thread_addr.sin_addr.s_addr = inet_addr(LOCAL_IP);
  thread_addr.sin_family      = AF_INET;
  thread_addr.sin_port        = htons(CLIENT_THREAD_RECEIVER_PORT); // don't forget about network byte order!

  fprintf(stderr, "flag 4.5\n");

  //binding user list receiver thread address to user list receiver thread socket
  ret = bind(arg->socket, (const struct sockaddr*)&thread_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error while binding address to user list receiver thread socket");

  fprintf(stderr, "flag 4.6\n");

  //user list receiver thread listening for incoming connections
  ret = listen(arg->socket, 1);
  ERROR_HELPER(ret, "Cannot listen on user list receiver thread socket");

  fprintf(stderr, "flag 5\n");

  //ublocking listen_semaphore for main process
  sem_post(listen_sem);

  //accepting connection on user list receiver thread socket
  int rec_socket = accept(arg->socket, (struct sockaddr*) &usrl_sender_address, &usrl_sender_address_len);
  ERROR_HELPER(ret, "Cannot accept connection on user list receiver thread socket");

  fprintf(stderr, "flag 6\n");

  //buffer for recv_msg function
  char* buf = (char*)calloc(USERLIST_BUFF_SIZE,sizeof(char));

  //receiving user list element from server
  //while(1){
    bzero(buf, USERLIST_BUFF_SIZE);

    //making sure to read all bytes of the message

    //number of modifications to receive
    ret = recv_msg(rec_socket, buf, USERLIST_BUFF_SIZE);
    if(ret != 0){
      fprintf(stderr, "Error while receiving number of modifications from server");
    }


    int num_modifiche = atoi(buf); //number of modifications

    fprintf(stderr, "flag 8\n");

    int i;
    for(i =0; i<num_modifiche; i++){
      bzero(buf, USERLIST_BUFF_SIZE);

      //receiveing user list element i from server
      ret = recv_msg(rec_socket, buf, USERLIST_BUFF_SIZE);
      if(ret != 0){
        fprintf(stderr, "Error while receiving  user list element[%d] from server", i);
      }

      //print elements sent from server (only for test)
      //send elements to function user list element handler
      print_elem_list(buf, i);

    }// end of for loop

    fprintf(stderr, "flag 11\n");

  //} //end of while(1)

  ret = close(rec_socket);
  ERROR_HELPER(ret, "Cannot close socket");

  free(buf);

  pthread_exit(NULL);

} //end of thread routine


int main(int argc, char* argv[]){

  int ret;
  fprintf(stderr, "flag 0\n");

  //getting username from argv
  char* username = argv[1];
  strcat(username, "\n"); //concatenating "\n" for server recv function

  //creating sempahore for listen function in usrl_liste_thread_routine
  listen_sem = sem_open(SEM_LISTEN, O_CREAT | O_EXCL, 0640, 0);
  //handling sem_open errors
  if (listen_sem == SEM_FAILED && errno == EEXIST) {
    fprintf(stderr, "[WARNING] listen_sem semaphore already exits.\n");

    //unlinking already open semaphore
    sem_unlink(SEM_LISTEN);

    // now we can try to create the semaphore from scratch
    listen_sem = sem_open(SEM_LISTEN, O_CREAT | O_EXCL, 0640, 0);
  }

  //if sem_open fails again exit(1)
  if (listen_sem == SEM_FAILED) {
    fprintf(stderr, "[FATAL ERROR] Could not open listen_sem semaphore, the reason is: %s\n", strerror(errno));
    exit(1);
  }


  //socket descriptor to connect to server
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Error while creating client socket descriptor");

  //data structure for the connection to the server
  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family         = AF_INET;
  serv_addr.sin_port           = htons(SERVER_PORT);
  serv_addr.sin_addr.s_addr    = inet_addr(LOCAL_IP);

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

  //waiting for usrl_liste_thread_routine to bind address to socket and to listen
  sem_wait(listen_sem);

  fprintf(stderr, "This flag should appear after flag 5 (flag 5 is in usrl_listen_thread_routine)\n");

  //closing and unlinking listen_sem
  sem_close(listen_sem);
  sem_unlink(SEM_LISTEN);

  //wait LISTEN in thread_usrl_rcv!!!!! then go
  //connection to server
  ret = connect(socket_desc, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to server");

  fprintf(stderr, "flag 2\n");

  //sending buffer init data for user list
  //creating buffer for username and availability flag
  char* username_buf = (char*)malloc(USRNAME_BUF_SIZE*sizeof(char));
  strncpy(username_buf, username, strlen(username));

  //sending username to server
  send_msg(socket_desc, username_buf);


  //detatching from user list receiver thread
  ret = pthread_join(thread_usrl_recv, NULL); //should be detatch but its only for test
  PTHREAD_ERROR_HELPER(ret, "Unable to detatch from user list receiver thread");
  //
  //detached from user list receiver thread


  fprintf(stderr, "flag 4\n");

  // close socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "Cannot close socket_desc");

  ret = close(usrl_recv_socket);
  ERROR_HELPER(ret, "Cannot close usrl_recv_socket");

  exit(EXIT_SUCCESS);

}
