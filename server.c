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
sem_t changes_queue_mutex;
sem_t thread_count_mutex;
GHashTable* user_list;
GHashTable* thread_ref;
int thread_count;

void update_availability(usr_list_elem_t* elem_to_update, char* buf_command){
  //updating user_list
  int ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD][UPDATING AVAILABILITY]: cannot wait on user_list_mutex");

  elem_to_update->a_flag = *buf_command; //update availability flag

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD][UPDATING AVAILABILITY]: cannot post on user_list_mutex");

  return;
}

void remove_entry(char* elem_to_remove){//to befinished
  int ret = REMOVE(user_list, elem_to_remove); //remove entry
  if(ret == 0){
    ret = -1;
    ERROR_HELPER(ret, "[CONNECTION THREAD][REMOVING ENTRY]: remove entry failed");
  }
  return;
}

//pushing message into sender thread personal hash table changelog
void push_entry(char* parsed_message, GAsyncQueue* queue){
  g_async_queue_push (queue, (gpointer)parsed_message);
  return;
}

void message_broadcast(){}

//sender retrieve message from its queue
void pop_entry(){}

//function called by FOR_EACH. It send single user element to receiver thread in client
void send_list_on_client_connection(gpointer key, gpointer value, gpointer user_data){
  int ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDING LIST]: cannot wait on user_list_mutex");

  char* buf = calloc(USERNAME_BUF_SIZE, sizeof(char));
  stringify_user_element(buf, value, key, NEW);

  fprintf(stdout, "[SENDING LIST][USERNAME]: %s\n", (char*)key);
  fprintf(stdout,"[SENDING LIST][IP]: %s\n", (char*)((usr_list_elem_t*)value)->client_ip);
  fprintf(stdout, "[SENDING LIST][FLAG]: %c\n", (char)((usr_list_elem_t*)value)->a_flag);

  send_msg(*((int*)user_data), buf); //(socket, buf);
  free(buf);

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDING LIST]: cannot post on user_list_mutex");
  return;
}

void receive_and_execute_command(thread_args_t* args, char* buf_command, usr_list_elem_t* element_to_update){
  int ret = recv_msg(args->socket, buf_command, 1);
  ERROR_HELPER(ret, "cannot receive server command from client");

  //selecting correct command
  switch(*buf_command){
    case UNAVAILABLE :
      update_availability(element_to_update, buf_command);
      break;
    case AVAILABLE :
      update_availability(element_to_update, buf_command);
      break;
    case DISCONNECT:
      //remove entry
      //thread's close operations;
      break;//never executed beacuse in close operations, the thread exit safely
    default :
      //throw error
      return;
  }
  return;
}

//transform a usr_list_elem_t in a string according to mod_command
void stringify_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command){
  fprintf(stdout, "[SENDER THREAD]: sono dentro la funzione di serializzazione\n");
  *buf_out = "";
  buf_out[0] = mod_command;
  //strcat(buf_out, &mod_command);
  strcat(buf_out, "-");
  strcat(buf_out, buf_username);
  fprintf(stdout, "[SENDER THREAD]: maremma maiala\n");


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
  else{//mod_command == MOD
    strcat(buf_out, "-");
    strcat(buf_out, &(elem->a_flag));
    strcat(buf_out, "-\n");
    return;
  }
}

//client-process/server-thread communication routine
void* connection_handler(void* arg){
  thread_args_t* args = (thread_args_t*)arg;

  int ret;

  fprintf(stderr, "[CONNECTION THREAD]: allocazione user element da inserire nella lista\n");

  //user list element
  usr_list_elem_t* element = (usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));

  //filling element struct with client data;
  element->client_ip = (char*)malloc(INET_ADDRSTRLEN*sizeof(char));
  memcpy(element->client_ip, args->client_ip, INET_ADDRSTRLEN);
  element->a_flag = AVAILABLE;

  //inserting user into hash-table userlist
  ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");

  ret = INSERT(user_list, (gpointer)args->client_user_name, (gpointer)element);
  if(ret == 0){
    fprintf(stdout, "[CONNECTION THREAD]: user is already in list\n");
    pthread_exit(NULL);
  }
  fprintf(stderr, "[CONNECTION THREAD]: elemento inserito con successo\n");

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");

  //unlock sender thread
  //retrieving semphore
  sem_t* sem = (sem_t*)LOOKUP(thread_ref, args->client_user_name);
  ret = sem_post(sem);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot post on chandler_sender_sync");

  //command receiver buffer
  char* buf_command = (char*)calloc(1, sizeof(char));

  while(1){
    receive_and_execute_command(args, buf_command, element);
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

  fprintf(stderr, "[SENDER THREAD]: inizializzazione indirizzo thread receiver\n");

  struct sockaddr_in rec_addr = {0};
  rec_addr.sin_family         = AF_INET;
  rec_addr.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);
  rec_addr.sin_addr.s_addr    = inet_addr(LOCAL_IP);

  fprintf(stderr, "[SENDER THREAD]: indirizzo thread receiver inizializzato con successo\n");

  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Cannot create sender thread socket");

  ret = connect(socket_desc, (struct sockaddr*) &rec_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to client receiver thread");

  fprintf(stderr, "[SENDER THREAD]: conneso al receiver thread\n");

  ret = sem_wait(args->chandler_sender_sync);//aspetto che cHandler abbia inserito nella lista per eseguire l'invio completo
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on chandler_sender_sync semaphore");
  //sending list to client (thread safe)
  FOR_EACH(user_list, (GHFunc)send_list_on_client_connection, (gpointer)&socket_desc);

  ret = close(socket_desc);
  ERROR_HELPER(ret, "Error closing socket_desc in sender routine");

  fprintf(stdout, "[SENDER THREAD]: routine exit point\n");
  pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
  int ret, server_desc, client_desc;

  thread_count = 0;

  //generating server userlist
  user_list = usr_list_init();

  //generating server thread manipulation hash table
  thread_ref = thread_ref_init();

  //init user_list_mutex
  ret = sem_init(&user_list_mutex, 0, 1);
  ERROR_HELPER(ret, "[FATAL ERROR] Could not init user_list_mutex semaphore");

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

      //thread spawning

      //client management thread
      // put arguments for the new thread into a buffer
      thread_args_t* thread_args = (thread_args_t*)malloc(sizeof(thread_args_t));
      thread_args->socket           = client_desc;
      thread_args->client_user_name = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
      thread_args->client_ip        = (char*)malloc(INET_ADDRSTRLEN*sizeof(char));
      memcpy(thread_args->client_ip, client_ip_buf, INET_ADDRSTRLEN);

      //receiving username
      char* buf = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
      ret = recv_msg(client_desc , buf, USERNAME_BUF_SIZE);
      ERROR_HELPER(ret, "client closed the socket");

      //print test
      fprintf(stdout, "[MAIN]: username: %s\n",buf);

      fprintf(stderr, "[MAIN]: ricezione username completata con successo\n");
      //copying username into struct
      memcpy(thread_args->client_user_name, buf, strlen(buf));
      free(buf);//free of username buffer

      //connection handler thread spawning
      pthread_t thread_client;
      ret = pthread_create(&thread_client, NULL, connection_handler, (void*)thread_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

      ret = pthread_detach(thread_client);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");

      fprintf(stderr, "[MAIN]:spawnato connection thread per il client accettato\n");

      //sender thread args and spawning
      sender_thread_args_t* sender_args = (sender_thread_args_t*)malloc(sizeof(sender_thread_args_t));
      sender_args->chandler_sender_sync = (sem_t*)malloc(sizeof(sem_t));
      ret = sem_init(sender_args->chandler_sender_sync, 0, 0);
      ERROR_HELPER(ret, "[MAIN]:cannot init chandler_sender_sync sempahore");

      fprintf(stderr, "[MAIN]: allocati argomenti per il sender thread\n");

      //inserting semphore in hash table. cHandler thread need to wait on this
      INSERT(thread_ref, (gpointer)thread_args->client_user_name, (gpointer)sender_args->chandler_sender_sync);


      pthread_t thread_sender;
      ret = pthread_create(&thread_sender, NULL, sender_routine, (void*)sender_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create sender thread");

      fprintf(stderr, "[MAIN]: spawnato sender thread\n");

      //new buffer for new incoming connection
      client_addr = calloc(1, sizeof(struct sockaddr_in));

      //incrementing progressive number of threads
      //mutex with queue manager thread and connection handler thread
      sem_wait(&thread_count_mutex);
      thread_count++;
      sem_post(&thread_count_mutex);

      ret = pthread_detach(thread_sender);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");

      fprintf(stderr, "[MAIN]: fine loop di accettazione connessione in ingresso, inizio nuova iterazione\n");
    }

    exit(EXIT_SUCCESS);
}
