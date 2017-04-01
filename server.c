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


GHashTable* user_list;

//transform a usr_list_elem_t in a string according to mod_command
void* stringify_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command){
  fprintf(stdout, "sono dentro la funzione di serializzazione\n");
  *buf_out = "";
  strcat(buf_out, &mod_command);
  strcat(buf_out, "-");
  strcat(buf_out, buf_username);
  fprintf(stdout, "maremma maiala\n");


  if(mod_command == DELETE){
    strcat(buf_out ,"-\n");
    return;
  }
  else if (mod_command == NEW){
    fprintf(stdout, "%s\n", elem->client_ip);
    strcat(buf_out, "-");
    strcat(buf_out, elem->client_ip);
    strcat(buf_out, "-");
    fprintf(stdout, "%s\n", &(elem->a_flag));
    strcat(buf_out, &(elem->a_flag));
    strcat(buf_out, "-\n");
    return;
  }
  else{//mod_command == MOD
    strcat(buf_out, &(elem->a_flag));
    strcat(buf_out, "-\n");
    return;
  }
}

//client-process/server-thread communication routine
void* connection_handler(void* arg){

  thread_args_t* args = (thread_args_t*)arg;

  int ret;
  //int recv_bytes;

  /*//COMMAND BUFFERS
  char* quit_command = SERVER_QUIT;//quit command buffer
  size_t quit_command_len = strlen(quit_command);

  char* a_command = AVAILABLE;//available command buffer
  size_t a_command_len = strlen(a_command);

  char* u_command = UNAVAILABLE;//unavailable command buffer
  size_t u_command_len = strlen(u_command);
  */

  fprintf(stderr, "flag 2\n");

  //user list element
  usr_list_elem_t* element = (usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));

  //filling element struct with client data;
  element->client_ip = (char*)malloc(INET_ADDRSTRLEN*sizeof(char));
  memcpy(element->client_ip, args->client_ip, INET_ADDRSTRLEN);
  element->a_flag = AVAILABLE;

  //inserting user into hash-table userlist
  ret = INSERT(user_list, (gpointer)args->client_user_name, (gpointer)element);
  if(ret == 0) fprintf(stdout, "yet present\n");
  while(1){}

  fprintf(stderr, "flag 10\n");
  //CLOSE OPERATIONS (TO BE COMPLETED)
  ret = close(args->socket);//close client_desc
  ERROR_HELPER(ret, "Cannot close socket for incoming connection");

  free(args->client_ip); //free of client_ip dotted
  //free(args);
  pthread_exit(NULL);
}

//list changes communication routine
void* sender_routine(void* arg){
  sender_thread_args_t* args = (sender_thread_args_t*)arg;

  fprintf(stderr, "flag 11\n");

  struct sockaddr_in rec_addr = {0};
  rec_addr.sin_family         = AF_INET;
  rec_addr.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);
  rec_addr.sin_addr.s_addr    = inet_addr(LOCAL_IP);

  fprintf(stderr, "flag 12\n");

  int ret, bytes_left, bytes_sent = 0;

  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Cannot create sender thread socket");

  ret = connect(socket_desc, (struct sockaddr*) &rec_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to client receiver thread");

  fprintf(stderr, "flag 13\n");

  //sending test buffer
  char* buf= (char*)calloc(USERLIST_BUFF_SIZE, sizeof(char));
  buf[0] = '1';
  buf[1] = '\n';
  bytes_left = 2;

  fprintf(stderr, "flag 14\n");

  //sending #mod
  while (bytes_left > 0){
      ret = send(socket_desc, buf + bytes_sent, bytes_left, 0);
      fprintf(stderr, "flag 15\n");
      if (ret == -1 && errno == EINTR){
        continue;
      }
      ERROR_HELPER(ret, "Error while sending number of modifications to server");

      bytes_left -= ret;
      bytes_sent += ret;
  }
  bzero(buf, USERLIST_BUFF_SIZE);

  char* test_username = (char*)calloc(8, sizeof(char));
  memcpy(test_username, "fulco_94", 8);


  //retreiving user information from hash table
  usr_list_elem_t* element = (usr_list_elem_t*)LOOKUP(user_list, (gconstpointer)test_username);

  fprintf(stdout,"%s\n", element->client_ip);
  fprintf(stdout, "%c\n", element->a_flag);
  fprintf(stderr, "flag 16\n");

  char mod_command = 'm';
  fprintf(stdout, "%c\n", mod_command);

  //serializing user element
  stringify_user_element(buf, element, test_username, mod_command);
  fprintf(stdout, "%s\n", buf);
  free(test_username);

  //sending user data to client;
  bytes_left = strlen(buf);
  bytes_sent = 0;

  fprintf(stderr, "flag 17\n");

  while (bytes_left > 0){
      ret = send(socket_desc, buf + bytes_sent, bytes_left, 0);

      fprintf(stderr, "flag 17\n");

      if (ret == -1 && errno == EINTR){
        continue;
      }
      ERROR_HELPER(ret, "Error while sending serialized user to server");

      bytes_left -= ret;
      bytes_sent += ret;
  }

  fprintf(stderr, "flag 18\n");

  ret = close(socket_desc);
  ERROR_HELPER(ret, "Error closing socket_desc in sender routine");

  //free(buf);
  fprintf(stdout, "end sender routine\n");
  pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
  int ret, server_desc, client_desc, thread_count = 0;

  //generating server userlist
  user_list = usr_list_init();

  //generating thread data structure
  GHashTable* thread_ref = thread_ref_init();

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

  fprintf(stderr, "flag 0\n");

  // loop to manage incoming connections spawning handler threads
    while (1) {
      client_desc = accept(server_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
      if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
      ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

      fprintf(stderr, "flag 1\n");

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
      fprintf(stdout, "%s\n",buf);

      fprintf(stderr, "flag4");
      //copying username into struct
      memcpy(thread_args->client_user_name, buf, strlen(buf));
      free(buf);//free of username buffer

      //allocation of memory for hash table
      int* thread_id = (int*)malloc(sizeof(int));
      *(thread_id) = thread_count;

      //insertion of thread i into hash-table with its args as value
      INSERT(thread_ref, (gpointer)thread_id, (gpointer)thread_args);

      fprintf(stderr, "flag5");

      fprintf(stderr, "flag6");

      pthread_t thread_client;
      ret = pthread_create(&thread_client, NULL, connection_handler, (void*)thread_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

      ret = pthread_detach(thread_client);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");


      fprintf(stderr, "flag7");

      //sender thread args and spawning
      sender_thread_args_t* sender_args = (sender_thread_args_t*)malloc(sizeof(sender_thread_args_t));

      fprintf(stderr, "flag8");

      pthread_t thread_sender;
      ret = pthread_create(&thread_sender, NULL, sender_routine, (void*)sender_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create sender thread");

      fprintf(stderr, "flag9");

      //new buffer for new incoming connection
      client_addr = calloc(1, sizeof(struct sockaddr_in));
      //incrementing progressive number of threads
      thread_count++;

      ret = pthread_detach(thread_sender);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");
    }

    exit(EXIT_SUCCESS);
}
