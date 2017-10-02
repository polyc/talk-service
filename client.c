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
#include <sys/prctl.h>

#include "client.h"
#include "common.h"
#include "util.h"

sem_t sync_receiver;
sem_t sync_userList;
GHashTable* user_list;

char* USERNAME;
char* USERNAME_CHAT;
char* buf_commands;
char* available;
char* unavailable;
char* disconnect;
int   CONNECTED; // 0 se non connesso 1 se connesso
int   MAIN_EXIT;


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
    CONNECTED = 0;
    memset(buf_commands, 0, MSG_LEN);
    return;
  }

  memset(buf_commands+1, 0, MSG_LEN-1);
  return;
}

void connect_to(int socket, char* target_client){

  fprintf(stdout, "[MAIN] Connect to: ");

  target_client[0] = CONNECTION_REQUEST;

  fgets(target_client+1, MSG_LEN-1, stdin);  //prende lo username

  //sending chat username to server
  send_msg(socket, target_client);

  memset(buf_commands,  0, MSG_LEN);
  memset(target_client, 0, MSG_LEN);

  return;
}

void responde(int socket){

  if(((buf_commands[1]=='y' || buf_commands[1]=='n') && strlen(buf_commands)==3)){

    if(buf_commands[1] == 'y'){
      CONNECTED = 1;
    }
    else{
      CONNECTED = 0;
    }

    strcpy(buf_commands+2, USERNAME_CHAT); //USERNAME_CHAT o USERNAME...CONTROLLARE!!
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
  char* buf = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
  strncpy(buf, username, strlen(username));
  buf[strlen(buf)] = '\n';

  //sending username to server
  send_msg(socket, buf);

  fprintf(stdout, "[GET_USERNAME] sent username [%s] to server\n", buf);

  //checking if username already in use
  memset(buf, 0, USERNAME_BUF_SIZE);

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

void* read_updates(void* args){

  int ret;

  ret = prctl(PR_SET_PDEATHSIG, SIGINT);
  ERROR_HELPER(ret, "[READ_UPDATES] error on prctl function");

  fprintf(stdout, "[READ_UPDATES] inside function read_updates\n");

  read_updates_args_t* arg = (read_updates_args_t*)args;

  GAsyncQueue* buf = REF(arg->read_updates_mailbox);
  //int socket_to_server = (int)(arg->server_socket); controllare se serve

  while(1){

    char* elem_buf;
    //fprintf(stdout, "[READ_UPDATES] inside while(1)\n");

    while(1){

      elem_buf = (char*)POP(buf, POP_TIMEOUT);

      if(elem_buf != NULL){
        break;
      }
    }

    fprintf(stdout, "[READ_UPDATES] Elem buff[0]: %c\n", elem_buf[0]);

    //fprintf(stdout, "[READ_UPDATES] Connection request");

    if(elem_buf[0]==MESSAGE){

      if(!strcmp(elem_buf+1,"exit")){
        CONNECTED = 0;
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

      strcpy(USERNAME_CHAT, elem_buf+1);

      free(elem_buf);

      continue;

    }

    else if(elem_buf[0] == CONNECTION_RESPONSE){

      if(elem_buf[1] == 'y'){

        strcpy(USERNAME_CHAT, elem_buf+2);

        fprintf(stdout, "[READ_UPDATES] Response from server is YES! You are now chatting with: %s\n", USERNAME_CHAT);

        CONNECTED = 1;
        buf_commands[0] = MESSAGE;

      }

      else{
        fprintf(stdout, "[READ_UPDATES] Response from server is NO! :(\n");
        CONNECTED = 0;
      }

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
}

void* usr_list_recv_thread_routine(void* args){

  int ret;

  ret = prctl(PR_SET_PDEATHSIG, SIGINT);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] error on prctl function");

  //socket descriptor for user list receiver thread
  int usrl_recv_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(usrl_recv_socket, "[RECV_THREAD_ROUTINE] Error while creating user list receiver thread socket descriptor");

  //creating buffers to store modifications sent by server
  GAsyncQueue* mail_box = mailbox_queue_init();

  GAsyncQueue* buf_modifications = REF(mail_box);
  ((read_updates_args_t*) args)->read_updates_mailbox = mail_box;

  //creating thread to manage updates to user list
  pthread_t manage_updates;
  ret = pthread_create(&manage_updates, NULL, read_updates, (void*)args);
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

  //user list receiver thread listening for incoming connections
  ret = listen(usrl_recv_socket, 1);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot listen on user list receiver thread socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] listening on socket for server connection\n");

  //ublocking &sync_receiver for main process
  ret = sem_post(&sync_receiver);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Error in sem_post on &sync_receiver semaphore (user list receiver thread routine)");

  //accepting connection from server on user list receiver thread socket
  int rec_socket = accept(usrl_recv_socket, (struct sockaddr*) &usrl_sender_address, &usrl_sender_address_len);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot accept connection on user list receiver thread socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] accepted connection from server\n");

  //buffer for recv_msg function
  char* buf = (char*)calloc(MSG_LEN, sizeof(char));

  //receiving user list element from server
  while(1){

      if(MAIN_EXIT){
        break;
      }

      memset(buf, 0, MSG_LEN);

      //receiveing user list element from server
      ret = recv_msg(rec_socket, buf, MSG_LEN);

      if(ret==-1){
        break;
      }

      //fprintf(stdout, "[RECV_THREAD_ROUTINE] MESSAGES: %s\n", buf);

      size_t len = strlen(buf);
      char* queueBuf_elem = (char*)calloc(len+1, sizeof(char));

      strcpy(queueBuf_elem, buf); //for \0 in the char*

      g_async_queue_push(buf_modifications, (gpointer)queueBuf_elem);

  } //end of while(1)

  //this part of code will only be executed if server quits

  fprintf(stderr, "[RECV_THREAD_ROUTINE] closing rec_socket and arg->socket...\n");

  ret = close(rec_socket);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot close socket");

  ret = close(usrl_recv_socket);
  ERROR_HELPER(ret, "[RECV_THREAD_ROUTINE] Cannot close usrl_recv_socket");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] closed rec_socket and arg->socket succesfully\n");

  free(buf);
  free(usrl_sender_address);
  UNREF(buf_modifications);

  fprintf(stderr, "[RECV_THREAD_ROUTINE] exiting usr_list_recv_thread_routine\n");

  fprintf(stderr, "[RECV_THREAD_ROUTINE] Server closed end-point exit program with CTRL-C\n");

  pthread_exit(NULL);

} //end of thread routine

int main(int argc, char* argv[]){

  int ret;
  CONNECTED = 0; // non sono connesso a nessuno per adesso
  MAIN_EXIT = 0;
  USERNAME_CHAT = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

  fprintf(stdout, "[MAIN] starting main process\n");

  //initializing GLibHashTable for user list
  user_list = usr_list_init();

  //alocating buffers for availability
  // ****non servono piu i buffer tanto ci pensa il server a vedere se sono collegato o meno****
  available   = (char*)calloc(3, sizeof(char));
  unavailable = (char*)calloc(3, sizeof(char));
  disconnect  = (char*)calloc(3, sizeof(char));

  //copying availability commands into buffers
  strcpy(available,   "a\n");
  strcpy(unavailable, "u\n");
  strcpy(disconnect,  "c\n");

  fprintf(stdout, "[MAIN][BUFF_TEST] %c, %c, %c", available[0], unavailable[0], disconnect[0]);

  fprintf(stdout, "[MAIN] initializing semaphores...\n");

  //creating sempahore for listen function in usrl_liste_thread_routine for bind action
  ret = sem_init(&sync_receiver, 0, 0);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_receiver semaphore");

  //creating sempahore for mutual exlusion in userList
  ret = sem_init(&sync_userList, 0, 1);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_userList semaphore");

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


  //user list receiver thread
  //
  //creating and spawning user list receiver thread with parameters
  read_updates_args_t* thread_usrl_recv_args = (read_updates_args_t*)malloc(sizeof(read_updates_args_t));
  //thread_usrl_recv_args->server_socket = socket_desc; non dovrebbe servire

  pthread_t thread_usrl_recv;
  ret = pthread_create(&thread_usrl_recv, NULL, usr_list_recv_thread_routine, (void*)thread_usrl_recv_args);
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
  USERNAME = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

  //getting username from user   add max number of attempts
  while(1){
    ret = get_username(USERNAME, socket_desc);
    if(ret==1){
      break;
    }
    memset(USERNAME, 0, USERNAME_BUF_SIZE); //setting buffer to 0
    //strcpy(USERNAME, "");
  }

  USERNAME = strtok(USERNAME, "\n");

  fprintf(stdout, "[MAIN] got username: [%s]\n", USERNAME);

  buf_commands = (char*)calloc(MSG_LEN, sizeof(char));
  buf_commands[0] = '-'; //orribile vedi se lo puoi cancellare

  char* user_buf = (char*)calloc(MSG_LEN, sizeof(char));

  while(1){

    if(CONNECTED){
      fprintf(stdout, "[MAIN] Enter message: ");
    }
    else{
      fprintf(stdout, "[MAIN] IMPUT COMMAND (exit/list/connect): ");
    }

    fgets(buf_commands+1, MSG_LEN-3, stdin);

    if(CONNECTED){ //per inviare messaggi in chat //messaggio contenuto in buf_commands
      send_message(socket_desc);
      continue;
    }

    else if(buf_commands[0] == CONNECTION_RESPONSE){
      responde(socket_desc);
      continue;
    }

    else if(strcmp(buf_commands+1, "list\n")==0){  //per la lista
      list_command();
      memset(buf_commands, 0, MSG_LEN);
      continue;
    }

    else if(strcmp(buf_commands+1, "connect\n")==0){ //per connettersi al client
      connect_to(socket_desc, user_buf);
      continue;
    }

    else if(strcmp(buf_commands+1, "exit\n")==0){ //per uscire dal programma
      MAIN_EXIT = 1;
      break;
    }

  }

  //sending disconnect command to server
  send_msg(socket_desc, disconnect);

  ret = pthread_join(thread_usrl_recv, NULL);

  fprintf(stdout, "[MAIN] destroying semaphores....\n");

  ret = sem_destroy(&sync_userList);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_userList semaphore");

  fprintf(stdout, "[MAIN] freeing allocated data...\n");

  free(USERNAME);
  free(USERNAME_CHAT);
  free(buf_commands);
  free(available);
  free(unavailable);
  free(disconnect);
  free(MAIN_EXIT);

  fprintf(stdout, "[MAIN] closing socket descriptors...\n");

  // close client main process socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "[MAIN] Cannot close socket_desc");

  fprintf(stdout, "[MAIN] exiting main process\n\n");


  exit(EXIT_SUCCESS);

}
