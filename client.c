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
sem_t sync_userList;
sem_t sync_chat;
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

static void print_userList(gpointer key, gpointer elem, gpointer data){

  char* username = (char*)key;

  usr_list_elem_t* usr_elem = (usr_list_elem_t*)elem;

  fprintf(stdout, "\n[PRINT_USERLIST]###############################################\n");
  fprintf(stdout, "[PRINT_USERLIST] Username: %s\n", username);
  fprintf(stdout, "[PRINT_USERLIST] IP:       %s\n", usr_elem->client_ip);

  if(usr_elem->a_flag == AVAILABLE){
    fprintf(stdout, "[PRINT_USERLIST] Flag:     AVAILABLE\n");
  }
  else{
    fprintf(stdout, "[PRINT_USERLIST] Flag:     UNAVAILABLE\n");
  }

  fprintf(stdout, "[PRINT_USERLIST]###############################################\n\n");
  fflush(stdout);
  return;
}

int get_username(char* username){
  int i;

  fprintf(stdout, "[GET_USERNAME] Enter username: ");
  fflush(stdout);
  username = fgets(username, USERNAME_BUF_SIZE, stdin);
  //fflush(stdin);

  //checking if username has atleast 1 character
  if(strlen(username)==0){
    fprintf(stdout, "[GET_USERNAME] No username input\n");
    fflush(stdout);
    return 0;
  }

  //checking if username contains '-' character
  for(i=0; i<sizeof(username); i++){
    if(username[i]=='-'){
      fprintf(stdout, "[GET_USERNAME] Char '-' found in username ... input correct username\n");
      fflush(stdout);
      return 0; //contains '-' character, username not ok return 0
    }
  }
  strcat(username, "\n");

  return 1; //usrname ok
}

void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command){

  int ret;

  ret = sem_wait(&sync_userList);
  ERROR_HELPER(ret, "[UPDATE_LIST] Error in wait function on sync_userList semaphore\n");

  //distiguere tra modify new e delete se e' modify usare LOOKUP
  if(mod_command[0] == NEW){
    REPLACE(user_list, (gpointer)buf_userName, (gpointer)elem);

    fprintf(stdout, "[UPDATE_LIST] created new entry in user list\n");

    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[UPDATE_LIST] Error in post function on sync_userList semaphore\n");

    return;
  }
  else if(mod_command[0] == MODIFY){
    usr_list_elem_t* element = (usr_list_elem_t*)LOOKUP(user_list, (gconstpointer)buf_userName);
    element->a_flag = elem->a_flag;

    fprintf(stdout, "[UPDATE_LIST] modified entry [%s] in user list\n", buf_userName);

    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[UPDATE_LIST] Error in post function on sync_userList semaphore\n");

    return;
  }
  else{
    REMOVE(user_list, (gpointer)buf_userName);

    fprintf(stdout, "[UPDATE_LIST] removed entry [%s] in user list\n", buf_userName);

    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[UPDATE_LIST] Error in post function on sync_userList semaphore\n");

    return;
  }

}

void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command){

  fprintf(stdout, "[PARSE_ELEM_LIST] inside parse_element function\n");

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

  fprintf(stdout, "[PARSE_ELEM_LIST] passed first strncpy()\n");

  i = j;

  for(; j<42; j++){
    if(buf[j]=='-'){ //if end of string break
      j++;
      break;
    }
  }

  strncpy(elem->client_ip, buf+i, j-i-1);
  elem->client_ip[j-i-1] = '\0';

  fprintf(stdout, "[PARSE_ELEM_LIST] passed second strncpy()\n");

  //checking availability char

  if(buf[j] == AVAILABLE){
    elem->a_flag = AVAILABLE;
  }
  else{
    elem->a_flag = UNAVAILABLE;
  }


  //elem->a_flag = buf[j];

  //checking if parsing done right
  fprintf(stdout, "[PARSE_ELEM_LIST] [CHECK USERNAME]     %s\n", buf_userName);
  fprintf(stdout, "[PARSE_ELEM_LIST] [CHECK IP]           %s\n", elem->client_ip);
  fprintf(stdout, "[PARSE_ELEM_LIST] [CHECK AVAILABILITY] %c\n", elem->a_flag);
  fprintf(stdout, "[PARSE_ELEM_LIST] [CHECK COMMAND]      %s\n", mod_command);

  return;

}

void* read_updates(void* args){

  fprintf(stdout, "[READ_UPDATES] inside function read_updates\n");

  GAsyncQueue* buf = (GAsyncQueue*)args;

  while(1){

    char* elem_buf;
    fprintf(stdout, "[READ_UPDATES] inside while(1)\n");
    while(1){

      //elem_buf = (char*)g_async_queue_try_pop(buf);

      if((elem_buf = (char*)g_async_queue_try_pop(buf)) != NULL){
        break;
      }
    }
    fprintf(stdout, "[READ_UPDATES] poped element from queue\n");

    fprintf(stdout, "[READ_UPDATES] %s\n", elem_buf);

    //creating struct usr_list_elem_t* for create_elem_list function
    usr_list_elem_t* elem = (usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));
    elem->client_ip = (char*)calloc(INET_ADDRSTRLEN,sizeof(char*));

    //creating buffer for username and command to pass to create_elem_list function
    char* userName = (char*)calloc(USERNAME_BUF_SIZE,sizeof(char));
    char* command = (char*)calloc(1,sizeof(char)); //ricordare che hai cambiato prima era una calloc

    fprintf(stdout, "[READ_UPDATES] passing element to parse function\n");

    //passing string with usr elements to be parsed by create_elem_list
    parse_elem_list(elem_buf, elem, userName, command);

    free(elem_buf);

    fprintf(stderr, "[READ_UPDATES] parsed string to create user element in user list\n");

    //updating elem list
    update_list(userName, elem, command);

    fprintf(stderr, "[READ_UPDATES] updated user list\n");
  }
}

void* usr_list_recv_thread_routine(void* args){

  int ret;

  //getting arguments from args used for connection
  client_thread_args_t* arg = (client_thread_args_t*)args;

  //creating buffers to store modifications sent by server
  GAsyncQueue* buf_modifications =g_async_queue_new();

  //creating thread to manage updates to user list
  pthread_t manage_updates;
  ret = pthread_create(&manage_updates, NULL, read_updates, (void*)buf_modifications);
  PTHREAD_ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Unable to create manage_updates thread");

  //address structure for user list sender thread
  struct sockaddr_in* usrl_sender_address = (struct sockaddr_in*)calloc(1, sizeof(struct sockaddr_in));
  socklen_t usrl_sender_address_len = sizeof(usrl_sender_address);


  //struct for thead user list receiver thread bind function
  struct sockaddr_in thread_addr = {0};
  thread_addr.sin_family      = AF_INET;
  thread_addr.sin_port        = htons(CLIENT_THREAD_RECEIVER_PORT); // don't forget about network byte order!
  thread_addr.sin_addr.s_addr = inet_addr(LOCAL_IP);

  fprintf(stderr, "[RECV_THREAD_ROUTINE] created sockaddr_in struct for bind function\n");

  //binding user list receiver thread address to user list receiver thread socket
  ret = bind(arg->socket, (const struct sockaddr*)&thread_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Error while binding address to user list receiver thread socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] address binded to socket\n");

  //user list receiver thread listening for incoming connections
  ret = listen(arg->socket, 1);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot listen on user list receiver thread socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] listening on socket for server connection\n");

  //ublocking &sync_receiver for main process
  ret = sem_post(&sync_receiver);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Error in sem_post on &sync_receiver semaphore (user list receiver thread routine)");

  //accepting connection on user list receiver thread socket
  int rec_socket = accept(arg->socket, (struct sockaddr*) &usrl_sender_address, &usrl_sender_address_len);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot accept connection on user list receiver thread socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] accepted connection from server\n");

  //buffer for recv_msg function
  char* buf = (char*)calloc(USERLIST_BUFF_SIZE,sizeof(char));

  //receiving user list element from server
  //while(1){
    bzero(buf, USERLIST_BUFF_SIZE);

      //receiveing user list element from server
      ret = recv_msg(rec_socket, buf, USERLIST_BUFF_SIZE);
      if(ret != 0){
        fprintf(stderr, "[RECV_THREAD_ROUTINE] Error while receiving  user list element from server\n");
      }

      char* queueBuf_elem = (char*)malloc(strlen(buf)*sizeof(char));

      memcpy(queueBuf_elem, buf, strlen(buf));

      g_async_queue_push(buf_modifications, (gpointer)queueBuf_elem);

  //} //end of while(1)

  fprintf(stderr, "[RECV_THREAD_ROUTINE] closing rec_socket and arg->socket...\n");

  ret = close(rec_socket);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot close socket");

  ret = close(arg->socket);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot close usrl_recv_socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] closed rec_socket and arg->socket succesfully\n");

  //joining manage_updates thread
  ret = pthread_join(manage_updates, NULL);
  PTHREAD_ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Unable to join manage_updates thread");

  free(buf);
  g_async_queue_unref(buf_modifications);

  fprintf(stderr, "[RECV_THREAD_ROUTINE] exiting usr_list_recv_thread_routine\n");

  pthread_exit(NULL);

} //end of thread routine

void* recv_routine(void* args){}

void* send_routine(void* args){}

void chat_session(char* username, int socket){

  int ret;

  //display welcome msg
  fprintf(stdout, "[CHAT_SESSION] You are chatting with [%s]\n", username);
  fflush(stdout);

  //initiate thread for recv and for send
  pthread_t chat_recv;
  pthread_t chat_send;

  //initiate arguments for both threads chat_recv and chat_send
  client_thread_args_t* args = (client_thread_args_t*)malloc(sizeof(client_thread_args_t));
  args->socket = socket;

  //creating thread chat_recv
  ret = pthread_create(&chat_recv, NULL, recv_routine, (void*)args);
  PTHREAD_ERROR_HELPER(ret, "[CHAT_SESSION] Unable to create chat_recv thread");

  //creating thread chat_send
  ret = pthread_create(&chat_send, NULL, send_routine, (void*)args);
  PTHREAD_ERROR_HELPER(ret, "[CHAT_SESSION] Unable to create chat_send thread");


}

void* connect_routine(void* args){

  int ret;

  client_thread_args_t* arg = (client_thread_args_t*)args;

  //while(1){ //start of while(1)

    while(g_hash_table_size(user_list) == 0){

    }

    ret = sem_wait(&sync_userList);
    ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in wait function on sync_userList semaphore\n");

    FOR_EACH(user_list, (GHFunc)print_userList, NULL); //printing hashtable

    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_userList semaphore\n");

    //buffer for target to connect to
    char* target = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

    //getting information of the target
    usr_list_elem_t* target_elem;

    //checking for correct username to connect to
    while(1){

      //letting user decide who to connect to
      fprintf(stdout, "[CONNECT_ROUTINE] Connect to: ");

      fgets(target, USERNAME_BUF_SIZE, stdin);

      strtok(target, "\n");

      ret = sem_wait(&sync_userList);
      ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in wait function on sync_userList semaphore\n");

      target_elem = (usr_list_elem_t*)LOOKUP(user_list, (gconstpointer)target);

      ret = sem_post(&sync_userList);
      ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_userList semaphore\n");

      if(target_elem != NULL){
        if(target_elem->a_flag == UNAVAILABLE){
          fprintf(stdout, "[CONNECT_ROUTINE] Client not available...\n");
          continue;
        }
        break;
      }
      fprintf(stdout, "[CONNECT_ROUTINE] Username [%s] not found...\n", target);
    }

    fprintf(stdout, "[CONNECT_ROUTINE] Got username...creating struct for connection\n");

    //creating struct for target connection
    struct sockaddr_in target_addr = {0};
    target_addr.sin_family         = AF_INET;
    target_addr.sin_port           = htons(CLIENT_THREAD_LISTEN_PORT);
    target_addr.sin_addr.s_addr    = inet_addr(target_elem->client_ip);

    fprintf(stdout, "[CONNECT_ROUTINE] Connecting to %s on ip: %s...\n", target, target_elem->client_ip);

    ret = connect(arg->socket, (struct sockaddr*) &target_addr, sizeof(struct sockaddr_in));
    ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error trying to connect to target");

    fprintf(stdout, "[CONNECT_ROUTINE] Connected to %s passing arguments to chat_session\n", target);

    chat_session(target, arg->socket);

  //} //end of while(1)

  pthread_exit(NULL);

}

int main(int argc, char* argv[]){

  int ret;
  fprintf(stdout, "[MAIN] starting main process\n");

  //initializing GLibHashTable for user list
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

  //fflush(stdin);

  fprintf(stdout, "[MAIN] got username\n");

  //creating sempahore for listen function in usrl_liste_thread_routine
  ret = sem_init(&sync_receiver, 0, 0);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not open &sync_receiver semaphore");

  //creating sempahore for mutual exlusion in userList
  ret = sem_init(&sync_userList, 0, 1);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not open &sync_userList semaphore");

  //socket descriptor to connect to server
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "[MAIN] Error while creating client socket descriptor");

  //data structure for the connection to the server
  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family         = AF_INET;
  serv_addr.sin_port           = htons(SERVER_PORT);
  serv_addr.sin_addr.s_addr    = inet_addr(LOCAL_IP);

  fprintf(stdout, "[MAIN] created data structure for connection with server\n");

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
  ERROR_HELPER(usrl_recv_socket, "[MAIN] Error while creating user list receiver thread socket descriptor");




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
  client_thread_args_t* usrl_recv_args = (client_thread_args_t*)malloc(sizeof(client_thread_args_t));
  usrl_recv_args->socket = usrl_recv_socket;

  //creating and spawning user list receiver thread with parameters
  pthread_t thread_usrl_recv;
  ret = pthread_create(&thread_usrl_recv, NULL, usr_list_recv_thread_routine, (void*)usrl_recv_args);
  PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to create user list receiver thread");

  fprintf(stdout, "[MAIN] waiting for usrl_listen_thread_routine to bind address\n");

  //waiting for usrl_liste_thread_routine to bind address to socket and to listen
  ret = sem_wait(&sync_receiver);
  ERROR_HELPER(ret, "[MAIN] Error in sem_wait (main process)");

  fprintf(stdout, "[MAIN] destroying sync_receiver semaphore\n");

  //closing &sync_receiver
  ret = sem_destroy(&sync_receiver);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_receiver semaphore");

  //connection to server
  ret = connect(socket_desc, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "[MAIN] Error trying to connect to server");

  fprintf(stdout, "[MAIN] connected to server\n");

  //sending buffer init data for user list
  //creating buffer for username and availability flag
  char* username_buf_server = (char*)malloc(USERNAME_BUF_SIZE*sizeof(char));
  strncpy(username_buf_server, username, strlen(username));

  //sending username to server
  send_msg(socket_desc, username_buf_server);

  fprintf(stdout, "[MAIN] sent username to server\n");

  //joining thread_usrl_recv
  //ret = pthread_join(thread_usrl_recv, NULL); //should be detatch but its only for test
  //PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to join user list receiver thread");

  fprintf(stderr, "[MAIN] finished waiting for thread_usrl_recv to finish routine\n");

  //print elemets from user list ONLY FOR TEST
  //FOR_EACH(user_list, print_userList, NULL); //printing hashtable

  //connect thread
  //
  //socket descriptor for connect thread
  int connect_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(connect_socket, "[MAIN] Error while creating connect thread socket descriptor");

  //creating parameters for connect thread funtion
  client_thread_args_t* connect_thread_args = (client_thread_args_t*)malloc(sizeof(client_thread_args_t));
  connect_thread_args->socket = connect_socket;

  pthread_t connect_thread;
  ret = pthread_create(&connect_thread, NULL, connect_routine, (void*)connect_thread_args);
  PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to create connect thread");

  fprintf(stderr, "[MAIN] Created connect_thread\n");

  ret = pthread_join(connect_thread, NULL); //should be detatch but its only for test
  PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to join connect_thread");
  //
  //

  // close client main process socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "[MAIN] Cannot close socket_desc");

  ret = sem_destroy(&sync_userList);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_userList semaphore");

  //ret = close(usrl_recv_socket);
  //ERROR_HELPER(ret, "Cannot close usrl_recv_socket");

  fprintf(stdout, "[MAIN] exiting main process\n");

  exit(EXIT_SUCCESS);

}
