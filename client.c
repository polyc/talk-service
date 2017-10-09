#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <semaphore.h>
#include <glib.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <signal.h>
#include <bits/sigaction.h>

#include "client.h"
#include "common.h"
#include "util.h"

sem_t sync_receiver;
sem_t sync_userList;
sem_t wait_response;
GHashTable* user_list;

char* USERNAME;
char* USERNAME_CHAT;
char* buf_commands;
int   IS_CHATTING;  // 0 se non connesso 1 se connesso
int   WAITING_RESPONSE;
int   GLOBAL_EXIT;  // 1 se bisogna interrompere il programma 0 altrimenti


void sigHandler(int sig){

  GLOBAL_EXIT = 1;
  return;

}

static void print_userList(gpointer key, gpointer elem, gpointer data){

  char* username = (char*)key;

  if(strcmp(username, USERNAME)==0){
    return;
  }

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

  fprintf(stdout, "[PRINT_USERLIST]###############################################\n");
  fflush(stdout);
  return;
}

void _initSignals(){

  int ret;

  //setting signal handler and signal mask
  sigset_t mask1;
  ret = sigfillset(&mask1);
  ERROR_HELPER(ret, "[MAIN] Error in sigfillset function");

  struct sigaction sigint_act;
  sigint_act.sa_mask    = mask1;
  sigint_act.sa_flags   = 0;
  sigint_act.sa_handler = sigHandler;

  ret = sigaction(SIGINT, &sigint_act, NULL);
  ERROR_HELPER(ret, "[MAIN] Error in sigaction function");

  sigset_t mask2;
  ret = sigfillset(&mask2);
  ERROR_HELPER(ret, "[MAIN] Error in sigfillset function");

  struct sigaction sigpipe_act;
  sigpipe_act.sa_mask = mask2;
  sigpipe_act.sa_flags = 0;
  sigpipe_act.sa_handler = sigHandler;

  ret = sigaction(SIGPIPE, &sigpipe_act, NULL);
  ERROR_HELPER(ret, "[MAIN] Error in sigaction function");

  sigset_t mask3;
  ret = sigfillset(&mask3);
  ERROR_HELPER(ret, "[MAIN] Error in sigfillset function");

  struct sigaction sigterm_act;
  sigpipe_act.sa_mask = mask3;
  sigpipe_act.sa_flags = 0;
  sigpipe_act.sa_handler = sigHandler;

  ret = sigaction(SIGTERM, &sigpipe_act, NULL);
  ERROR_HELPER(ret, "[MAIN] Error in sigaction function");

  return;

}

void list_command(){

  int ret;

  ret = sem_wait(&sync_userList);
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in wait function on sync_userList semaphore\n");

  FOR_EACH(user_list, (GHFunc)print_userList, NULL); //printing hashtable

  ret = sem_post(&sync_userList);
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_userList semaphore\n");

}

void send_message(int socket){

  buf_commands[0] = MESSAGE; //per il parsing per i messaggi

  buf_commands[strlen(buf_commands)]   = '\n';
  buf_commands[strlen(buf_commands)+1] = '\0';
  send_msg(socket, buf_commands);

  //fprintf(stdout, "[MAIN] buf_commands = %s\n", buf_commands);

  if(strcmp(buf_commands,"xexit\n\n")==0){
    IS_CHATTING = 0;
    memset(buf_commands, 0, MSG_LEN);
    return;
  }

  memset(buf_commands+1, 0, MSG_LEN-1);
  return;
}

void connect_to(int socket, char* target_client){

  int ret;

  WAITING_RESPONSE = 1;

  fprintf(stdout, "[MAIN] Connect to: ");

  target_client[0] = CONNECTION_REQUEST;

  fgets(target_client+1, MSG_LEN-1, stdin);  //prende lo username

  //sending chat username to server
  send_msg(socket, target_client);

  memset(buf_commands,  0, MSG_LEN);
  memset(target_client, 0, MSG_LEN);

  //waiting for usrl_liste_thread_routine to bind address to socket and to listen
  ret = sem_wait(&wait_response);
  ERROR_HELPER(ret, "[MAIN] Error in sem_wait wait_response");

  WAITING_RESPONSE = 0;

  return;
}

void responde(int socket){

  if(((buf_commands[1]=='y' || buf_commands[1]=='n') && strlen(buf_commands)==3)){

    if(buf_commands[1] == 'y'){
      IS_CHATTING = 1;
    }
    else{
      IS_CHATTING = 0;
    }

    strncpy(buf_commands+2, USERNAME_CHAT, strlen(USERNAME_CHAT)); //USERNAME_CHAT o USERNAME...CONTROLLARE!!
    buf_commands[strlen(buf_commands)] = '\n';
    buf_commands[strlen(buf_commands)+1] = '\0';

    send_msg(socket, buf_commands);

    memset(buf_commands, 0, MSG_LEN);
    return;
  }

  else{
    fprintf(stdout, "[MAIN] Wrong input for connetion response\n");
    memset(buf_commands+1, 0, MSG_LEN);
    return;
  }

}

int get_username(char* username, int socket){

  //changed USERNAME_BUF_SIZE + 1 because fgets puts in buffer the \n character
  //if there are problems take away +1

  int i, ret;

  fprintf(stdout, "[GET_USERNAME] Enter username(max 16 char): ");

  username = fgets(username, USERNAME_BUF_SIZE, stdin);

  if(GLOBAL_EXIT){
    return 1;
  }

  //checking if username has atleast 1 character
  if(strlen(username)-1 == 0){ //because there is a \n got from fgets
    fprintf(stdout, "[GET_USERNAME] No username input\n");
    fflush(stdout);
    return 0;
  }

  //checking if username contains '-' character
  for(i=0; i < strlen(username); i++){
    if(username[i]=='-'){
      fprintf(stdout, "[GET_USERNAME] Char '-' found in username ... input correct username\n");
      fflush(stdout);
      return 0; //contains '-' character, username not ok return 0
    }
  }

  //sending buffer init data for user list
  //creating buffer for username and availability flag
  char* buf = (char*)calloc(USERNAME_BUF_SIZE+1, sizeof(char));
  strncpy(buf, username, strlen(username));
  buf[strlen(buf)]   = '\n';
  buf[strlen(buf)+1] = '\0'; //invalide write of size 1 ... dont know why

  //sending username to server
  send_msg(socket, buf);

  fprintf(stdout, "[GET_USERNAME] sent username [%s] to server\n", buf);

  //checking if username already in use
  memset(buf, 0, USERNAME_BUF_SIZE+1);

  ret = recv_msg(socket, buf, 2);
  ERROR_HELPER(ret, "[GET_USERNAME] Error receiving username check from server"); //forse bisogna aggiungere un timeout per questa receive

  fprintf(stdout, "[GET_USERNAME] Username available? [%c]\n", buf[0]);

  if(buf[0] == UNAVAILABLE){
    free(buf);
    fflush(stdout);
    return 0; //username not ok
  }

  free(buf);
  fflush(stdout);
  username[strlen(username)-1] = '\0';
  return 1; //usrname ok
}

void display_commands(){
  fprintf(stdout, CMD_STRING);
  return;
}

//mettere uno switch e' piu' mejo!
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

void init_semaphores(){

  int ret;

  fprintf(stdout, "[MAIN] initializing semaphores...\n");

  //creating sempahore for listen function in usrl_liste_thread_routine for bind action
  ret = sem_init(&sync_receiver, 0, 0);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_receiver semaphore");

  //creating semaphore for connect_to function
  ret = sem_init(&wait_response, 0, 0);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &wait_response semaphore");

  //creating sempahore for mutual exlusion in userList
  ret = sem_init(&sync_userList, 0, 1);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_userList semaphore");

  fprintf(stdout, "[MAIN] semaphores initialized correctly\n");

  return;
}

void* read_updates(void* args){

  int ret;

  fprintf(stdout, "[READ_UPDATES] inside function read_updates\n");

  read_updates_args_t* arg = (read_updates_args_t*)args;

  GAsyncQueue* buf = REF(arg->read_updates_mailbox);
  //int socket_to_server = (int)(arg->server_socket); controllare se serve

  while(1){

    char* elem_buf;
    //fprintf(stdout, "[READ_UPDATES] inside while(1)\n");

    while(1){

      if(GLOBAL_EXIT){
        break;
      }

      elem_buf = (char*)POP(buf, POP_TIMEOUT);

      if(elem_buf != NULL){
        break;
      }
    }

    if(GLOBAL_EXIT){
      break;
    }

    fprintf(stdout, "[READ_UPDATES] Elem buff[0]: %c\n", elem_buf[0]);

    //fprintf(stdout, "[READ_UPDATES] Connection request");

    if(elem_buf[0]==MESSAGE){

      if(!strcmp(elem_buf+1,"exit")){
        IS_CHATTING = 0;
        fprintf(stdout, "[Receiving chat] exit message received press ENTER to exit chat\n");
        memset(USERNAME_CHAT, 0, USERNAME_BUF_SIZE);
        continue;
      }

      fprintf(stdout, "[Receiving chat] %s\n", elem_buf+1);
      free(elem_buf);
      continue;
    }


    else if(elem_buf[0] == CONNECTION_REQUEST){

      fprintf(stdout, "[READ_UPDATES] Connection request from [%s] accept [y] refuse [n]: \n", elem_buf+1);

      buf_commands[0] = CONNECTION_RESPONSE;

      strncpy(USERNAME_CHAT, elem_buf+1, strlen(elem_buf)-1);

      free(elem_buf);

      continue;

    }

    else if(elem_buf[0] == CONNECTION_RESPONSE){

      if(elem_buf[1] == 'y'){

        strncpy(USERNAME_CHAT, elem_buf+2, strlen(elem_buf)-2);

        fprintf(stdout, "[READ_UPDATES] Response from server is YES! You are now chatting with: %s\n", USERNAME_CHAT);

        IS_CHATTING = 1;
        buf_commands[0] = MESSAGE;

      }

      else{
        fprintf(stdout, "[READ_UPDATES] Response from server is NO! :(\n");
        IS_CHATTING = 0;
      }

      //ublocking &wait_response
      ret = sem_post(&wait_response);
      ERROR_HELPER(ret, "[READ_UPDATES] Error in sem_post on &wait_response semaphore");

      free(elem_buf);
      continue;

    }


    //tutto questo va fatto solo se NON e' un messaggio un connection response o request

    fprintf(stdout, "[READ_UPDATES] poped element from queue\n");

    fprintf(stdout, "[READ_UPDATES] %s\n", elem_buf);

    //creating struct usr_list_elem_t* for create_elem_list function
    usr_list_elem_t* elem = (usr_list_elem_t*)calloc(1, sizeof(usr_list_elem_t));
    elem->client_ip = (char*)calloc(INET_ADDRSTRLEN,sizeof(char));

    //creating buffer for username and command to pass to create_elem_list function
    char* userName = (char*)calloc(USERNAME_BUF_SIZE,sizeof(char));
    char* command = (char*)calloc(2, sizeof(char)); //ricordare che hai cambiato prima era una calloc

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

  UNREF(arg->read_updates_mailbox);
  UNREF(buf);

  fprintf(stdout, "[READ_UPDATES] exiting\n");

  pthread_exit(NULL);

}

void* recv_updates(void* args){

  int ret;

  struct timeval timeout;
  timeout.tv_sec  = 2;
  timeout.tv_usec = 0;

  //socket descriptor for user list receiver thread
  int usrl_recv_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(usrl_recv_socket, "[RECV_UPDATES] Error while creating user list receiver thread socket descriptor");

  //creating buffers to store modifications sent by server
  GAsyncQueue* mail_box = mailbox_queue_init();

  GAsyncQueue* buf_modifications = REF(mail_box);
  ((read_updates_args_t*) args)->read_updates_mailbox = mail_box;

  //creating thread to manage updates to user list
  pthread_t manage_updates;
  ret = pthread_create(&manage_updates, NULL, read_updates, (void*)args);
  PTHREAD_ERROR_HELPER(ret, "[RECV_UPDATES] Unable to create manage_updates thread");

  //address structure for user list sender thread
  struct sockaddr_in *usrl_sender_address = (struct sockaddr_in*)calloc(1, sizeof(struct sockaddr_in));
  socklen_t usrl_sender_address_len = sizeof(usrl_sender_address);

  //struct for thead user list receiver thread bind function
  struct sockaddr_in thread_addr = {0};
  thread_addr.sin_family      = AF_INET;
  thread_addr.sin_port        = htons(CLIENT_THREAD_RECEIVER_PORT); // don't forget about network byte order!
  thread_addr.sin_addr.s_addr = INADDR_ANY;

  fprintf(stderr, "[RECV_UPDATES] created sockaddr_in struct for bind function\n");

  //we enable SO_REUSEADDR to quickly restart our server after a crash
  int reuseaddr_opt = 1;
  ret = setsockopt(usrl_recv_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot set SO_REUSEADDR option");

  //binding user list receiver thread address to user list receiver thread socket
  ret = bind(usrl_recv_socket, (const struct sockaddr*)&thread_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "[RECV_UPDATES] Error while binding address to user list receiver thread socket");

  fprintf(stderr, "[RECV_UPDATES] address binded to socket\n");

  //user list receiver thread listening for incoming connections
  ret = listen(usrl_recv_socket, 1);
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot listen on user list receiver thread socket");

  fprintf(stderr, "[RECV_UPDATES] listening on socket for server connection\n");

  //ublocking &sync_receiver for main process
  ret = sem_post(&sync_receiver);
  ERROR_HELPER(ret, "[RECV_UPDATES] Error in sem_post on &sync_receiver semaphore (user list receiver thread routine)");

  //accepting connection from server on user list receiver thread socket
  int rec_socket = accept(usrl_recv_socket, (struct sockaddr*) usrl_sender_address, &usrl_sender_address_len);
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot accept connection on user list receiver thread socket");

  //we enable SO_RCVTIMEO so that recv function will not be blocking
  ret = setsockopt(rec_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot set SO_RCVTIMEO option");

  fprintf(stderr, "[RECV_UPDATES] accepted connection from server\n");

  //buffer for recv_msg function
  char* buf = (char*)calloc(MSG_LEN, sizeof(char));

  int count = 0;

  //receiving user list element from server
  while(1){

      if(GLOBAL_EXIT){
        break;
      }

      memset(buf, 0, MSG_LEN);

      //receiveing user list element from server
      ret = recv_msg(rec_socket, buf, MSG_LEN);

      if(ret==-2){
        continue;
      }

      //server morto
      else if(ret==-1){
        GLOBAL_EXIT = 1;
        break;
      }

      if(!GLOBAL_EXIT){

        size_t len = strlen(buf);
        char* queueBuf_elem = (char*)calloc(len+1, sizeof(char));

        strncpy(queueBuf_elem, buf, strlen(buf)); //for \0 in the char*

        PUSH(buf_modifications, (gpointer)queueBuf_elem);
      }

  } //end of while(1)

  //this part of code will only be executed if server or main quits

  fprintf(stdout, "[RECV_UPDATES] joining read_updates\n");

  ret = pthread_join(manage_updates, NULL);

  free(buf);
  free(usrl_sender_address);
  UNREF(buf_modifications);

  fprintf(stderr, "[RECV_UPDATES] closing rec_socket and arg->socket...\n");

  ret = close(rec_socket);
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot close socket");

  ret = close(usrl_recv_socket);
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot close usrl_recv_socket");

  fprintf(stderr, "[RECV_UPDATES] closed rec_socket and arg->socket succesfully\n");

  fprintf(stderr, "[RECV_UPDATES] exiting recv_updates\n");

  fprintf(stderr, "[RECV_UPDATES] Press any key to exit\n"); //potrei mettere una variabile globale per il connect_to e far stampare solo se = 1

  if(WAITING_RESPONSE){

    //ublocking &wait_response
    ret = sem_post(&wait_response);
    ERROR_HELPER(ret, "[RECV_UPDATES] Error in sem_post on &wait_response semaphore");
  }

  pthread_exit(NULL);

} //end of thread routine

int main(int argc, char* argv[]){

  int ret;

  _initSignals();

  IS_CHATTING      = 0; // non sono connesso a nessuno per adesso
  GLOBAL_EXIT      = 0;
  WAITING_RESPONSE = 0; // non aspetto nessuna risposta da server
  USERNAME         = (char*)calloc(USERNAME_BUF_SIZE+1, sizeof(char));
  USERNAME_CHAT    = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

  //system("say welcome to talk service!");

  fprintf(stdout, "[MAIN] starting main process\n");

  //initializing GLibHashTable for user list
  user_list = usr_list_init();

  //alocating buffers for disconnect
  char* disconnect  = (char*)calloc(3, sizeof(char));

  //copying disconnect command into buffers
  strcpy(disconnect,  "c\n");

  //initializing semaphores
  init_semaphores();

  //socket descriptor to connect to server
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "[MAIN] Error while creating client socket descriptor");

  //data structure for the connection to the server
  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family         = AF_INET;
  serv_addr.sin_port           = htons(SERVER_PORT);
  serv_addr.sin_addr.s_addr    = inet_addr(SERVER_IP);

  fprintf(stdout, "[MAIN] created data structure for connection with server\n");

  //creating and spawning user list receiver thread with parameters
  read_updates_args_t* thread_usrl_recv_args = (read_updates_args_t*)malloc(sizeof(read_updates_args_t));

  pthread_t thread_usrl_recv;
  ret = pthread_create(&thread_usrl_recv, NULL, recv_updates, (void*)thread_usrl_recv_args);
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

  //getting username from user   add max number of attempts
  while(1){
    ret = get_username(USERNAME, socket_desc);
    if(ret==1){
      break;
    }
    memset(USERNAME, 0, USERNAME_BUF_SIZE); //setting buffer to 0
  }


  fprintf(stdout, "[MAIN] got username: [%s]\n", USERNAME);

  buf_commands = (char*)calloc(MSG_LEN, sizeof(char));
  //buf_commands[0] = '-'; //orribile vedi se lo puoi cancellare

  char* user_buf = (char*)calloc(MSG_LEN, sizeof(char));

  //display commands for first time
  if(!GLOBAL_EXIT) display_commands();

  while(1){

    if(GLOBAL_EXIT){
      fprintf(stdout, "[MAIN] Ready to exit process...\n");
      break;
    }

    else if(IS_CHATTING){
      fprintf(stdout, "[MAIN]>>: ");
    }
    else{
      fprintf(stdout, "[MAIN] IMPUT COMMAND (help to display commands): ");
    }

    fgets(buf_commands+1, MSG_LEN-3, stdin);

    if(IS_CHATTING){ //per inviare messaggi in chat //messaggio contenuto in buf_commands
      send_message(socket_desc);
      continue;
    }

    else if(buf_commands[0] == CONNECTION_RESPONSE){
      responde(socket_desc);
      continue;
    }

    else if(strcmp(buf_commands+1, LIST)==0 && !GLOBAL_EXIT){  //per la lista
      list_command();
      memset(buf_commands, 0, MSG_LEN);
      continue;
    }

    else if(strcmp(buf_commands+1, CONNECT)==0 && !GLOBAL_EXIT){ //per connettersi al client
      connect_to(socket_desc, user_buf);
      continue;
    }

    else if(strcmp(buf_commands+1, HELP)==0 && !GLOBAL_EXIT){
      display_commands(); //imbarazzante una funzione per stampare una stringa cazzo
      memset(buf_commands,  0, MSG_LEN);
      continue;
    }

    else if(strcmp(buf_commands+1, EXIT)==0 && !GLOBAL_EXIT){ //per uscire dal programma
      GLOBAL_EXIT = 1;
      break;
    }

    else if(strcmp(buf_commands+1, CLEAR)==0 && !GLOBAL_EXIT){
      memset(buf_commands,  0, MSG_LEN);
      system("clear");
      continue;
    }

    else{
      if(GLOBAL_EXIT){
        break;
      }
      fprintf(stdout, "\n>>command not found\n");
      display_commands();
      memset(buf_commands,  0, MSG_LEN);
    }

  }

  //sending disconnect command to server
  send_msg(socket_desc, disconnect);

  ret = pthread_join(thread_usrl_recv, NULL);

  fprintf(stdout, "[MAIN] destroying semaphores....\n");

  ret = sem_destroy(&sync_userList);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_userList semaphore");

  ret = sem_destroy(&wait_response);
  ERROR_HELPER(ret, "[MAIN] Error destroying &wait_response semaphore");

  fprintf(stdout, "[MAIN] freeing allocated data...\n");

  free(USERNAME);
  free(USERNAME_CHAT);
  free(buf_commands);
  free(user_buf);
  free(thread_usrl_recv_args);
  //free(available);
  //free(unavailable);
  free(disconnect);

  fprintf(stdout, "[MAIN] Destroying user list...\n");
  DESTROY(user_list);


  fprintf(stdout, "[MAIN] closing socket descriptors...\n");

  // close client main process socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "[MAIN] Cannot close socket_desc");

  fprintf(stdout, "[MAIN] exiting main process\n\n");


  exit(EXIT_SUCCESS);

}
