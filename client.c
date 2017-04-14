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
sem_t sync_connect_listen;
GHashTable* user_list;

char* USERNAME;
char* available;
char* unavailable;
char* disconnect;

static volatile int keepRunning = 1;

void main_interrupt_handler(int dummy){
    keepRunning = 0;
}

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

int list_command(char* command){
  int ret;

  if(strcmp("list", command)==0){

    ret = sem_wait(&sync_userList);
    ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in wait function on sync_userList semaphore\n");

    FOR_EACH(user_list, (GHFunc)print_userList, NULL); //printing hashtable

    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_userList semaphore\n");

    return 1;
  }
  return 0;
}

void* listen_routine(void* args){

  int ret;
  int sem_value;

  client_thread_args_t* arg = (client_thread_args_t*)args;

  while(1){

    //socket descriptor for listen thread
    int thread_socket = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(thread_socket, "[LISTEN_ROUTINE] Error while creating thread_socket");

    //struct for thead user list receiver thread bind function
    struct sockaddr_in thread_addr = {0};
    thread_addr.sin_family         = AF_INET;
    thread_addr.sin_port           = htons(CLIENT_THREAD_LISTEN_PORT); // don't forget about network byte order!
    thread_addr.sin_addr.s_addr    = INADDR_ANY;

    //binding listen thread address to listen thread socket
    ret = bind(thread_socket, (const struct sockaddr*)&thread_addr, sizeof(struct sockaddr_in));
    ERROR_HELPER(ret, "[LISTEN_ROUTINE] Error while binding address to thread socket");

    fprintf(stdout, "[LISTEN_ROUTINE] binded address to listen thread socket\n");

    //thread_listen listening for incoming connections
    ret = listen(thread_socket, 1);
    ERROR_HELPER(ret, "[LISTEN_ROUTINE] Cannot listen on listen_thread socket");

    fprintf(stdout, "[LISTEN_ROUTINE] listening on listen thread socket\n");

    //address structure for user list sender thread
    struct sockaddr_in* client_address = (struct sockaddr_in*)calloc(1, sizeof(struct sockaddr_in));
    socklen_t client_address_len = sizeof(client_address);

    sem_getvalue(&sync_connect_listen, &sem_value);

    if(sem_value < 1){
      continue;
    }

    int client_socket = accept(thread_socket, (struct sockaddr*) &client_address, &client_address_len);
    ERROR_HELPER(ret, "[LISTEN_ROUTINE] Cannot accept connection on user list receiver thread socket");

    //sending unavailability to server
    send_msg(arg->socket, unavailable);

    ret = sem_wait(&sync_connect_listen);
    ERROR_HELPER(ret, "[LISTEN_ROUTINE] Error in wait function on sync_connect_listen semaphore\n");

    char* buf_answer = (char*)malloc(sizeof(char));

    fprintf(stdout, "[LISTEN_ROUTINE] Accept incoming connection?[Y,N]: \n");
    fgets(buf_answer, 1, stdin);

    if(buf_answer[0] == 'N' || buf_answer[0] == 'n'){
      close(thread_socket);
      free(client_address);

      //sending availability to server
      send_msg(arg->socket, available);

      ret = sem_post(&sync_connect_listen);
      ERROR_HELPER(ret, "[LISTEN_ROUTINE] Error in post function on sync_connect_listen semaphore\n");

      free(buf_answer);
      free(client_address);

      continue;
    }

    free(buf_answer);

    fprintf(stdout, "[LISTEN_ROUTINE] incoming connection accepted...waiting for username\n");

    char* client_username = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

    //receiveing username from client
    ret = recv_msg(client_socket, client_username, USERNAME_BUF_SIZE);
    if(ret != 0){
      fprintf(stderr, "[LISTEN_ROUTINE] Error while receiving  username from client\n");
    }

    fprintf(stdout, "[LISTEN_ROUTINE] username received\n");

    ret = chat_session(client_username, client_socket);

    free(client_username);
    free(client_address);

    //sending availability to server
    send_msg(arg->socket, available);

    ret = sem_post(&sync_connect_listen);
    ERROR_HELPER(ret, "[LISTEN_ROUTINE] Error in post function on sync_connect_listen semaphore\n");

  }

  //rememeber to free stuff

}

void* connect_routine(void* args){

  int ret;

  //socket descriptor for connect thread
  int connect_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(connect_socket, "[CONNECT_ROUTINE] Error while creating connect thread socket descriptor");

  client_thread_args_t* arg = (client_thread_args_t*)args;

  //if no users connected exit connect_routine
  if(g_hash_table_size(user_list) == 0){

    fprintf(stdout, "[CONNECT_ROUTINE] No user to connect to...Sorry\n");

    pthread_exit(NULL);
  }

  //sending availability to server
  send_msg(arg->socket, unavailable);
  fprintf(stdout, "[CONNECT_ROUTINE] unavailable:  %s\n", unavailable);

  //mutual exlusion on user_list hashtable
  ret = sem_wait(&sync_userList);
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in wait function on sync_userList semaphore\n");

  FOR_EACH(user_list, (GHFunc)print_userList, NULL); //printing hashtable

  ret = sem_post(&sync_userList);
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_userList semaphore\n");

  //buffer for target to connect to
  char* target = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
  strcpy(target, "");
  //getting information of the target
  usr_list_elem_t* target_elem;

  //checking for correct username to connect to
  while(1){

    //letting user decide who to connect to
    fprintf(stdout, "[CONNECT_ROUTINE] Connect to: ");

    fgets(target, USERNAME_BUF_SIZE, stdin);

    strtok(target, "\n");

    char* username_cpy = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
    strncpy(username_cpy, USERNAME, USERNAME_BUF_SIZE);
    strtok(username_cpy, "\n");

    if(strcmp(target, username_cpy)==0){

      fprintf(stdout, "[CONNECT_ROUTINE] Impossible to connect with yourself...\n");
      free(username_cpy);
      continue;
    }

    free(username_cpy);

    if(list_command(target)==1){

      continue;
    }

    if(strcmp(target, "exit connect")==0){

      fprintf(stdout, "[CONNECT_ROUTINE] exiting connect routine\n");

      //sending availability to server
      send_msg(arg->socket, available);

      ret = sem_post(&sync_connect_listen);
      ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_connect_listen semaphore\n");

      free(target);

      pthread_exit(NULL);
    }

    ret = sem_wait(&sync_userList);
    ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in wait function on sync_userList semaphore\n");

    target_elem = (usr_list_elem_t*)LOOKUP(user_list, (gconstpointer)target);

    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_userList semaphore\n");

    if(target_elem != NULL){
      if((target_elem->a_flag) == UNAVAILABLE){
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

  ret = connect(connect_socket, (struct sockaddr*) &target_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error trying to connect to target");

  //sending username to receiver client so he knows who it is
  send_msg(connect_socket, USERNAME);

  fprintf(stdout, "[CONNECT_ROUTINE] Connected to %s passing arguments to chat_session\n", target);

  //passing username and connected socket to chat session function
  ret = chat_session(target, connect_socket);
  //check cheat_session return value

  //sending availability to server
  send_msg(arg->socket, available);

  ret = sem_post(&sync_connect_listen);
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_connect_listen semaphore\n");

  // close connection to client socket
  ret = close(connect_socket);
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Cannot close connect_socket");

  free(target);

  pthread_exit(NULL);

}

int get_username(char* username, int socket){
  int i, ret;

  fprintf(stdout, "[GET_USERNAME] Enter username: ");

  username = fgets(username, USERNAME_BUF_SIZE, stdin);

  //checking if username has atleast 1 character
  if(strlen(username)==1){ //because there is a \n got from fgets
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

  //sending buffer init data for user list
  //creating buffer for username and availability flag
  char* buf = (char*)calloc(USERNAME_BUF_SIZE,sizeof(char));
  strncpy(buf, username, strlen(username));

  //sending username to server
  send_msg(socket, buf);

  fprintf(stdout, "[GET_USERNAME] sent username [%s] to server\n", buf);

  //checking if username already in use
  bzero(buf, USERNAME_BUF_SIZE);

  ret = recv_msg(socket, buf, 2);
  ERROR_HELPER(ret, "[GET_USERNAME] Error receiving username check from server");

  fprintf(stdout, "[GET_USERNAME] Username available? [%c]\n", buf[0]);

  if(buf[0] == UNAVAILABLE){
    free(buf);
    return 0;
  }
  free(buf);
  return 1; //usrname ok
}

void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command){

  int ret;

  ret = sem_wait(&sync_userList);
  ERROR_HELPER(ret, "[UPDATE_LIST] Error in wait function on sync_userList semaphore\n");

  //distiguere tra modify new e delete se e' modify usare LOOKUP
  if(mod_command[0] == NEW){
    INSERT(user_list, (gpointer)buf_userName, (gpointer)elem);

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

      elem_buf = (char*)POP(buf, POP_TIMEOUT);

      if(elem_buf != NULL){
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

    free(command);

  }
}

void* usr_list_recv_thread_routine(void* args){

  int ret;

  //socket descriptor for user list receiver thread
  int usrl_recv_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(usrl_recv_socket, "[MAIN] Error while creating user list receiver thread socket descriptor");

  //creating buffers to store modifications sent by server
  GAsyncQueue* buf_modifications = g_async_queue_new();

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
  thread_addr.sin_addr.s_addr = INADDR_ANY;

  fprintf(stderr, "[RECV_THREAD_ROUTINE] created sockaddr_in struct for bind function\n");

  //we enable SO_REUSEADDR to quickly restart our server after a crash
  int reuseaddr_opt = 1;
  ret = setsockopt(usrl_recv_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot set SO_REUSEADDR option");

  //binding user list receiver thread address to user list receiver thread socket
  ret = bind(usrl_recv_socket, (const struct sockaddr*)&thread_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Error while binding address to user list receiver thread socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] address binded to socket\n");

  //ublocking &sync_receiver for main process
  ret = sem_post(&sync_receiver);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Error in sem_post on &sync_receiver semaphore (user list receiver thread routine)");

  //user list receiver thread listening for incoming connections
  ret = listen(usrl_recv_socket, 1);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot listen on user list receiver thread socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] listening on socket for server connection\n");

  //accepting connection from server on user list receiver thread socket
  int rec_socket = accept(usrl_recv_socket, (struct sockaddr*) &usrl_sender_address, &usrl_sender_address_len);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot accept connection on user list receiver thread socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] accepted connection from server\n");

  //buffer for recv_msg function
  char* buf = (char*)calloc(USERLIST_BUFF_SIZE,sizeof(char));

  //receiving user list element from server
  while(1){
    bzero(buf, USERLIST_BUFF_SIZE);

      //receiveing user list element from server
      ret = recv_msg(rec_socket, buf, USERLIST_BUFF_SIZE);

      if(ret==-1){
        continue;
      }

      fprintf(stderr, "[RECV_THREAD_ROUTINE] accepted connection from server\n");

      char* queueBuf_elem = (char*)malloc(strlen(buf)*sizeof(char));

      memcpy(queueBuf_elem, buf, strlen(buf));

      g_async_queue_push(buf_modifications, (gpointer)queueBuf_elem);

  } //end of while(1)


  fprintf(stderr, "[RECV_THREAD_ROUTINE] closing rec_socket and arg->socket...\n");

  ret = close(rec_socket);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot close socket");

  ret = close(usrl_recv_socket);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot close usrl_recv_socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] closed rec_socket and arg->socket succesfully\n");

  //joining manage_updates thread
  ret = pthread_join(manage_updates, NULL);
  PTHREAD_ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Unable to join manage_updates thread");

  free(buf);
  free(usrl_sender_address);
  g_async_queue_unref(buf_modifications);

  fprintf(stderr, "[RECV_THREAD_ROUTINE] exiting usr_list_recv_thread_routine\n");

  pthread_exit(NULL);

} //end of thread routine

void* recv_routine(void* args){

  int ret;

  chat_session_args_t* arg = (chat_session_args_t*)args;

  char* buf = (char*)calloc(MSG_LEN, sizeof(char));

  while(1){
    bzero(buf, MSG_LEN);


    ret = recv_msg(arg->socket, buf, MSG_LEN);
    ERROR_HELPER(ret, "[RECV_ROUTINE] Error in recv_msg");

    if(strcmp(buf, "exit")==0){
      //do something (Ex. sem_post on a semaphore)

      break;
    }

    fprintf(stdout, "\n[%s] %s\n", arg->username, buf);

  }

  free(buf);

  ret = sem_post(&sync_chat);
  ERROR_HELPER(ret, "[RECV_ROUTINE] Error in post function on sync_userList semaphore");

}

void* send_routine(void* args){

  int ret;

  chat_session_args_t* arg = (chat_session_args_t*)args;

  char* buf = (char*)calloc(MSG_LEN, sizeof(char));

  while(1){
    bzero(buf, MSG_LEN);

    fgets(buf, MSG_LEN, stdin);

    strtok(buf, "\n");

    if(strcmp(buf, "exit")==0){
      //do something

      //sending exit message
      send_msg(arg->socket, buf);
      break;
    }

    send_msg(arg->socket, buf);

  }

  free(buf);

  ret = sem_post(&sync_chat);
  ERROR_HELPER(ret, "[SEND_ROUTINE] Error in post function on sync_userList semaphore\n");

}

int chat_session(char* username, int socket){

  int ret;

  //display welcome msg
  fprintf(stdout, "[CHAT_SESSION] You are chatting with [%s]\n", username);
  fflush(stdout);

  //initiate thread for recv and for send
  pthread_t chat_recv;
  pthread_t chat_send;

  //initiate arguments for both threads chat_recv and chat_send
  chat_session_args_t* args = (chat_session_args_t*)malloc(sizeof(chat_session_args_t));
  args->socket   = socket;
  args->username = username;

  //creating thread chat_recv
  ret = pthread_create(&chat_recv, NULL, recv_routine, (void*)args);
  PTHREAD_ERROR_HELPER(ret, "[CHAT_SESSION] Unable to create chat_recv thread");

  //creating thread chat_send
  ret = pthread_create(&chat_send, NULL, send_routine, (void*)args);
  PTHREAD_ERROR_HELPER(ret, "[CHAT_SESSION] Unable to create chat_send thread");

  //mutual exlusion on chat
  ret = sem_wait(&sync_chat);
  ERROR_HELPER(ret, "[CHAT_SESSION] Error in wait function on sync_chat semaphore");

  free(args);

}

int main(int argc, char* argv[]){

  int ret;
  fprintf(stdout, "[MAIN] starting main process\n");

  signal(SIGINT, main_interrupt_handler);

  //initializing GLibHashTable for user list
  user_list = usr_list_init();

  //alocating buffers for availability
  available   = (char*)calloc(2,sizeof(char));
  unavailable = (char*)calloc(2,sizeof(char));
  disconnect  = (char*)calloc(2,sizeof(char));

  //copying availability commands into buffers
  fprintf(stdout, "[MAIN] strlen = %d\n", strlen("a\n"));
  strcpy(available,   "a\n");
  strcpy(unavailable, "u\n");
  strcpy(disconnect,  "c\n");

  fprintf(stdout, "[MAIN][BUFF_TEST] %c, %c, %s", available[0], unavailable[0], disconnect);

  fprintf(stdout, "[MAIN] initializing semaphores...\n");

  //creating sempahore for listen function in usrl_liste_thread_routine for bind action
  ret = sem_init(&sync_receiver, 0, 0);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_receiver semaphore");

  //creating sempahore for mutual exlusion in userList
  ret = sem_init(&sync_userList, 0, 1);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_userList semaphore");

  //creating sempahore for syncronization between connect_routine and listen_routine
  ret = sem_init(&sync_connect_listen, 0, 1);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_connect_listen");

  //creating sempahore for syncronization between chat threads
  ret = sem_init(&sync_chat, 0, 0);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_chat");

  fprintf(stdout, "[MAIN] semaphores initialized correctly\n");

  //socket descriptor to connect to server
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "[MAIN] Error while creating client socket descriptor");

  //data structure for the connection to the server
  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family         = AF_INET;
  serv_addr.sin_port           = htons(SERVER_PORT);
  serv_addr.sin_addr.s_addr    = inet_addr(SERVER_IP);

  fprintf(stdout, "[MAIN] created data structure for connection with server\n");

  //thread listen
  //
  //creating parameters for listen thread
  client_thread_args_t* listen_thread_args = (client_thread_args_t*)malloc(sizeof(client_thread_args_t));
  listen_thread_args->socket = socket_desc;

  //creating and spawning thread listen with parameters
  pthread_t thread_listen;
  ret = pthread_create(&thread_listen, NULL, listen_routine, (void*)listen_thread_args);
  PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to create listen_thread");

  //detatching from listen_thread
  ret = pthread_detach(thread_listen);                                          //cancell because main process will join on this thread
  PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to detatch from listen_thread");     //when handling SIGINT
  //
  //detached from thread listen


  //user list receiver thread
  //
  //creating and spawning user list receiver thread with parameters
  pthread_t thread_usrl_recv;
  ret = pthread_create(&thread_usrl_recv, NULL, usr_list_recv_thread_routine, NULL);
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

  //initializing username buffer
  USERNAME = (char*)malloc(USERNAME_BUF_SIZE*sizeof(char));

  //getting username from user   add max number of attempts
  while(1){
    ret = get_username(USERNAME, socket_desc);
    if(ret==1){
      break;
    }
    bzero(USERNAME, USERNAME_BUF_SIZE); //setting buffer to 0
    strcpy(USERNAME, "");
  }

  strtok(USERNAME, "\n");

  fprintf(stdout, "[MAIN] got username: [%s]\n", USERNAME);

  //creating parameters for connect thread funtion
  client_thread_args_t* connect_thread_args = (client_thread_args_t*)malloc(sizeof(client_thread_args_t));
  connect_thread_args->socket = socket_desc;

  char* buf_commands = (char*)malloc(8*sizeof(char));

  pthread_t connect_thread;

  while(keepRunning){

    //if(sem_sync_connect_listen < 0){continue;}

    bzero(buf_commands, 8);
    fprintf(stdout, "[MAIN] exit/connect: ");
    fgets(buf_commands, 8, stdin);

    strtok(buf_commands, "\n");

    if(list_command(buf_commands)==1){
      continue;
    }

    if(strcmp(buf_commands, "connect")==0){

      ret = sem_wait(&sync_connect_listen);
      ERROR_HELPER(ret, "[MAIN] Error in wait function on sync_connect_listen semaphore\n");

      fprintf(stderr, "[MAIN] Creating connect_thread...\n");

      pthread_t connect_thread;
      ret = pthread_create(&connect_thread, NULL, connect_routine, (void*)connect_thread_args);
      PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to create connect thread");

      fprintf(stderr, "[MAIN] Created connect_thread\n");

      ret = pthread_join(connect_thread, NULL);
      PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to join connect thread");

      continue;
    }

    else if(strcmp(buf_commands, "exit")==0){
      break;
    }

  }

  fprintf(stdout, "\n[MAIN] catched signal CTRL-C...\n");

  //sending disconnect command to server
  send_msg(socket_desc, disconnect);

  fprintf(stdout, "[MAIN] destroying semaphores....\n");

  ret = sem_destroy(&sync_userList);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_userList semaphore");

  ret = sem_destroy(&sync_connect_listen);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_connect_listen semaphore");

  ret = sem_destroy(&sync_chat);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_chat semaphore");

  fprintf(stdout, "[MAIN] freeing allocated data...\n");

  free(USERNAME);
  free(buf_commands);
  free(connect_thread_args);
  free(available);
  free(unavailable);
  free(disconnect);
  free(listen_thread_args);

  fprintf(stdout, "[MAIN] closing socket descriptors...\n");

  // close client main process socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "[MAIN] Cannot close socket_desc");

  fprintf(stdout, "[MAIN] exiting main process\n\n");

  exit(EXIT_SUCCESS);

}
