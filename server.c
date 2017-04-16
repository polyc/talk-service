#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <glib.h>
#include <fcntl.h>
#include <semaphore.h>

#include "common.h"
#include "server.h"
#include "util.h"

sem_t user_list_mutex; // mutual exclusion to acces user list hash-table
sem_t mailbox_list_mutex;
sem_t thread_count_mutex;
GHashTable* user_list;
GHashTable* mailbox_list;
int thread_count;

void get_and_check_username(int socket, char* username){
  char* send_buf = (char*)calloc(2, sizeof(char));
  while(1){
    int ret = recv_msg(socket, username, USERNAME_BUF_SIZE);
    ERROR_HELPER(ret, "[MAIN]: cannot receive username");

    ret = sem_wait(&user_list_mutex);
    ERROR_HELPER(ret, "[MAIN]:cannot wait on user_list_mutex");

    if(CONTAINS(user_list, username) == FALSE){
      ret = sem_post(&user_list_mutex);
      ERROR_HELPER(ret, "[MAIN]:cannot post on user_list_mutex");

      send_buf[0] = AVAILABLE;
      send_buf[1] = '\n';
      send_msg(socket, send_buf);
      break;
    }
    else{
      ret = sem_post(&user_list_mutex);
      ERROR_HELPER(ret, "[MAIN]:cannot post on user_list_mutex");

      send_buf[0] = UNAVAILABLE;
      send_msg(socket, send_buf);
      bzero(username, USERNAME_BUF_SIZE);
      bzero(send_buf, 2);
    }
  }
  fprintf(stdout, "[MAIN]: username getted\n");
  return;
}

void update_availability(usr_list_elem_t* elem_to_update, char* buf_command){
  //updating user_list
  fprintf(stdout, "SONO QUI\n" );
  int ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD][UPDATING AVAILABILITY]: cannot wait on user_list_mutex");

  elem_to_update->a_flag = *buf_command; //update availability flag
  fprintf(stdout, "AOOOOOOO\n" );
  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD][UPDATING AVAILABILITY]: cannot post on user_list_mutex");

  return;
}

void remove_entry(char* elem_to_remove, char* mailbox_to_remove){//to befinished
  gint ret = REMOVE(user_list, elem_to_remove); //remove entry
  if(ret == FALSE){
    ERROR_HELPER(-1, "[CONNECTION THREAD][REMOVING ENTRY]: remove entry failed");
  }

  ret = REMOVE(mailbox_list, mailbox_to_remove);
  if(ret == FALSE){
    ERROR_HELPER(-1, "[CONNECTION THREAD][REMOVING ENTRY]: remove hoihoh entry failed");
  }

  fprintf(stdout, "[CONNECTION THREAD] successfully removed entry on disconnect operation\n");
  return;
}

//pushing message into sender thread personal AsyncQueue
void push_entry(gpointer key, gpointer value, gpointer user_data/*parsed message*/){
  int ret = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");

  g_async_queue_push((GAsyncQueue*)value, user_data);

  ret = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");
  return;
}

//sender retrieve message from its queue
void pop_entry(){}

//function called by FOR_EACH. It send single user element to receiver thread in client
void send_list_on_client_connection(gpointer key, gpointer value, gpointer user_data){
  int ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDING LIST]: cannot wait on user_list_mutex");

  char* buf = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

  serialize_user_element(buf, value, key, NEW);
  fprintf(stdout, "[SENDING LIST]: serialized element: %s\n", buf);

  fprintf(stdout, "[SENDING LIST][USERNAME]: %s\n", (char*)key);
  fprintf(stdout,"[SENDING LIST][IP]: %s\n", (char*)((usr_list_elem_t*)value)->client_ip);
  fprintf(stdout, "[SENDING LIST][FLAG]: %c\n", (char)((usr_list_elem_t*)value)->a_flag);

  send_msg(*((int*)user_data), buf); //(socket, buf);


  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDING LIST]: cannot post on user_list_mutex");
  return;
}

void execute_command(thread_args_t* args, char* availability_buf, usr_list_elem_t* element_to_update){

  //selecting correct command
  char availability = availability_buf[0];
  char mod_command;

  fprintf(stdout, "[CONNECTION THREAD] availability: %c\n", availability);

  switch(availability){

    case UNAVAILABLE :
      mod_command = MODIFY;
      update_availability(element_to_update, &availability);
      //pushing updates to mailboxes
      FOR_EACH(mailbox_list, (GHFunc)push_entry, build_mailbox_message(args->client_user_name, &mod_command));
      fprintf(stdout, "[CONNECTION THREAD]: unavailable command processed\n");
      break;

    case AVAILABLE :
      mod_command = MODIFY;
      update_availability(element_to_update, &availability);
      //pushing updates to mailboxes
      FOR_EACH(mailbox_list, (GHFunc)push_entry, build_mailbox_message(args->client_user_name, &mod_command));
      fprintf(stdout, "[CONNECTION THREAD]: available command procesed\n");
      break;

    case DISCONNECT:
      mod_command = DELETE;
      remove_entry(args->client_user_name, args->mailbox_key);
      //pushing updates to mailboxes
      FOR_EACH(mailbox_list, (GHFunc)push_entry, build_mailbox_message(args->client_user_name, &mod_command));
      fprintf(stdout, "[CONNECTION THREAD]: disconnect command processed\n");
      //thread's close operations;
      break;//never executed beacuse in close operations, the thread exit safely
    default :
      //throw error
      return;
  }
  return;
}

mailbox_message_t* build_mailbox_message(char* username, char* mod_command) {
  mailbox_message_t* ret = (mailbox_message_t*)calloc(1, sizeof(mailbox_message_t));
  ret->mod_command = (char*)calloc(1, sizeof(char));
  *(ret->mod_command) = *mod_command;
  ret->client_user_name = username; //copying key address;
  return ret;
}

/*void extract_username_from_message(char* message, char* username){
  fprintf(stdout, "[SENDER THREAD]: extracting username %s\n", message);
  int i;
  for (i = 2; i < USERNAME_BUF_SIZE; i++) {
    if(message[i] == '-'){
      break;
    }
  }
  strncpy(username, message + 2, i-2);
  return;
}*/

//transform a usr_list_elem_t in a string according to mod_command
void serialize_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command){
  fprintf(stdout, "[SERIALIZE]: sono dentro la funzione di serializzazione\n");
  buf_out[0] = mod_command;
  strcat(buf_out, "-");
  strcat(buf_out, buf_username);
  fprintf(stdout, "[SERIALIZE]: maremma maiala\n");


  if(mod_command == DELETE){
    strcat(buf_out ,"-\n");
    return;
  }
  else if (mod_command == NEW){
    strcat(buf_out, "-");
    strcat(buf_out, elem->client_ip);
    strcat(buf_out, "-");
    strcat(buf_out, &(elem->a_flag));
    strcat(buf_out, "-\n");
    return;
  }

  else{
    strcat(buf_out, "-");
    strcat(buf_out, elem->client_ip);
    strcat(buf_out, "-");

    if((elem->a_flag) == AVAILABLE){
      strcat(buf_out, "a");
      strcat(buf_out, "-\n");
      return;
    }
    else{
      strcat(buf_out, "u");
      strcat(buf_out, "-\n");
      return;
    }
  }
}

//client-process/server-thread communication routine
void* connection_handler(void* arg){
  thread_args_t* args = (thread_args_t*)arg;

  int ret;

  //ret = fcntl(args->socket, F_SETFL, fcntl(args->socket, F_GETFL) | O_NONBLOCK);
  //ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot set socket in non-blocking mode");

  fprintf(stderr, "[CONNECTION THREAD]: allocazione user element da inserire nella lista\n");

  //user list element
  usr_list_elem_t* element = (usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));

  //filling element struct with client data;
  element->client_ip = (char*)malloc(INET_ADDRSTRLEN*sizeof(char));
  memcpy(element->client_ip, args->client_ip, INET_ADDRSTRLEN);
  element->a_flag = AVAILABLE;

  fprintf(stdout, "BBBBBBBBBBBBB%s", args->client_user_name);

  //inserting user into hash-table userlist
  ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");

  ret = INSERT(user_list, (gpointer)args->client_user_name, (gpointer)element);

  fprintf(stderr, "[CONNECTION THREAD]: elemento inserito con successo\n");

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");

  //pushing updates to mailboxes
  char newComand = NEW;
  FOR_EACH(mailbox_list, (GHFunc)push_entry, build_mailbox_message(args->client_user_name, &newComand));

  //unlock sender thread
  fprintf(stdout, "[CONNECTION THREAD]: %s\n", args->client_user_name);
  ret = sem_post(args->chandler_sender_sync);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot post on chandler_sender_sync");

  //command receiver buffer
  char* buf_command = (char*)calloc(2,sizeof(char));

  while(1){
    int ret = recv_msg(args->socket, buf_command, 2); //TO BE FIXED
    ERROR_HELPER(ret, "[CONNECTION THREAD][ERROR]: cannot receive server command from client");

    execute_command(args, buf_command, element);
  }

  /*CLOSE OPERATIONS (TO BE WRITTEN IN A FUNCTION)
  ret = close(args->socket);//close client_desc
  ERROR_HELPER(ret, "Cannot close socket for incoming connection");

  free(args->client_ip); //free of client_ip dotted
  //free(args);*/
  pthread_exit(NULL);
}

//list changes communication routine
void* sender_routine(void* arg){
  int ret;

  sender_thread_args_t* args = (sender_thread_args_t*)arg;

  //referring mailbox
  GAsyncQueue* my_mailbox = REF(args->mailbox);

  fprintf(stderr, "[SENDER THREAD]: inizializzazione indirizzo thread receiver\n");

  struct sockaddr_in rec_addr = {0};
  rec_addr.sin_family         = AF_INET;
  rec_addr.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);
  rec_addr.sin_addr.s_addr    = inet_addr(args->client_ip);

  fprintf(stderr, "[SENDER THREAD]: indirizzo thread receiver inizializzato con successo\n");

  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Cannot create sender thread socket");

  ret = connect(socket_desc, (struct sockaddr*) &rec_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to client receiver thread");

  fprintf(stderr, "[SENDER THREAD]: conneso al receiver thread\n");

  ret = sem_wait(args->chandler_sender_sync);//aspetto che cHandler abbia inserito nella lista per eseguire l'invio completo
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on chandler_sender_sync semaphore");
  //sending list to client (thread safe)
  FOR_EACH(user_list, (GHFunc)send_list_on_client_connection, &socket_desc);

  //retrieving changes from mailbox and sending to client
  //buffer to be sent
  char* send_buf = (char*)calloc(USERLIST_BUFF_SIZE, sizeof(char));
  //aux command buffer
  char buf_command;
  //aux username pointer to key of userlist hash table
  char* username;

  while(1){
    mailbox_message_t* message = POP(my_mailbox, (guint64)POP_TIMEOUT);
    if(message == NULL)continue;
    //extract command from message
    buf_command = *(message->mod_command);
    username = message->client_user_name;

    //using username to retrieve changes from userlist
    ret = sem_wait(&user_list_mutex);//wait for other threads
    ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on user_list_mutex semaphore");
    usr_list_elem_t* element_to_serialize = (usr_list_elem_t*)LOOKUP(user_list, username);

    //serializing message
    serialize_user_element(send_buf, element_to_serialize, username, buf_command);
    fprintf(stdout, "[SENDER THREAD]: message serialized = %s\n", send_buf);

    ret = sem_post(&user_list_mutex);//unlock semaphore
    ERROR_HELPER(ret, "[SENDER THREAD]: cannot post on user_list_mutex semaphore");

    free(message->mod_command);
    //not freeing client username becuause is freed in free_user_list_element_key
    free(message);

    //sending message to client's receiver thread
    send_msg(socket_desc, send_buf);
    fprintf(stdout, "[SENDER THREAD]: message sended to client's reciever thread\n");
    bzero(send_buf, USERLIST_BUFF_SIZE);
  }
  //CLOSE OPERATIONS TO BE HANDLED BY SIGNAL handler
  UNREF(args->mailbox);
  free(send_buf);
  free(username);

  ret = close(socket_desc);
  ERROR_HELPER(ret, "Error closing socket_desc in sender routine");

  fprintf(stdout, "[SENDER THREAD]: routine exit point\n");
  pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
  int ret, server_desc, client_desc;

  thread_count = 0;

  //init server userlist
  user_list = usr_list_init();

  //init mailbox_list
  mailbox_list = mailbox_list_init();

  //init user_list_mutex
  ret = sem_init(&user_list_mutex, 0, 1);
  ERROR_HELPER(ret, "[FATAL ERROR] Could not init user_list_mutex semaphore");

  //init mailbox_list_mutex
  ret = sem_init(&mailbox_list_mutex, 0, 1);
  ERROR_HELPER(ret, "[FATAL ERROR] Could not init mailbox_list_mutex semaphore");

  //init thread_count_mutex
  ret = sem_init(&thread_count_mutex, 0, 1);
  ERROR_HELPER(ret, "[FATAL ERROR] Could not init thread_count_mutex semaphore");

  struct sockaddr_in server_addr = {0};
  int sockaddr_len = sizeof(struct sockaddr_in);

  // initialize socket for listening
  server_desc = socket(AF_INET , SOCK_STREAM , 0);
  ERROR_HELPER(server_desc, "Could not create socket");

  server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

  //we enable SO_REUSEADDR to quickly restart our server after a crash
  int reuseaddr_opt = 1;
  ret = setsockopt(server_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

  // bind address to socket
  ret = bind(server_desc, (struct sockaddr*) &server_addr, sockaddr_len);
  ERROR_HELPER(ret, "Cannot bind address to socket");

  // start listening
  ret = listen(server_desc, MAX_CONN_QUEUE);
  ERROR_HELPER(ret, "Cannot listen on socket");

  // we allocate client_addr dynamically and initialize it to zero
  struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

  fprintf(stderr, "[MAIN]: fine inizializzazione, entro nel while di accettazione\n");

  // loop to manage incoming connections spawning handler threads
    while (1) {
      client_desc = accept(server_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
      if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
      ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

      fprintf(stderr, "[MAIN]: connessione accettata\n");

      //parsing client ip in dotted form
      char* client_ip_buf = inet_ntoa(client_addr->sin_addr);

      //semaphore for sync a pair of chandler/sender
      sem_t* chandler_sender_sync = (sem_t*)malloc(sizeof(sem_t));
      ret = sem_init(chandler_sender_sync, 0, 0);
      ERROR_HELPER(ret, "[MAIN]:cannot init chandler_sender_sync sempahore");

      //mailbox creation
      //init mailbox for sender thread and chandlers
      GAsyncQueue* mailbox_queue = mailbox_queue_init();

      //receiving username
      char* username_receiver_buf = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
      get_and_check_username(client_desc, username_receiver_buf);


      //thread spawning
      //*********************
      //client handler thread
      //arguments allocation
      char* client_user_name = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
      char* client_ip = (char*)malloc(INET_ADDRSTRLEN*sizeof(char));
      memcpy(client_ip, client_ip_buf, INET_ADDRSTRLEN);

      thread_args_t* thread_args = (thread_args_t*)malloc(sizeof(thread_args_t));
      thread_args->socket               = client_desc;
      thread_args->client_user_name     = client_user_name;
      thread_args->client_ip            = client_ip;
      thread_args->chandler_sender_sync = chandler_sender_sync;
      thread_args->mailbox_key          = username_receiver_buf;

      //inserting  mailbox_queue in mailbox hash table for usage by chandlers notifying system
      INSERT(mailbox_list, (gpointer)username_receiver_buf, (gpointer)mailbox_queue);

      //sender thread args
      //arguments allocation
      sender_thread_args_t* sender_args = (sender_thread_args_t*)malloc(sizeof(sender_thread_args_t));

      sender_args->chandler_sender_sync = chandler_sender_sync;
      sender_args->client_ip = (char*)malloc(sizeof(INET_ADDRSTRLEN));
      memcpy(sender_args->client_ip, client_ip_buf, INET_ADDRSTRLEN);
      sender_args->mailbox = mailbox_queue;

      fprintf(stderr, "[MAIN]: preparati argomenti per il sender thread\n");

      //print test
      fprintf(stdout, "[MAIN]: username: %s\n",username_receiver_buf);

      fprintf(stderr, "[MAIN]: ricezione username completata con successo\n");
      //copying username into struct
      memcpy(thread_args->client_user_name, username_receiver_buf, strlen(username_receiver_buf));

      //connection handler thread spawning
      pthread_t thread_client;
      ret = pthread_create(&thread_client, NULL, connection_handler, (void*)thread_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

      ret = pthread_detach(thread_client);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");

      fprintf(stderr, "[MAIN]:spawnato connection thread per il client accettato\n");

      pthread_t thread_sender;
      ret = pthread_create(&thread_sender, NULL, sender_routine, (void*)sender_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create sender thread");

      ret = pthread_detach(thread_sender);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");

      fprintf(stderr, "[MAIN]: spawnato sender thread\n");

      //new buffer for new incoming connection
      client_addr = calloc(1, sizeof(struct sockaddr_in));

      //incrementing progressive number of threads
      //mutex with queue manager thread and connection handler thread
      sem_wait(&thread_count_mutex);
      thread_count++;
      sem_post(&thread_count_mutex);

      fprintf(stderr, "[MAIN]: fine loop di accettazione connessione in ingresso, inizio nuova iterazione\n");
    }

    exit(EXIT_SUCCESS);
}
