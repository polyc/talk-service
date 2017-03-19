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

#include "server.h"
#include "common.h"

/*void create_user_list_element(usr_list_elem_t* element, char* ip, thread_args_t* args, char* buf){
  element->client_ip = ip; //dotted form
  element->a_flag    = AVAILABLE;

  //receiving username
  while ((recv_bytes = recv(args->socket, buf, sizeof(buf), 0)) < 0) {
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "Cannot read from socket");
  }
  //print test
  for (size_t i = 0; i < sizeof(buf) ; i++) {
    printf(buf[i]);
  }
  printf("\n");

  //filling elemnt.user_name
  element.user_name = buf;

  return;
}*/

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

  //user list element buffer
  //struct usr_list_elem_t* element = (struct usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));

  fprintf(stderr, "flag 2\n");


  //filling element
  //create_user_list_element(element, thread_args.addr, args, buf);


  //fprintf(stderr, "message read\n");

  /*TD:
    -list insertion
  */

  //while(1){
    /*//read message from client
    while ((recv_bytes = recv(args->socket, buf, buf_len, 0)) < 0) {
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "Cannot read from socket");
    }

    // quit command check
    //if (recv_bytes == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;

    //TD: -other commands management*/
  //}
  fprintf(stderr, "flag 10\n");
  //CLOSE OPERATIONS (TO BE COMPLETED)
  ret = close(args->socket);//close client_desc
  ERROR_HELPER(ret, "Cannot close socket for incoming connection");

  free(args->addr);
  free(args);
  pthread_exit(NULL);
}

void* sender_routine(void* arg){
  sender_thread_args_t* args = (sender_thread_args_t*)arg;

  fprintf(stderr, "flag 11\n");

  struct sockaddr_in rec_addr = {0};
  rec_addr.sin_family         = AF_INET;
  rec_addr.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);
  rec_addr.sin_addr.s_addr    = inet_addr("127.0.0.1"); //args->receiver_addr

  fprintf(stderr, "flag 12\n");

  int ret, bytes_left, bytes_sent = 0;

  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(ret, "Cannot create sender thread socket");

  ret = connect(socket_desc, (struct sockaddr*) &rec_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to client receicer thread");

  fprintf(stderr, "flag 13\n");

  //sending test buffer
  char* buf= (char*)calloc(USERLIST_BUFF_SIZE, sizeof(char));
  buf[0] = '1';
  buf[1] = '\n';
  bytes_left = 2;

  fprintf(stderr, "flag 14\n");

  fprintf(stdout, "prova\n");
  //sending #mod
  while (bytes_left > 0){
      ret = send(socket_desc, buf + bytes_sent, bytes_left, 0);
      fprintf(stderr, "flag 15\n");
      if (ret == -1 && errno == EINTR){
        continue;
      }
      ERROR_HELPER(ret, "Error while sending username to server");

      bytes_left -= ret;
      bytes_sent += ret;
  }
  bzero(buf, USERLIST_BUFF_SIZE);

  fprintf(stderr, "flag 16\n");

  //sendig username ecc.
  buf = "regibald_94-127.0.0.1-a-0-\n";
  bytes_left = strlen(buf);
  bytes_sent = 0;

  fprintf(stderr, "flag 17\n");

  while (bytes_left > 0){
      ret = send(socket_desc, buf + bytes_sent, bytes_left, 0);

      fprintf(stderr, "flag 17\n");

      if (ret == -1 && errno == EINTR){
        continue;
      }
      ERROR_HELPER(ret, "Error while sending username to server");

      bytes_left -= ret;
      bytes_sent += ret;
  }

  fprintf(stderr, "flag 18\n");

  //free(buf);
  //fprintf(stdout, "end sender routine\n");
  pthread_exit(NULL);

}

int main(int argc, char const *argv[]) {
  int ret, server_desc, client_desc;

  //TD:list initialization

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

      //thread spawning

      //client management thread
      // put arguments for the new thread into a buffer
      thread_args_t* thread_args = (thread_args_t*)malloc(sizeof(thread_args_t)); // cambiare, fare array di args
      thread_args->socket           = client_desc;
      thread_args->thread_id        = 0; //new_thread_id(),not written yet;
      thread_args->client_user_name = (char*)malloc(17*sizeof(char));

      //receiving username
      int bytes_read = 0;
      //user list element, single slot receive buffer
      char buf[17] = {0};
      size_t buf_len = sizeof(buf);

      // messages longer than sizeof(buf) will not be read
      while (bytes_read <= sizeof(buf)) {
          ret = recv(client_desc, buf + bytes_read, 1, 0);

          if (ret == -1 && errno == EINTR) continue;
          ERROR_HELPER(ret, "Errore nella lettura da socket");

          // controlling last byte read
          if (buf[bytes_read] == '\n') break; // end of message

          bytes_read++;
      }

      buf[bytes_read] = '\0'; //adding string terminator

      //print test
      for (size_t i = 0; i < 17 ; i++) {
        if(buf[i]=='\0'){ //if end of string break
          break;
        }
        fprintf(stdout, "%c",buf[i]);
      }
      fprintf(stdout, "\n");

      fprintf(stderr, "flag4");
      //copying username into struct
      memcpy(thread_args->client_user_name, &buf, bytes_read);

      fprintf(stderr, "flag5");

      char* client_ip_buf = inet_ntoa(client_addr->sin_addr); //parsing addr to simplified dotted form
      thread_args->addr   = (char*)malloc(sizeof(client_ip_buf));// memory allocation for dotted address
      memcpy(thread_args->addr, client_ip_buf, sizeof(*(client_ip_buf))); //copying dotted address into struct

      fprintf(stderr, "flag6");

      pthread_t thread_client;
      ret = pthread_create(&thread_client, NULL, connection_handler, (void*)thread_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

      ret = pthread_detach(thread_client);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");

      fprintf(stderr, "flag7");

      //sender thread
      sender_thread_args_t* sender_args = (sender_thread_args_t*)malloc(sizeof(sender_thread_args_t));
      sender_args->receiver_addr = (char*)malloc(sizeof(client_ip_buf));
      memcpy(sender_args->receiver_addr, client_ip_buf, sizeof(*(client_ip_buf)));

      fprintf(stderr, "flag8");

      pthread_t thread_sender;
      ret = pthread_create(&thread_sender, NULL, sender_routine, (void*)sender_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create sender thread");

      fprintf(stderr, "flag9");

      //new buffer for new incoming connection
      //client_addr = calloc(1, sizeof(struct sockaddr_in));

      ret = pthread_detach(thread_sender);
      PTHREAD_ERROR_HELPER(ret, "Could not detach thread");
    }

    exit(EXIT_SUCCESS);
}
