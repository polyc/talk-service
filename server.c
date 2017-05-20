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
sem_t mailbox_list_mutex; // mutual exclusion to acces mailbox hash-table
sem_t thread_count_mutex;
GHashTable* user_list;
GHashTable* mailbox_list;
int thread_count;

//receive username from client and check if it's already used by another connected client
void get_and_check_username(int socket, char* username){
  char* send_buf = (char*)calloc(2, sizeof(char)); //buffer used to send response to client
  while(1){
    int ret = recv_msg(socket, username, USERNAME_BUF_SIZE);
    ERROR_HELPER(ret, "[MAIN]: cannot receive username");

    ret = sem_wait(&user_list_mutex);
    ERROR_HELPER(ret, "[MAIN]:cannot wait on user_list_mutex");

    //check if username is in user_list GHashTable
    if(CONTAINS(user_list, username) == FALSE){
      ret = sem_post(&user_list_mutex);
      ERROR_HELPER(ret, "[MAIN]:cannot post on user_list_mutex");

      send_buf[0] = AVAILABLE;
      send_buf[1] = '\n';
      send_msg(socket, send_buf); //sending OK to client
      break;
    }
    else{
      ret = sem_post(&user_list_mutex);
      ERROR_HELPER(ret, "[MAIN]:cannot post on user_list_mutex");

      send_buf[0] = UNAVAILABLE;
      send_buf[1] = '\n';
      send_msg(socket, send_buf);
      memset(username, USERNAME_BUF_SIZE, 0);
      memset(send_buf, 2, 0);
    }
  }
  fprintf(stdout, "[MAIN]: username getted\n");
  return;
}

void update_availability(usr_list_elem_t* elem_to_update, char* buf_command){
  //updating user_list
  int ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD][UPDATING AVAILABILITY]: cannot wait on user_list_mutex");

  elem_to_update->a_flag = *buf_command; //update availability flag

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD][UPDATING AVAILABILITY]: cannot post on user_list_mutex");

  return;
}

//remove entries from hash tables when a client disconnects from server
void remove_entry(char* elem_to_remove, char* mailbox_to_remove){
  //removing from userlist
  gint ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: could not wait on user_list_mutex_list semaphore");

  ret = REMOVE(user_list, elem_to_remove); //remove entry
  if(ret == FALSE){
    ERROR_HELPER(-1, "[CONNECTION THREAD][REMOVING ENTRY]: remove entry failed");
  }

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: could not post on user_list_mutex_list semaphore");

  //removing from mailboxlist
  ret = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: could not wait on mailbox_list_mutex semaphore");

  ret = REMOVE(mailbox_list, mailbox_to_remove);

  ret = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: could not post on mailbox_list_mutex semaphore");

  if(ret == FALSE){
    ERROR_HELPER(-1, "[CONNECTION THREAD][REMOVING ENTRY]: remove entry failed");
  }

  fprintf(stdout, "[CONNECTION THREAD] successfully removed entry on disconnect operation\n");
  return;
}

//pushing message into sender thread personal GAsyncQueue
void push_entry(gpointer key, gpointer value, gpointer user_data/*parsed message*/){
    char* message = (char*)malloc(USERLIST_BUFF_SIZE*sizeof(char));
    message = strcpy(message, (char*)user_data);
  //if((strcmp((char*)key, ((mailbox_message_t*)(user_data))->client_user_name)) != 0){
    PUSH((GAsyncQueue*)value, (gpointer)message);
  //}
  return;
}

//sender retrieve message from its queue
void pop_entry(){}

//function called by FOR_EACH. It sends userlist on connection to receiver thread in client
void send_list_on_client_connection(gpointer key, gpointer value, gpointer user_data){

  char* buf = (char*)calloc(USERLIST_BUFF_SIZE, sizeof(char));

  serialize_user_element(buf, (usr_list_elem_t*)value, (char*)key, NEW);
  //fprintf(stdout, "[SENDING LIST]: serialized element: %s\n", buf);

  //fprintf(stdout, "[SENDING LIST][USERNAME]: %s\n", (char*)key);
  //fprintf(stdout,"[SENDING LIST][IP]: %s\n", (char*)(((usr_list_elem_t*)value)->client_ip));
  //fprintf(stdout, "[SENDING LIST][FLAG]: %c\n", (char)(((usr_list_elem_t*)value)->a_flag));

  send_msg(*((int*)user_data), buf); //(socket, buf);
  return;
}

int execute_command(thread_args_t* args, char* availability_buf, usr_list_elem_t* element_to_update){
  int ret = 0;
  //selecting correct command
  char availability = availability_buf[0];
  char mod_command;

  fprintf(stdout, "[CONNECTION THREAD] availability: %c\n", availability);

  char* message = (char*)calloc(USERLIST_BUFF_SIZE ,sizeof(char));

  switch(availability){

    case UNAVAILABLE :
      mod_command = MODIFY;
      update_availability(element_to_update, &availability);

      serialize_user_element(message, element_to_update, args->client_user_name, mod_command);

      fprintf(stdout, "[CONNECTION THREAD]: unavailable command processed\n");
      break;

    case AVAILABLE :
      mod_command = MODIFY;
      update_availability(element_to_update, &availability);

      serialize_user_element(message, element_to_update, args->client_user_name, mod_command);

      fprintf(stdout, "[CONNECTION THREAD]: available command procesed\n");
      break;

    case DISCONNECT:
      mod_command = DELETE;
      remove_entry(args->client_user_name, args->mailbox_key);

      //notify sender thread the termination condition
      GAsyncQueue* mailbox = (GAsyncQueue*)LOOKUP(mailbox_list, args->mailbox_key);
      message = "exit";
      PUSH(mailbox, (gpointer)message);

      //serializing update for other senders
      serialize_user_element(message, element_to_update, args->client_user_name, mod_command);

      fprintf(stdout, "[CONNECTION THREAD]: disconnect command processed\n");
      ret = -1;
      break;
    default :
      //throw error
      return ret;
  }

  //pushing updates to mailboxes
  int err = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(err, "[CONNECTION THREAD]: cannot wait on user_list_mutex");

  FOR_EACH(mailbox_list, (GHFunc)push_entry, (gpointer)message);

  err = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(err, "[CONNECTION THREAD]: cannot post on user_list_mutex");

  free(message);
  return ret;
}

//transform a usr_list_elem_t in a string according to mod_command
void serialize_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command){
  fprintf(stdout, "[SERIALIZE]: sono dentro la funzione di serializzazione\n");
  buf_out[0] = mod_command;
  buf_out = strcat(buf_out, "-");
  buf_out = strcat(buf_out, buf_username);
  fprintf(stdout, "[SERIALIZE]: maremma maiala\n");
  char* a_flag_buf = (char*)malloc(sizeof(char));
  *(a_flag_buf)= elem->a_flag;


  if(mod_command == DELETE){
    buf_out = strcat(buf_out ,"-\n");
    return;
  }
  else if (mod_command == NEW){
    buf_out = strcat(buf_out, "-");
    buf_out = strcat(buf_out, elem->client_ip);
    buf_out = strcat(buf_out, "-");
    buf_out = strcat(buf_out, a_flag_buf);
    buf_out = strcat(buf_out, "-\n");
    return;
  }

  else{
    buf_out = strcat(buf_out, "-");
    buf_out = strcat(buf_out, elem->client_ip);
    buf_out = strcat(buf_out, "-");

    if(*(a_flag_buf) == AVAILABLE){
      buf_out = strcat(buf_out, "a");
      buf_out = strcat(buf_out, "-\n");
      return;
    }
    else{
      buf_out = strcat(buf_out, "u");
      buf_out = strcat(buf_out, "-\n");
      return;
    }
  }
}

//client-process/server-threads communication routine
void* connection_handler(void* arg){
  thread_args_t* args = (thread_args_t*)arg;

  int ret;

  //get username
  get_and_check_username(args->socket, args->client_user_name);

  fprintf(stderr, "[CONNECTION THREAD %d]: allocazione user element da inserire nella lista\n", args->id);

  //user list element
  usr_list_elem_t* element = (usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));

  //filling element struct with client data;
  element->client_ip = (char*)malloc(INET_ADDRSTRLEN*sizeof(char));
  memcpy(element->client_ip, args->client_ip, strlen(args->client_ip)+1);
  element->a_flag = UNAVAILABLE;


  //inserting user into hash-table userlist
  ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on user_list_mutex");

  ret = INSERT(user_list, (gpointer)args->client_user_name, (gpointer)element);

  fprintf(stderr, "[CONNECTION THREAD %d]: elemento inserito con successo\n", args->id);

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");


  //unlock sender thread
  fprintf(stdout, "[CONNECTION THREAD %d]: %s\n",args->id, args->client_user_name);

  ret = sem_post(args->chandler_sync);
  ERROR_HELPER(ret, "[CONNECTION THREAD %d]:cannot post on chandler_sync");


  //wait for sender thread
  ret = sem_wait(args->sender_sync);
  ERROR_HELPER(ret, "[CONNECTION THREAD %d]:cannot wait on chandler_sync");

  //pushing updates to mailboxes
  char* message = (char*)calloc(USERLIST_BUFF_SIZE, sizeof(char));
  serialize_user_element(message, element, args->client_user_name, NEW);

  ret = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on user_list_mutex");

  FOR_EACH(mailbox_list, (GHFunc)push_entry, (gpointer)message);

  ret = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");


  //command receiver buffer
  char* buf_command = (char*)calloc(2,sizeof(char));

  //RECEIVING COMMANDS
  while(1){
    int ret = recv_msg(args->socket, buf_command, 2);
    ERROR_HELPER(ret, "[CONNECTION THREAD][ERROR]: cannot receive server command from client");

    ret = execute_command(args, buf_command, element);
    if (ret < 0) break; //exit condition
  }

  //close operations
  free(buf_command);

  ret = close(args->socket);

  free(args->client_ip);
  free(args->client_user_name);

  ret = sem_destroy(args->chandler_sync);
  ERROR_HELPER(ret, "[CONNECTION THREAD][ERROR]: cannot destroy chandler_sync semaphore");

  free(args);

  pthread_exit(EXIT_SUCCESS);
}

//list changes communication routine
void* sender_routine(void* arg){
  int ret;

  sender_thread_args_t* args = (sender_thread_args_t*)arg;

  fprintf(stderr, "[SENDER THREAD %d]: inizializzazione indirizzo client thread receiver\n", args->id);

  struct sockaddr_in rec_addr = {0};
  rec_addr.sin_family         = AF_INET;
  rec_addr.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);
  rec_addr.sin_addr.s_addr    = inet_addr(args->client_ip);

  fprintf(stderr, "[SENDER THREAD %d]: indirizzo client thread receiver inizializzato con successo\n", args->id);

  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Cannot create sender thread socket");

  ret = connect(socket_desc, (struct sockaddr*) &rec_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to client receiver thread");

  fprintf(stderr, "[SENDER THREAD %d]: conneso al receiver thread\n", args->id);

  ret = sem_wait(args->chandler_sync);//aspetto che cHandler abbia inserito i dati nella userlist
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on chandler_sync semaphore");

  //inserting mailbox in hash-table mailbox list
  ret = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: could not wait on mailbox_list semaphore");

  INSERT(mailbox_list, (gpointer)(args->mailbox_key), (gpointer)(args->mailbox));
  fprintf(stdout, "[SENDER THREAD]: inserted entry in mailbox_list\n");

  ret = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: Could not wait on mailbox_list semaphore");


  //referring mailbox
  GAsyncQueue* my_mailbox = REF(args->mailbox);

  //unlock chandler thread
  ret = sem_post(args->chandler_sync);
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot post on chandler_sync semaphore");

  //sending list to client
  ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on user_list_mutex");

  FOR_EACH(user_list, (GHFunc)send_list_on_client_connection, (gpointer)&socket_desc);

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on user_list_mutex");
  fprintf(stderr, "[SENDER THREAD %d]: sended list on first connection", args->id);

  //retrieving changes from mailbox
  while(1){
    char* message = (char*)POP(my_mailbox, POP_TIMEOUT);

    if(message == NULL)continue;

    if (strcmp(message, "exit") == 0) {
      fprintf(stdout, "[SENDER THREAD %d: exiting sender routine]\n", args->id);
      break;
    }

    //sending message to client's receiver thread
    send_msg(socket_desc, message);
    fprintf(stdout, "[SENDER THREAD %d]: message sended to client's reciever thread\n", args->id);
  }
  //Close operations
  UNREF(args->mailbox);

  ret = close(socket_desc);
  ERROR_HELPER(ret, "Error closing socket_desc in sender routine");

  ret = sem_destroy(args->sender_sync);
  ERROR_HELPER(ret, "[SENDER THREAD][ERROR]: cannot destroy sender_sync semaphore");

  free(args->client_ip);
  free(args);

  fprintf(stdout, "[SENDER THREAD %d]: routine exit point\n", args->id);
  pthread_exit(EXIT_SUCCESS);
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

      //semaphores for syncing chandler and sender threads
      sem_t* chandler_sync = (sem_t*)malloc(sizeof(sem_t));
      ret = sem_init(chandler_sync, 0, 0);
      ERROR_HELPER(ret, "[MAIN]:cannot init chandler_sync sempahore");

      sem_t* sender_sync = (sem_t*)malloc(sizeof(sem_t));
      ret = sem_init(chandler_sync, 0, 0);
      ERROR_HELPER(ret, "[MAIN]:cannot init sender_sync sempahore");

      //mailbox init
      GAsyncQueue* mailbox_queue = mailbox_queue_init();


      //thread spawning
      //*********************

      //client handler thread
      //arguments allocation
      char* client_user_name = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
      char* client_ip = (char*)malloc(INET_ADDRSTRLEN*sizeof(char));
      memcpy(client_ip, client_ip_buf, strlen(client_ip_buf)+1);

      thread_args_t* thread_args = (thread_args_t*)malloc(sizeof(thread_args_t));
      thread_args->socket               = client_desc;
      thread_args->client_user_name     = client_user_name;
      thread_args->client_ip            = client_ip;
      thread_args->chandler_sync        = chandler_sync;
      thread_args->sender_sync          = sender_sync;
      thread_args->mailbox_key          = client_user_name;
      thread_args->id                   = thread_count;


      //sender thread args
      //arguments allocation
      sender_thread_args_t* sender_args = (sender_thread_args_t*)malloc(sizeof(sender_thread_args_t));

      sender_args->chandler_sync      = chandler_sync;
      sender_args->sender_sync        = sender_sync;
      sender_args->client_ip          = (char*)malloc(sizeof(INET_ADDRSTRLEN));
      memcpy(sender_args->client_ip, client_ip_buf, strlen(client_ip_buf)+1);
      sender_args->mailbox            = mailbox_queue;
      sender_args->mailbox_key        = client_user_name;
      sender_args->id                 = thread_count;

      fprintf(stderr, "[MAIN]: preparati argomenti per il sender thread\n");


      //connection handler thread spawning
      pthread_t thread_client;
      ret = pthread_create(&thread_client, NULL, connection_handler, (void*)thread_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

      fprintf(stderr, "[MAIN]:spawnato connection thread per il client accettato\n");

      //sender thread spawning
      pthread_t thread_sender;
      ret = pthread_create(&thread_sender, NULL, sender_routine, (void*)sender_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create sender thread");

      fprintf(stderr, "[MAIN]: spawnato sender thread\n");

      //new buffer for new incoming connection
      client_addr = calloc(1, sizeof(struct sockaddr_in));

      //incrementing progressive number of threads
      sem_wait(&thread_count_mutex);
      thread_count++;
      sem_post(&thread_count_mutex);

      fprintf(stderr, "[MAIN]: fine loop di accettazione connessione in ingresso, inizio nuova iterazione\n");
    }

    exit(EXIT_SUCCESS);
}
