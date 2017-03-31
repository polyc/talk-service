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

sem_t sync_receiver;
GHashTable* user_list;


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

int get_username(char* username){
  int i;

  fprintf(stdout, "Enter username: ");
  fflush(stdout);
  fgets(username, sizeof(username)+1, stdin);
  fflush(stdin);

  //checking if username has atleast 1 character
  if(strlen(username)==0){
    fprintf(stdout, "No username input\n");
    fflush(stdout);
    return 0;
  }

  //checking if username contains '-' character
  for(i=0; i<sizeof(username); i++){
    if(username[i]=='-'){
      fprintf(stdout, "Char '-' found in username ... input correct username\n");
      return 0; //contains '-' character, username not ok return 0
    }
  }

  strcat(username, "\n"); //concatenating "\n" for server recv function

  return 1; //usrname ok
}

void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command){

  if(mod_command[0] == MODIFY){
    REPLACE(user_list, (gpointer)buf_userName, (gpointer)elem);
    return;
  }
  else{
    REMOVE(user_list, (gpointer)buf_userName);
    return;
  }

}

void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command){

  int i, j;

  mod_command[0] = buf[0];

  for(j=2; j<42; j++){
    if(buf[j]=='-'){ //if end of string break
      j++;
      break;
    }
  }

  strncpy(buf_userName, buf+2, j-3);
  buf_userName[j-3] = '\0';

  i = j;

  for(; j<42; j++){
    if(buf[j]=='-'){ //if end of string break
      j++;
      break;
    }
  }

  strncpy(elem->client_ip, buf+i, j-i-1);
  elem->client_ip[j-i-1] = '\0';

  //checking availability char
  if(buf[j] == AVAILABLE){
    elem->a_flag = AVAILABLE;
  }
  else{
    elem->a_flag = UNAVAILABLE;
  }

  //checking if parsing done right
  fprintf(stdout, "[CHECK USERNAME] %s\n", buf_userName);
  fprintf(stdout, "[CHECK IP] %s\n", elem->client_ip);
  fprintf(stdout, "[CHECK AVAILABILITY] %c\n", elem->a_flag);
  fprintf(stdout, "[CHECK COMMAND] %s\n", mod_command);

  return;

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

  //ublocking &sync_receiver for main process
  ret = sem_post(&sync_receiver);
  ERROR_HELPER(ret, "Error in sem_post on &sync_receiver semaphore (user list receiver thread routine)");

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

      //
      //create struct from string received from server
      //pass struct (which is a usr element) to INSERT function
      //

      //creating struct usr_list_elem_t* for create_elem_list function
      usr_list_elem_t* elem = (usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));
      elem->client_ip = (char*)calloc(INET_ADDRSTRLEN,sizeof(char*));

      //creating buffer for username and command to pass to create_elem_list function
      char* userName = (char*)calloc(USERNAME_BUF_SIZE,sizeof(char));
      char* command = (char*)calloc(1,sizeof(char));

      fprintf(stderr, "flag 9\n");

      //passing string with usr elements to be parsed by create_elem_list
      parse_elem_list(buf, elem, userName, command);

      fprintf(stderr, "flag 10\n");

      //updating elem list
      update_list(userName, elem, command);


    }// end of for loop

    fprintf(stderr, "flag 11\n");

  //} //end of while(1)

  ret = close(rec_socket);
  ERROR_HELPER(ret, "Cannot close socket");

  ret = close(arg->socket);
  ERROR_HELPER(ret, "Cannot close usrl_recv_socket");

  free(buf);

  pthread_exit(NULL);

} //end of thread routine


int main(int argc, char* argv[]){

  int ret;
  fprintf(stderr, "flag 0\n");

  //initializing GLibHashTable for user liste
  user_list = usr_list_init();

  //initializing username buffer
  char* username = (char*)malloc(USERNAME_BUF_SIZE*sizeof(char));

  //getting username from user   add max number of attempts
  while(1){
    ret = get_username(username);
    if(ret==1){
      break;
    }
    username = realloc(username, USERNAME_BUF_SIZE); //reallocating buffer for username

  }

  //creating sempahore for listen function in usrl_liste_thread_routine
  ret = sem_init(&sync_receiver, 0, 0);
  ERROR_HELPER(ret, "[FATAL ERROR] Could not open &sync_receiver semaphore");

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
  t_listen_args.addr   = incoming_client_addr;username

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
  ret = sem_wait(&sync_receiver);
  ERROR_HELPER(ret, "Error in sem_wait (main process)");

  fprintf(stderr, "This flag should appear after flag 5 (flag 5 is in usrl_listen_thread_routine)\n");

  //closing and unlinking &sync_receiver
  ret = sem_destroy(&sync_receiver);
  ERROR_HELPER(ret, "Error destroying &sync_receiver semaphore");

  //wait LISTEN in thread_usrl_rcv!!!!! then go
  //connection to server
  ret = connect(socket_desc, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to server");

  fprintf(stderr, "flag 2\n");

  //sending buffer init data for user list
  //creating buffer for username and availability flag
  char* username_buf_server = (char*)malloc(USERNAME_BUF_SIZE*sizeof(char));
  strncpy(username_buf_server, username, strlen(username));

  //sending username to server
  send_msg(socket_desc, username_buf_server);


  //detatching from user list receiver thread
  ret = pthread_join(thread_usrl_recv, NULL); //should be detatch but its only for test
  PTHREAD_ERROR_HELPER(ret, "Unable to detatch from user list receiver thread");
  //
  //detached from user list receiver thread
  fprintf(stderr, "flag 4\n");

  //print elemets from user list ONLY FOR TEST
  fprintf(stdout, "Found regibald? %d\n", CONTAINS(user_list, "regibald_94"));

  fprintf(stderr, "flag 4.1\n");

  // close client main process socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "Cannot close socket_desc");

  //ret = close(usrl_recv_socket);
  //ERROR_HELPER(ret, "Cannot close usrl_recv_socket");

  exit(EXIT_SUCCESS);

}
