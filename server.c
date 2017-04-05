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

sem_t mutex_cnnHandler_sender; // sync between connection handler thread and its sender thread ONE FOR EACH THREAD IN NEAR FUTURE
GHashTable* user_list;
GHashTable* thread_ref;

void receive_and_execute_command(thread_args_t* args, char* buf_command){
  int ret = recv_msg(args->socket, buf_command, 1);
  ERROR_HELPER(ret, "cannot receive server command from client");

  //selecting correct command
  switch(*buf_command){
    case UNAVAILABLE :
      //update hash table;
      //queue insertion;
      break;
    case AVAILABLE :
      //update hash table;
      //queue insertion;
      break;
    case DISCONNECT :
      //update hash table;
      //queue insertion;
      //thread's close operations;
      break;//never executed beacuse in close operations, the thread exit safely
    default :
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
  ret = INSERT(user_list, (gpointer)args->client_user_name, (gpointer)element);
  if(ret == 0) fprintf(stdout, "[CONNECTION THREAD]: user is already in list\n");
  fprintf(stderr, "[CONNECTION THREAD]: elemento inserito con successo\n");
  ret = sem_post(&mutex_cnnHandler_sender);
  ERROR_HELPER(ret, "cannot post on mutex_cnnHandler_sender");

  //command receiver buffer
  char* buf_command = (char*)calloc(1, sizeof(char));

  while(1){
    receive_and_execute_command(args, buf_command);
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
  sender_thread_args_t* args = (sender_thread_args_t*)arg;

  fprintf(stderr, "[SENDER THREAD]: inizializzazione indirizzo thread receiver\n");

  struct sockaddr_in rec_addr = {0};
  rec_addr.sin_family         = AF_INET;
  rec_addr.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);
  rec_addr.sin_addr.s_addr    = inet_addr(LOCAL_IP);

  fprintf(stderr, "[SENDER THREAD]: indirizzo thread receiver inizializzato con successo\n");

  int ret, bytes_left, bytes_sent = 0;

  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Cannot create sender thread socket");

  ret = connect(socket_desc, (struct sockaddr*) &rec_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to client receiver thread");

  fprintf(stderr, "[SENDER THREAD]: conneso al receiver thread\n");

  //sending test buffer
  char* buf= (char*)calloc(USERLIST_BUFF_SIZE, sizeof(char));
  buf[0] = '1';
  buf[1] = '\n';
  //bytes_left = 2;

  fprintf(stderr, "[SENDER THREAD]: allocato e preparato buffer di invio\n");

  send_msg(socket_desc, buf);
  fprintf(stderr, "[SENDER THREAD]: #modifiche inviate con successo\n");
  bzero(buf, USERLIST_BUFF_SIZE);

  char* test_username = (char*)calloc(8, sizeof(char));
  memcpy(test_username, "fulco_94", 8);

  //retreiving user information from hash table
  ret = sem_wait(&mutex_cnnHandler_sender);
  ERROR_HELPER(ret, "cannot wait on mutex_cnnHandler_sender");
  usr_list_elem_t* element = (usr_list_elem_t*)LOOKUP(user_list, (gconstpointer)test_username);
  if(element == NULL){
    ret = -1;
    ERROR_HELPER(ret, "[SENDER THREAD]user not present in list");
  }

  fprintf(stdout, "[SENDER THREAD][USERNAME]: %s\n", test_username);
  fprintf(stdout,"[SENDER THREAD][IP]: %s\n", element->client_ip);
  fprintf(stdout, "[SENDER THREAD][FLAG]: %c\n", element->a_flag);

  char mod_command = 'n';

  //serializing user element
  stringify_user_element(buf, element, test_username, mod_command);
  fprintf(stdout, "[SENDER THREAD]: sono fuori la funzione di serializzazione\n");
  fprintf(stdout, "[SENDER THREAD][BUFFER READY]:%s\n", buf);
  free(test_username);
  ret = sem_post(&mutex_cnnHandler_sender);
  ERROR_HELPER(ret, "cannot post on mutex_cnnHandler_sender");

  //sending user data to client;
  send_msg(socket_desc, buf);
  fprintf(stderr, "[SENDER THREAD]: user element inviato con successo\n");

  ret = close(socket_desc);
  ERROR_HELPER(ret, "Error closing socket_desc in sender routine");

  //free(buf);
  fprintf(stdout, "[SENDER THREAD]: routine exit point\n");
  pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
  int ret, server_desc, client_desc, thread_count = 0;

  //generating server userlist
  user_list = usr_list_init();

  //generating server thread manipulation hash table
  thread_ref = thread_ref_init();

  //init mutex_cnnHandler_sender
  ret = sem_init(&mutex_cnnHandler_sender, 0, 1);
  ERROR_HELPER(ret, "[FATAL ERROR] Could not init mutex_cnnHandler_sender semaphore");

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

      //insertion of thread i into hash-table with its syncing semaphore needed to access userlist
      //INSERT(thread_ref, (gpointer)thread_args->client_user_name, (gpointer));

      fprintf(stderr, "[MAIN]: thread id inserito nella hash table dei thread\n");

      //connection handler thread spawning
      ret = sem_wait(&mutex_cnnHandler_sender);
      ERROR_HELPER(ret, "cannot wait on mutex_cnnHandler_sender");
      pthread_t thread_client;
      ret = pthread_create(&thread_client, NULL, connection_handler, (void*)thread_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

      ret = pthread_detach(thread_client);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");


      fprintf(stderr, "[MAIN]:spawnato connection thread per il client accettato\n");

      //sender thread args and spawning
      sender_thread_args_t* sender_args = (sender_thread_args_t*)malloc(sizeof(sender_thread_args_t));

      fprintf(stderr, "[MAIN]: allocati argomenti per il sender thread\n");

      pthread_t thread_sender;
      ret = pthread_create(&thread_sender, NULL, sender_routine, (void*)sender_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create sender thread");

      fprintf(stderr, "[MAIN]: spawnato sender thread\n");

      //new buffer for new incoming connection
      client_addr = calloc(1, sizeof(struct sockaddr_in));
      //incrementing progressive number of threads
      thread_count++;

      ret = pthread_detach(thread_sender);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");

      fprintf(stderr, "[MAIN]: fine loop di accettazione connessione in ingresso, inizio nuova iterazione\n");
    }

    exit(EXIT_SUCCESS);
}
