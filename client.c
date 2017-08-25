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
sem_t sync_chat;
GHashTable* user_list;

char* USERNAME;
char* USERNAME_CHAT;
char* available;
char* unavailable;
char* disconnect;
int CONNECTED; // 0 se non connesso 1 se connesso


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

int get_username(char* username, int socket){

  //changed USERNAME_BUF_SIZE + 1 because fgets puts in buffer the \n character
  //if there are problems take away +1

  int i, ret;

  fprintf(stdout, "[GET_USERNAME] Enter username: ");

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
  int socket_to_server = (int)(arg->server_socket);

  while(1){

    char* elem_buf;
    fprintf(stdout, "[READ_UPDATES] inside while(1)\n");

    while(1){

      elem_buf = (char*)POP(buf, POP_TIMEOUT);

      if(elem_buf != NULL){
        break;
      }
    }

    fprintf(stdout, "[READ_UPDATES] Elem buff: %s\n", elem_buf);

    if(elem_buf[0]==MESSAGE){

      if(!strcmp(elem_buf+1,"exit")){
        CONNECTED = 0;
        memset(USERNAME_CHAT, 0, USERNAME_BUF_SIZE);
        continue;
      }

      fprintf(stdout, "[Receiving chat] %s\n", elem_buf+1);
      free(elem_buf);
      continue;
    }

    else if(elem_buf[0] == CONNECTION_REQUEST){
      for(int i=1; i<USERNAME_BUF_SIZE && elem_buf[i] != '\0'; i++){
        USERNAME_CHAT[i] = elem_buf[i];
      }

      fprintf(stdout, "[READ_UPDATES] Connection request from [%s] accept [y] refuse [n]: ", USERNAME_CHAT);

      while(1){
        fgets(elem_buf+1, 2, stdin);
        elem_buf[0] = CONNECTION_RESPONSE;

        if(elem_buf[1] == 'y' || elem_buf[1] == 'n'){
          memset(elem_buf+2, 0, MSG_LEN);
          break;
        }
        memset(elem_buf+1, 0, MSG_LEN);

      }
      if(elem_buf[1] == 'y'){
        CONNECTED = 1;
      }
      elem_buf[strlen(elem_buf)] = '\0';

      send_msg(socket_to_server, elem_buf);

      continue;

    }


    //tutto questo va fatto solo se NON e' un messaggio

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
  char* buf = (char*)calloc(MSG_LEN, sizeof(char));

  //receiving user list element from server
  while(1){
      memset(buf, 0, MSG_LEN);

      //receiveing user list element from server
      ret = recv_msg(rec_socket, buf, MSG_LEN);

      if(ret==-1){
        break;
      }

      fprintf(stdout, "[RECV_THREAD_ROUTINE] MESSAGES: %s\n", buf);

      size_t len = strlen(buf);
      char* queueBuf_elem = (char*)calloc(len, sizeof(char));

      memcpy(queueBuf_elem, buf, len);

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

  pthread_exit(NULL);

} //end of thread routine

int main(int argc, char* argv[]){

  int ret;
  CONNECTED = 0; // non sono connesso a nessuno per adesso
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

  //creating sempahore for syncronization between connect_routine and listen_routine
  // ****non serve piu perche ci sara una sola routine****
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


  //user list receiver thread
  //
  //creating and spawning user list receiver thread with parameters
  read_updates_args_t* thread_usrl_recv_args = (read_updates_args_t*)malloc(sizeof(read_updates_args_t));
  thread_usrl_recv_args->server_socket = socket_desc;

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

  char* buf_commands = (char*)calloc(MSG_LEN, sizeof(char));

  while(1){

    //creating sempahore for syncronization between connect_routine and listen_routine
    ret = sem_init(&sync_chat, 0, 0);
    ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_chat");

    memset(buf_commands, 0, MSG_LEN);

    fprintf(stdout, "[MAIN] exit/connect to/list: ");

    fgets(buf_commands+1, MSG_LEN-1, stdin);
    buf_commands = strtok(buf_commands+1, "\n");

    fprintf(stdout, "[MAIN] buf_commands = %s\n", buf_commands);


    if(CONNECTED){ //per inviare messaggi in chat
      buf_commands[0] = MESSAGE; //per il parsing per i messaggi
      send_msg(socket_desc, buf_commands); //aggiungere \n al buffer

      continue;
    }

    if(list_command(buf_commands+1)==1){  //per la lista
      continue;
    }

    else if(strcmp(buf_commands, "connect")==0){ //per chattare

      fprintf(stdout, "[MAIN] passato il connect\n");

      memset(buf_commands, 0, MSG_LEN);

      char* user_buf = (char*)calloc(MSG_LEN, sizeof(char));

      user_buf[0] = CONNECTION_REQUEST;

      fgets(user_buf+1, MSG_LEN-1, stdin);  //prende lo username

      //sending chat username to server
      send_msg(socket_desc, user_buf);

      memset(buf_commands, 0, MSG_LEN);
      memset(user_buf, 0, MSG_LEN);

      ret = recv_msg(socket_desc, buf_commands,2);

      fprintf(stdout, "[MAIN]risposta del server: [%c]\n", buf_commands[0]);

      if(buf_commands[0] == CONNECTION_RESPONSE && buf_commands[1]=='y'){

        fprintf(stdout, "[MAIN] username accepted from server\n");
        CONNECTED = 1;

        user_buf[0] = MESSAGE;

        while(CONNECTED){

          memset(user_buf+1, 0, MSG_LEN-1);

          fprintf(stdout, "[CHAT]: ");
          fgets(user_buf+1, MSG_LEN-1, stdin);

          fprintf(stdout, "[CHAT_FLAG]: %s", user_buf);

          send_msg(socket_desc, user_buf);

          if(!strcmp(user_buf+1, "exit\n")){
            fprintf(stdout, "[CHAT] exit message arrived....exiting chat\n");
            CONNECTED = 0;
            break;
          }

        }

        /*
        *
        *
        *  aspettare e fare qualcosa con la connessione
        *  connected = 1 RICORDA!!
        *
        *
        */

      }

      else{
        fprintf(stdout, "[MAIN] Incorrect username, server may be down or connection refused\n");
      }

      continue; //nonn deve essere continue ma deve fare qualcosa per la chat
    }

    else if(strcmp(buf_commands, "exit")==0){ //per uscire dal programma
      break;
    }

  }

  //sending disconnect command to server
  send_msg(socket_desc, disconnect);

  fprintf(stdout, "[MAIN] destroying semaphores....\n");

  ret = sem_destroy(&sync_userList);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_userList semaphore");

  ret = sem_destroy(&sync_chat);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_chat semaphore");

  fprintf(stdout, "[MAIN] freeing allocated data...\n");

  free(USERNAME);
  free(buf_commands);
  free(available);
  free(unavailable);
  free(disconnect);

  fprintf(stdout, "[MAIN] closing socket descriptors...\n");

  // close client main process socket
  ret = close(socket_desc);
  ERROR_HELPER(ret, "[MAIN] Cannot close socket_desc");

  fprintf(stdout, "[MAIN] exiting main process\n\n");

  exit(EXIT_SUCCESS);

}
