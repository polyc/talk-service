#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <glib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <bits/sigaction.h>

#include "server.h"
#include "util.h"

sem_t user_list_mutex; // mutual exclusion to acces user list hash-table
sem_t mailbox_list_mutex; // mutual exclusion to acces mailbox hash-table
GHashTable* user_list;
GHashTable* mailbox_list;

static volatile int GLOBAL_EXIT = 0;
struct timeval timeout;

void intHandler(){
  fprintf(stdout, "\n\n<<<<< preparing to exit program>>>>>\n\n");
  GLOBAL_EXIT = 1;
}

//parse target username
char* parse_username(char* src, char* dest, char message_type){
  int i;
  int len = strlen(src);

  if(len > USERNAME_BUF_SIZE){
    return dest;
  }

  else if(message_type == CONNECTION_REQUEST){
    for (i = 1; i < len; i++) {
      dest[i - 1] = src[i];
    }
    dest[i-1] = '\0';
    return dest;
  }
  else if(message_type == CONNECTION_RESPONSE){
    for (i = 2; i < len; i++) {
      dest[i - 2] = src[i];
    }
    dest[i-2] = '\0';
    return dest;
  }
  ERROR_HELPER(-1, "[CONNECTION THREAD]: buffer inconsistency");
  //return dest;
}

int connection_accepted(char* response){
  if(response[1] == 'y')
    return 1;
  else
    return 0;
}

//receive username from client and check if it's already used by another connected client
int get_username(thread_args_t* args, usr_list_elem_t* new_element){
  char* send_buf = (char*)calloc(3, sizeof(char)); //buffer used to send response to client
  int inactivity_counter = 0;
  while(!GLOBAL_EXIT){

    int ret = recv_msg(args->socket, args->client_user_name, USERNAME_BUF_SIZE);

    if(ret == -2){//EAGAIN case
      if(inactivity_counter == MAX_GET_USERNAME_INACTIVITY){
        fprintf(stdout, "[CONNECTION THREAD]: client inactive, killing threads\n");
        free(send_buf);
        return -1;
      }
      else{
        inactivity_counter++;
        continue;
      }
    }

    if(ret == -1){//endpoint closed by client
      free(send_buf);
      return -1;
    }
    ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot receive username");

    ret = sem_wait(&user_list_mutex);
    ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot wait on user_list_mutex");

    //check if username is in user_list GHashTable
    if(CONTAINS(user_list, args->client_user_name) == FALSE &&
      strcmp(args->client_user_name, "")!= 0){

      //filling element struct with client data;
      new_element->client_ip = args->client_ip;
      new_element->a_flag = AVAILABLE;
      //inserting user into hash-table userlist
      ret = INSERT(user_list, (gpointer)args->client_user_name, (gpointer)new_element);
      fprintf(stdout, "[CONNECTION THREAD]: elemento inserito con successo\n");

      ret = sem_post(&user_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot post on user_list_mutex");

      //sending OK to client
      send_buf[0] = AVAILABLE;
      send_buf[1] = '\n';
      send_buf[2] = '\0';
      send_msg(args->socket, send_buf);
      free(send_buf);
      fprintf(stdout, "[CONNECTION THREAD]: username got\n");
      return 0;
    }

    else{
      ret = sem_post(&user_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot post on user_list_mutex");

      //sending OK to client
      send_buf[0] = UNAVAILABLE;
      send_buf[1] = '\n';
      send_buf[2] = '\0';
      send_msg(args->socket, send_buf);
      free(send_buf);
      memset(args->client_user_name, 0, USERNAME_BUF_SIZE);
    }
  }
  free(send_buf);
  return -1;
}

void update_availability(usr_list_elem_t* elem_to_update, char buf_command){
  int ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD][UPDATING AVAILABILITY]: cannot wait on user_list_mutex");

  elem_to_update->a_flag = buf_command; //update availability flag

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD][UPDATING AVAILABILITY]: cannot post on user_list_mutex");
}

//remove entries from hash tables when a client disconnects from server
void remove_entry(char* elem_to_remove, char* mailbox_to_remove){
  gboolean removed;

  //removing from mailboxlist
  int ret = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: could not wait on mailbox_list_mutex semaphore");
  removed = REMOVE(mailbox_list, mailbox_to_remove);
  ret = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: could not post on mailbox_list_mutex semaphore");

  //removing from userlist
  ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: could not wait on user_list_mutex_list semaphore");
  removed = REMOVE(user_list, elem_to_remove); //remove entry
  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: could not post on user_list_mutex_list semaphore");

  fprintf(stdout, "[CONNECTION THREAD]: successfully removed entry on disconnect operation\n");
}

//pushing message into sender thread personal GAsyncQueue
void push_entry(gpointer key, gpointer value, gpointer user_data){
  push_entry_args_t* args = (push_entry_args_t*)user_data;

  if(strcmp((char*)key, (char*)(args->sender_username))){//doesn't push to himself
    char* message = (char*)calloc(MSG_LEN, 1);
    strncpy(message, args->message, MSG_LEN);

    GAsyncQueue* ref_value = REF((GAsyncQueue*)value);
    PUSH(ref_value, (gpointer)message);
    UNREF(ref_value);
  }
}

//get target element from userlist
usr_list_elem_t* getTargetElement(char* target_buf){

  int ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on user_list_mutex");

  usr_list_elem_t* target = (usr_list_elem_t*)LOOKUP(user_list, (gconstpointer)target_buf);

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");

  return target;
}

//function called by FOR_EACH. It sends userlist on connection to receiver thread in client
void send_list_on_client_connection(gpointer key, gpointer value, gpointer user_data){

  char* buf = (char*)calloc(USERLIST_BUFF_SIZE, sizeof(char)); //user element string allocation

  serialize_user_element(buf, (usr_list_elem_t*)value, (char*)key, NEW);

  send_msg(*((int*)user_data), buf); //(socket, buf);
  free(buf);
  return;
}

//push a message to all clients connected to server
void push_all(push_entry_args_t* args){
  int err = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(err, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");

  //push message in each sender thread mailbox
  FOR_EACH(mailbox_list, (GHFunc)push_entry, (gpointer)args);

  //free of push_entry_args
  free(args->message);
  free(args);

  err = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(err, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");
}

//notify all clients
void notify(char* message_buf, char* element_username, char* mod_command, usr_list_elem_t* element_to_update){
  int ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "Cannot wait on user_list_mutex");

  if(element_to_update != NULL){
    ret = sem_post(&user_list_mutex);
    ERROR_HELPER(ret, "Cannot post on user_list_mutex");

    //generating message
    memset(message_buf, 0, MSG_LEN);
    serialize_user_element(message_buf, element_to_update, element_username, *mod_command);

    //arguments for push entry function
    push_entry_args_t* a = (push_entry_args_t*)malloc(sizeof(push_entry_args_t));
    a->message = (char*)calloc(MSG_LEN, 1);
    strncpy(a->message, message_buf, MSG_LEN);
    a->sender_username = element_username;

    //pushing updates to mailboxes
    push_all(a);
    }

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "Cannot post on user_list_mutex");
}

int execute_command(thread_args_t* args, char* message_buf, usr_list_elem_t* element_to_update, char* target_buf){

  int ret = 0;
  int size_buf;

  //parsing command
  char message_type = message_buf[0];
  char mod_command;


  //status variables
  GAsyncQueue* target_mailbox;
  GAsyncQueue** value;
  push_entry_args_t* p_args = NULL;

  switch(message_type){

    case CONNECTION_RESPONSE :
      parse_username(message_buf, target_buf, CONNECTION_RESPONSE);

      //---push connection response in target mailbox---

      ret = sem_wait(&mailbox_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");
      //search for target mailbox
      target_mailbox = NULL;
      value = &target_mailbox;
      ret = g_hash_table_lookup_extended(mailbox_list, (gconstpointer)target_buf, NULL, (gpointer*)value);
      ///////////////////////////////
      ret = sem_post(&mailbox_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");

      fprintf(stdout, "[CONNECTION THREAD]: MESSAGE = %s\n", message_buf);

      //these operations need to be hidden
      memset(message_buf+2, 0, strlen(message_buf)-2);
      strcpy(message_buf+2, args->client_user_name);
      size_buf = strlen(message_buf);
      message_buf[size_buf] = '\n';
      message_buf[size_buf+1] = '\0';

      p_args = (push_entry_args_t*)malloc(sizeof(push_entry_args_t));
      p_args->message = message_buf;
      p_args->sender_username = args->client_user_name;

      push_entry(target_buf, target_mailbox, p_args); //push connection response
      free(p_args);

      if(connection_accepted(message_buf)){
        update_availability(element_to_update, UNAVAILABLE);//set this client UNAVAILABLE
        mod_command = MODIFY;
        notify(message_buf, args->client_user_name, &mod_command, element_to_update); //alerts all connected clients
      }

      else{ //(connection not accepted) = reset target-response availability because before it was setted UNAVAILABLE
        usr_list_elem_t* target_element = getTargetElement(target_buf);
        update_availability(target_element, AVAILABLE);//set target-response client AVAILABLE
        mod_command = MODIFY;
        notify(message_buf, target_buf, &mod_command, target_element); //alerts all connected clients
      }

      return 0;

    //handle connection requests to other clients and check if they are available
    case CONNECTION_REQUEST :
      parse_username(message_buf, target_buf, CONNECTION_REQUEST);

      p_args = (push_entry_args_t*)malloc(sizeof(push_entry_args_t));

      //check if parsed username is connected to server
      ret = sem_wait(&user_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on user_list_mutex");

      usr_list_elem_t* target = (usr_list_elem_t*)LOOKUP(user_list, target_buf);

      //if true, send CONNECTION_REQUEST to target
      if(target != NULL && strcmp(target_buf, args->client_user_name) && target->a_flag == AVAILABLE){
        ret = sem_post(&user_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");

        mod_command = MODIFY;
        update_availability(element_to_update, UNAVAILABLE); //set this client UNAVAILABLE
        notify(message_buf, args->client_user_name, &mod_command, element_to_update);

        //send request to target client
        memset(message_buf, 0, MSG_LEN);
        message_buf[0] = CONNECTION_REQUEST;
        strcpy(message_buf + 1, args->client_user_name);
        size_buf = strlen(message_buf);
        message_buf[size_buf] = '\n';
        message_buf[size_buf+1] = '\0';

        //---push connection request in target mailbox---
        ret = sem_wait(&mailbox_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");

        //search for target mailbox
        target_mailbox = NULL;
        value = &target_mailbox;
        ret = g_hash_table_lookup_extended(mailbox_list, (gconstpointer)target_buf, NULL, (gpointer*)value);
        //////////////////////////

        p_args->message = message_buf;
        p_args->sender_username = args->client_user_name;
        push_entry(target_buf, target_mailbox, p_args);  //push connection request

        ret = sem_post(&mailbox_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");

        fprintf(stdout, "[CONNECTION THREAD]: MESSAGE = %s\n", message_buf);

        free(p_args);
        return 0;
      }

      else{//unavailable or not existent

        ret = sem_post(&user_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");

        message_buf[0] = CONNECTION_RESPONSE;
        message_buf[1] = 'n';
        message_buf[2] = '\n';
        message_buf[3] = '\0';

        ret = sem_wait(&mailbox_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");
        //search for target mailbox
        target_mailbox = NULL;
        value = &target_mailbox;
        ret = g_hash_table_lookup_extended(mailbox_list, (gconstpointer)args->client_user_name, NULL, (gpointer*)value);
        //////////////////////////

        p_args->message = message_buf;

        if (strcmp(target_buf, args->client_user_name) == 0) {
          //per evitare il blocco della notifica a se stesso
          p_args->sender_username = "-";
          push_entry(target_buf, target_mailbox, p_args);  //push connection request
        }
        else{
          p_args->sender_username = args->client_user_name;
          push_entry(target_buf, target_mailbox, p_args);  //push connection request
        }

        ret = sem_post(&mailbox_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");

        fprintf(stdout, "[CONNECTION THREAD]: MESSAGE = %s\n", message_buf);

        free(p_args);
        return 0;
      }

    case MESSAGE:
      ret = sem_wait(&mailbox_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");

      //search for target mailbox
      target_mailbox = NULL;
      value = &target_mailbox;
      ret = g_hash_table_lookup_extended(mailbox_list, (gconstpointer)target_buf, NULL, (gpointer*)value);

      ret = sem_post(&mailbox_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");

      fprintf(stdout, "[CONNECTION THREAD]: MESSAGE = %s\n", message_buf);

      //these operations need to be hidden
      size_buf = strlen(message_buf);
      message_buf[size_buf] = '\n';
      message_buf[size_buf+1] = '\0';

      p_args = (push_entry_args_t*)malloc(sizeof(push_entry_args_t));
      p_args->message = message_buf;
      p_args->sender_username = args->client_user_name;

      push_entry(target_buf, target_mailbox, p_args); //push chat message in target mailbox
      free(p_args);

      //check exit condition
      if(strcmp(message_buf + 1, EXIT) == 0){

        update_availability(element_to_update, AVAILABLE); //set this client available
        mod_command = MODIFY;
        notify(message_buf, args->client_user_name, &mod_command, element_to_update);

        usr_list_elem_t* target_element = getTargetElement(target_buf);
        update_availability(target_element, AVAILABLE); //set target client available
        mod_command = MODIFY;
        notify(message_buf, target_buf, &mod_command, target_element);

        printf("[CONNECTION THREAD]: exit chat\n");
      }

      return 0;

    case DISCONNECT:
      mod_command = DELETE;
      //update for other senders
      notify(message_buf, args->client_user_name, &mod_command, element_to_update);

      //check if client was chatting with someone before disconnection
      if(target_buf != NULL){
        int ret = sem_wait(&user_list_mutex);
        ERROR_HELPER(ret, "Cannot wait on user_list_mutex");

        usr_list_elem_t*  element = (usr_list_elem_t*)LOOKUP(user_list, target_buf);
        if (element!= NULL && element->a_flag == UNAVAILABLE) {
          update_availability(element, AVAILABLE);
        }

        ret = sem_post(&user_list_mutex);
        ERROR_HELPER(ret, "Cannot post on user_list_mutex");

        mod_command = MODIFY;
        notify(message_buf, target_buf, &mod_command, element);
      }


      //delete entry from hastables
      remove_entry(args->client_user_name, args->mailbox_key);

      fprintf(stdout, "[CONNECTION THREAD]: disconnect command processed\n");
      return -1;

    default :
      return ret;
  }

  return ret;
}

//transform a usr_list_elem_t in a string according to mod_command
void serialize_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command){
  fprintf(stdout, "[SERIALIZE]: sono dentro la funzione di serializzazione\n");
  buf_out[0] = mod_command;
  buf_out[1] = '\0';
  strncat(buf_out, "-", 1);
  strncat(buf_out, buf_username, USERNAME_BUF_SIZE);
  fprintf(stdout, "[SERIALIZE]: maremma maiala\n");

  if(mod_command == DELETE){
    strncat(buf_out ,"-\n", 2);
    return;
  }
  else if (mod_command == NEW){
    strncat(buf_out, "-", 2);
    strncat(buf_out, elem->client_ip, INET_ADDRSTRLEN);
    strncat(buf_out, "-", 2);

    int s = strlen(buf_out);
    buf_out[s] = elem->a_flag;
    buf_out[s+1] = '\0';

    strncat(buf_out, "-\n", 2);
    return;
  }

  else{
    strncat(buf_out, "-", 1);
    strncat(buf_out, elem->client_ip, INET_ADDRSTRLEN);
    strncat(buf_out, "-", 1);

    if(elem->a_flag == AVAILABLE){
      strncat(buf_out, "a", 1);
      strncat(buf_out, "-\n", 2);
      return;
    }
    else{
      strncat(buf_out, "u", 1);
      strncat(buf_out, "-\n", 2);
      return;
    }
  }
}

//client-process/server-threads communication routine
void* connection_handler(void* arg){
  thread_args_t* args = (thread_args_t*)arg;

  int ret, inactivity_counter = 0;

  char* mod_command = (char*)malloc(sizeof(char));

  //command receiver buffer
  char* message_buf = (char*)calloc(MSG_LEN, sizeof(char));

  char* target_useraname_buf = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

  usr_list_elem_t* element = (usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));

  //we enable SO_RCVTIMEO so that recv function will not be blocking
  ret = setsockopt(args->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  ERROR_HELPER(ret, "[CONENCTION THREAD] Cannot set SO_RCVTIMEO option");

  //get username while
  while (1) {
    ret = get_username(args, element);

    if (ret == 0) {//received username is available
      break;
    }

    if (ret == -2) {//received an unavailable username
      continue;
    }

    if(ret == -1){ //client closed endpoint or server killed

      fprintf(stdout, "[CONNECTION THREAD]: closed endpoint or server killed\n");

      //close operations
      free(target_useraname_buf);
      free(mod_command);
      free(message_buf);
      free(element);

      ret = close(args->socket);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot close socket");

      free(args->client_ip);
      free(args->client_user_name);
      free(args);

      pthread_exit(EXIT_SUCCESS);
    }
  }

  fprintf(stdout, "[CONNECTION THREAD]: %s\n", args->client_user_name);

  //SEMAPHORES TO SYNC WITH AND STOP SENDER THREAD
  sem_t* sender_sync = (sem_t*)malloc(sizeof(sem_t));
  ret = sem_init(sender_sync, 0, 0);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot init sender_sync sempahore");

  sem_t* sender_stop = (sem_t*)malloc(sizeof(sem_t));
  ret = sem_init(sender_stop, 0, 1);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot init sender_stop sempahore");

  //sender thread args
  //arguments allocation
  sender_thread_args_t* sender_args = (sender_thread_args_t*)malloc(sizeof(sender_thread_args_t));

  sender_args->sender_stop      = sender_stop;
  sender_args->sender_sync        = sender_sync;
  sender_args->client_ip          = args->client_ip;
  sender_args->mailbox_key        = args->client_user_name;

  fprintf(stderr, "[CONNECTION THREAD]: preparati argomenti per il sender thread\n");

  pthread_t thread_sender;
  ret = pthread_create(&thread_sender, NULL, sender_routine, (void*)sender_args);
  PTHREAD_ERROR_HELPER(ret, "[CONNECTION THREAD]: Could not create sender thread");

  fprintf(stderr, "[CONNECTION THREAD]: creato sender thread\n");

  //wait for sender thread init
  ret = sem_wait(sender_sync);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot wait on sender_sync");


  //notify connected clients new client arrival
  char* message = (char*)calloc(MSG_LEN, sizeof(char));
  mod_command[0] = NEW;
  notify(message, args->client_user_name, mod_command, element);
  free(message);

  //RECEIVING COMMANDS
  while(!GLOBAL_EXIT){
    int ret = recv_msg(args->socket, message_buf, MSG_LEN);

    //EAGAIN case
    if(ret == -2){
      if(inactivity_counter == MAX_INACTIVITY){
        ret = sem_wait(&user_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot wait on user_list_mutex");

        if(element->a_flag != UNAVAILABLE){ //se non è in chat
          ret = sem_post(&user_list_mutex);
          ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot post on user_list_mutex");

          fprintf(stdout, "[CONNECTION THREAD]: client inactive, killing threads\n");
          message_buf[0] = DISCONNECT;

          ret = execute_command(args, message_buf, element, target_useraname_buf);
          break;
        }
      }
      else{
          inactivity_counter++;
          continue;
      }
    }

    if(ret == -1){ //socket is cloesed by client
      fprintf(stdout, "[CONNECTION THREAD]: closed endpoint\n");
      message_buf[0] = DISCONNECT;

      ret = execute_command(args, message_buf, element, target_useraname_buf);
      break;
    }

    //PERFORM REQUESTED ACTIVITY
    inactivity_counter = 0; //reset counter because is going to be performed an activity
    ret = execute_command(args, message_buf, element, target_useraname_buf);
    //HANDLE CLIENT EXIT
    if (ret < 0){
      break; //exit condition
    }
  }

  //EXIT OPERATIONS

  //notify sender thread must stop
  ret = sem_wait(sender_stop);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot wait on sender_stop");


  free(target_useraname_buf);
  free(mod_command);
  free(message_buf);

  ret = pthread_join(thread_sender, NULL);

  ret = sem_destroy(sender_sync);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot destroy sender_sync semaphore");
  free(sender_sync);


  ret = sem_destroy(sender_stop);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot destroy sender_stop semaphore");
  free(sender_stop);

  ret = close(args->socket);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot close socket");

  free(args);

  pthread_exit(EXIT_SUCCESS);
}

//push notification communication routine
void* sender_routine(void* arg){
  int ret;

  sender_thread_args_t* args = (sender_thread_args_t*)arg;

  fprintf(stderr, "[SENDER THREAD]: inizializzazione indirizzo client receiver thread\n");

  struct sockaddr_in rec_addr = {0};
  rec_addr.sin_family         = AF_INET;
  rec_addr.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);
  inet_pton(AF_INET ,args->client_ip, &(rec_addr.sin_addr.s_addr));

  fprintf(stderr, "[SENDER THREAD]: indirizzo client thread receiver inizializzato con successo\n");

  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Cannot create sender thread socket");
  ret = connect(socket_desc, (struct sockaddr*) &rec_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error trying to connect to client receiver thread");

  fprintf(stderr, "[SENDER THREAD]: conneso al receiver thread\n");

  //mailbox init
  GAsyncQueue* mailbox_queue = mailbox_queue_init();

  GAsyncQueue* my_mailbox = REF(mailbox_queue);

  //inserting mailbox in hash-table mailbox list
  ret = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: could not wait on mailbox_list semaphore");
  INSERT(mailbox_list, (gpointer)(args->mailbox_key), (gpointer)(mailbox_queue));
  fprintf(stdout, "[SENDER THREAD]: inserted entry in mailbox_list\n");
  ret = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: Could not wait on mailbox_list semaphore");


  //unlock chandler thread
  ret = sem_post(args->sender_sync);
  ERROR_HELPER(ret, "[SENDER THREAD]:cannot post sender_sync sempahore");

  //sending list to client receiver thread
  ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on user_list_mutex");

  FOR_EACH(user_list, (GHFunc)send_list_on_client_connection, (gpointer)&socket_desc);

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on user_list_mutex");
  fprintf(stdout, "[SENDER THREAD]: sended list on first connection\n");

  char* message;
  int sem_value;
  //GET UPDATES FROM PERSONAL MAILBOX
  while(1){
    ret = sem_getvalue(args->sender_stop, &sem_value);
    if(!(ret == -1 && errno == EINVAL)){//semaforo distrutto da cHandler
      ERROR_HELPER(ret, "[SENDER THREAD]:cannot get value of chandler_sync sempahore");
    }
    //check termination condition
    if (sem_value == 0) {
      fprintf(stdout, "[SENDER THREAD]: terminating sender routine\n");
      break;
    }

    if(GLOBAL_EXIT){
      fprintf(stdout, "[SENDER THREAD]: terminating sender routine\n");
      break;
    }

    message = (char*)POP(my_mailbox, POP_TIMEOUT);

    if(message == NULL)continue;

    //sending message to client's receiver thread
    send_msg(socket_desc, message);
    fprintf(stdout, "MESSAGGIO: %s\n", message);
    fprintf(stdout, "[SENDER THREAD]: message sended to client's reciever thread\n");
    free(message);
  }
  //exit operations
  UNREF(my_mailbox);
  UNREF(mailbox_queue);

  ret = close(socket_desc);
  ERROR_HELPER(ret, "Error closing socket_desc in sender routine");

  fprintf(stdout, "[SENDER THREAD]: routine exit point\n");
  free(args);


  pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char const *argv[]) {
  int ret, server_desc, client_desc;
  struct sigaction actSIGINT, actSIGPIPE, actSIGHUP, actSIGTERM;

  memset(&actSIGINT, 0, sizeof(struct sigaction));
  memset(&actSIGPIPE, 0, sizeof(struct sigaction));
  memset(&actSIGHUP, 0, sizeof(struct sigaction));
  memset(&actSIGTERM, 0, sizeof(struct sigaction));

  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  sigset_t sa_mask;

  ret = sigfillset(&sa_mask);

  actSIGINT.sa_handler = intHandler;
  actSIGINT.sa_mask = sa_mask;
  ret = sigaction(SIGINT, &actSIGINT, NULL);
  ERROR_HELPER(ret, "[MAIN]: Error in sigaction function");

  actSIGPIPE.sa_handler = intHandler;
  actSIGPIPE.sa_mask = sa_mask;
  ret = sigaction(SIGPIPE, &actSIGPIPE, NULL);
  ERROR_HELPER(ret, "[MAIN]: Error in sigaction function");

  actSIGHUP.sa_handler = intHandler;
  actSIGHUP.sa_mask = sa_mask;
  ret = sigaction(SIGHUP, &actSIGHUP, NULL);
  ERROR_HELPER(ret, "[MAIN]: Error in sigaction function");

  actSIGTERM.sa_handler = intHandler;
  actSIGTERM.sa_mask = sa_mask;
  ret = sigaction(SIGTERM, &actSIGTERM, NULL);
  ERROR_HELPER(ret, "[MAIN]: Error in sigaction function");

  //init thread queue
  GAsyncQueue* thread_queue = thread_queue_init();

  //init addresses queue
  GAsyncQueue* addresses_queue = addresses_queue_init();

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

  struct sockaddr_in server_addr = {0};
  int sockaddr_len = sizeof(struct sockaddr_in);

  // initialize socket for listening
  server_desc = socket(AF_INET , SOCK_STREAM , 0);
  ERROR_HELPER(server_desc, "[MAIN]: Could not create socket");

  server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

  //we enable SO_REUSEADDR to quickly restart our server after a crash
  int reuseaddr_opt = 1;
  ret = setsockopt(server_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "[MAIN]: Cannot set SO_REUSEADDR option");

  //we enable SO_RCVTIMEO so that recv function will not be blocking
  ret = setsockopt(server_desc, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot set SO_RCVTIMEO option");

  // bind address to socket
  ret = bind(server_desc, (struct sockaddr*) &server_addr, sockaddr_len);
  ERROR_HELPER(ret, "[MAIN]:Cannot bind address to socket");

  // start listening
  ret = listen(server_desc, MAX_CONN_QUEUE);
  ERROR_HELPER(ret, "[MAIN]:Cannot listen on socket");

  // we allocate client_addr dynamically and initialize it to zero
  struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

  fprintf(stderr, "[MAIN]: fine inizializzazione, entro nel while di accettazione\n");

  // loop to manage incoming connections spawning handler threads
    while (!GLOBAL_EXIT) {
      client_desc = accept(server_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
      if (client_desc == -1 &&(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue; // check for interruption by signals
      ERROR_HELPER(client_desc, "[MAIN]:cannot open socket for incoming connection");

      fprintf(stderr, "[MAIN]: connessione accettata\n");

      PUSH(addresses_queue, client_addr);


      //thread spawning
      //*********************

      //client handler thread
      //arguments allocation
      char* client_user_name = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
      char* client_ip = (char*)malloc(INET_ADDRSTRLEN*sizeof(char));

      //parsing client ip in dotted form
      inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);

      thread_args_t* thread_args = (thread_args_t*)malloc(sizeof(thread_args_t));
      thread_args->socket               = client_desc;
      thread_args->client_user_name     = client_user_name;
      thread_args->client_ip            = client_ip;
      thread_args->mailbox_key          = client_user_name;

      //connection handler thread spawning
      pthread_t thread_client; //= (pthread_t*)malloc(sizeof(pthread_t));
      ret = pthread_create(&thread_client, NULL, connection_handler, (void*)thread_args);
      PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");
      PUSH(thread_queue, &thread_client);

      fprintf(stderr, "[MAIN]: creato connection thread\n");


      //new buffer for incoming connection
      client_addr = calloc(1, sizeof(struct sockaddr_in));

      fprintf(stderr, "[MAIN]: fine loop di accettazione connessione in ingresso, inizio nuova iterazione\n");
    }

    //close operations

    //free threads
    pthread_t*  t;
    while((t = POP(thread_queue, POP_TIMEOUT)) != NULL){
      ret = pthread_join(*t, NULL);
    }

    //free clients addreses
    struct sockaddr_in* addr;
    while((addr = POP(addresses_queue, POP_TIMEOUT)) != NULL){
      free(addr);
    }

    free(client_addr);
    UNREF(addresses_queue);
    UNREF(thread_queue);
    DESTROY(user_list);
    DESTROY(mailbox_list);

    ret = sem_destroy(&user_list_mutex);
    ERROR_HELPER(ret, "[MAIN][ERROR]: cannot destroy user_list_mutex semaphore");

    ret = sem_destroy(&mailbox_list_mutex);
    ERROR_HELPER(ret, "[MAIN][ERROR]: cannot destroy mailbox_list_mutex semaphore");

    ret = close(server_desc);
    ERROR_HELPER(ret, "[MAIN]: cannot close server socket");

    exit(EXIT_SUCCESS);
}
