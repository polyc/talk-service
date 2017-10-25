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
GHashTable* user_list; //userlist hash-table
GHashTable* mailbox_list; //mailbox_list hash-table

static volatile int GLOBAL_EXIT = 0;
struct timeval timeout; //timeout for sockets

void _initSignals(){

  struct sigaction actSIGINT, actSIGPIPE, actSIGHUP, actSIGTERM;
  sigset_t sa_mask;

  memset(&actSIGINT, 0, sizeof(struct sigaction));
  memset(&actSIGPIPE, 0, sizeof(struct sigaction));
  memset(&actSIGHUP, 0, sizeof(struct sigaction));
  memset(&actSIGTERM, 0, sizeof(struct sigaction));

  int ret = sigfillset(&sa_mask);

  //SIGINT
  actSIGINT.sa_handler = intHandler;
  actSIGINT.sa_mask = sa_mask;
  ret = sigaction(SIGINT, &actSIGINT, NULL);
  ERROR_HELPER(ret, "[MAIN]: Error in sigaction function");

  //SIGPIPE
  actSIGPIPE.sa_handler = SIG_IGN;
  actSIGPIPE.sa_mask = sa_mask;
  ret = sigaction(SIGPIPE, &actSIGPIPE, NULL);
  ERROR_HELPER(ret, "[MAIN]: Error in sigaction function");

  //SIGHUP
  actSIGHUP.sa_handler = intHandler;
  actSIGHUP.sa_mask = sa_mask;
  ret = sigaction(SIGHUP, &actSIGHUP, NULL);
  ERROR_HELPER(ret, "[MAIN]: Error in sigaction function");

  //SIGTERM
  actSIGTERM.sa_handler = intHandler;
  actSIGTERM.sa_mask = sa_mask;
  ret = sigaction(SIGTERM, &actSIGTERM, NULL);
  ERROR_HELPER(ret, "[MAIN]: Error in sigaction function");
}

void _initMainSemaphores(){

  //init user_list_mutex
  int ret = sem_init(&user_list_mutex, 0, 1);
  ERROR_HELPER(ret, "[FATAL ERROR] Could not init user_list_mutex semaphore");

  //init mailbox_list_mutex
  ret = sem_init(&mailbox_list_mutex, 0, 1);
  ERROR_HELPER(ret, "[FATAL ERROR] Could not init mailbox_list_mutex semaphore");
}

void intHandler(int sig){
  if(sig == SIGPIPE){
    fprintf(stdout, "CATCHED SIGPIPE\n");
    return;
  }
  else{
    fprintf(stdout, "\n\n<<<<< preparing to exit program>>>>>\n\n");
    GLOBAL_EXIT = 1;
  }
}

//parse target username from a connection request/response string
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

    //EAGAIN case
    if(ret == -2){
      if(inactivity_counter == MAX_GET_USERNAME_INACTIVITY){
        fprintf(stdout, "[CONNECTION THREAD]: client inactive, killing threads\n");
        free(send_buf);
        return GENERIC_THREAD_TERM;
      }
      else{
        inactivity_counter++;
        continue;
      }
    }

    //ENDPOINT CLOSED BY CLIENT
    if(ret == -1){
      free(send_buf);
      return GENERIC_THREAD_TERM;
    }
    ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot receive username");


    ret = sem_wait(&user_list_mutex);
    ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot wait on user_list_mutex");
    //-----------------------------CRITICAL SECTION-----------------------------

    //------------------------------CHECKS--------------------------------------
    if(CONTAINS(user_list, args->client_user_name) == FALSE &&
      strcmp(args->client_user_name, "")!= 0){

      //---------------------------USERLIST INSERTION---------------------------
      //filling element struct with client data;
      new_element->client_ip = args->client_ip;
      new_element->a_flag = AVAILABLE;
      //inserting user into hash-table userlist
      ret = INSERT(user_list, (gpointer)args->client_user_name, (gpointer)new_element);
      fprintf(stdout, "[CONNECTION THREAD]: element in serted\n");

      ret = sem_post(&user_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot post on user_list_mutex");
      //-------------------------END OF CRITICAL SECTION------------------------

      //sending YES to client
      send_buf[0] = AVAILABLE;
      send_buf[1] = '\n';
      send_buf[2] = '\0';
      fprintf(stdout, "[CONNECTION THREAD]: username got\n");
    }

    else{
      ret = sem_post(&user_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot post on user_list_mutex");
      //-------------------------END OF CRITICAL SECTION------------------------

      //sending NO to client
      send_buf[0] = UNAVAILABLE;
      send_buf[1] = '\n';
      send_buf[2] = '\0';
      memset(args->client_user_name, 0, USERNAME_BUF_SIZE); //reset username
    }

    //-------------------------SEND REPLY TO CLIENT-----------------------------
    ret = send_msg(args->socket, send_buf);
    //SIGPIPE
    if(ret == -1){
      return GENERIC_THREAD_TERM; //TERM THREAD, NOT SERVER
    }
    //NETWORK DOWN
    if(ret == -2){
      GLOBAL_EXIT = 1;
      continue;
    }
    ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot send message on socket");

    //-------------------------SETTING RETURN TYPE------------------------------
    if(send_buf[0] == AVAILABLE){
      free(send_buf);
      return 0; //OK
    }
    else{
      free(send_buf);
      return TRY_AGAIN; //INCORRECT USERNAME
    }

  }

  free(send_buf);
  return GENERIC_THREAD_TERM;
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


//push a message into sender thread personal GAsyncQueue
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

  int ret = send_msg( *(((send_list_args_t*)user_data)->socket), buf);
  //SIGPIPE
  if(ret == -1){
    *(((send_list_args_t*)user_data)->threads_term) = 1; //TERM PAIR OF THREADS
    free(buf);
    return;
  }
  //NETWORK DOWN
  if(ret == -2){
    GLOBAL_EXIT = 1;
    free(buf);
    return;
  }
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot send message on socket");
  free(buf);
  return;
}


//push a message to all clients connected to server
void push_all(push_entry_args_t* args){
  int ret = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");

  //push message in each sender thread mailbox
  FOR_EACH(mailbox_list, (GHFunc)push_entry, (gpointer)args);

  //free push_entry_args
  free(args->message);
  free(args);

  ret = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");
}


//notify all clients, using push_all
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


//parse a command received from client and execute it
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
      //moving username response target in taget_buf
      parse_username(message_buf, target_buf, CONNECTION_RESPONSE);

      //-------------PUSH CONNECTION_RESPONSE IN TEARGET MAILBOX----------------

      ret = sem_wait(&mailbox_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");

      //-----------------------SEARCH FOR TARGET MAILBOX------------------------
      target_mailbox = NULL;
      value = &target_mailbox;
      ret = g_hash_table_lookup_extended(mailbox_list, (gconstpointer)target_buf, NULL, (gpointer*)value);

      fprintf(stdout, "[CONNECTION THREAD]: MESSAGE = %s\n", message_buf);

      //SWAPPING USERNAME, SO TARGET CLIENT WILL KNOW WHO SENDED THE RESPONSE
      memset(message_buf+2, 0, strlen(message_buf)-2);
      strcpy(message_buf+2, args->client_user_name);

      size_buf = strlen(message_buf);
      message_buf[size_buf]   = '\n';
      message_buf[size_buf+1] = '\0';

      //push_entry_args preparation
      p_args = (push_entry_args_t*)malloc(sizeof(push_entry_args_t));
      p_args->message = message_buf;
      p_args->sender_username = args->client_user_name;

      //PUSH CONNECTION RESPONSE
      push_entry(target_buf, target_mailbox, p_args);

      ret = sem_post(&mailbox_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");

      free(p_args);

      //-------------------USERLIST INTEGRITY OPERATIONS------------------------

      if(connection_accepted(message_buf)){
        update_availability(element_to_update, UNAVAILABLE);//set this client UNAVAILABLE
        mod_command = MODIFY;
        //notify all connected clients
        notify(message_buf, args->client_user_name, &mod_command, element_to_update);
      }

      else{ //!connection_accepted()
        //reset target-response availability because before it was setted UNAVAILABLE
        usr_list_elem_t* target_element = getTargetElement(target_buf);
        update_availability(target_element, AVAILABLE);//set target-response client AVAILABLE
        mod_command = MODIFY;
        //notify all connected clients
        notify(message_buf, target_buf, &mod_command, target_element);
      }

      return 0;



    //HANDLE CONNECTION_REQUEST TO OTHER CLIENTS
    case CONNECTION_REQUEST :
      //moving username request target in taget_buf
      parse_username(message_buf, target_buf, CONNECTION_REQUEST);

      //push_entry_args allocation
      p_args = (push_entry_args_t*)malloc(sizeof(push_entry_args_t));


      //--------------------------------CHECK----------------------------------

      ret = sem_wait(&user_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on user_list_mutex");

      usr_list_elem_t* target_element = getTargetElement(target_buf);


      if(target_element != NULL && strcmp(target_buf, args->client_user_name)
        && target_element->a_flag == AVAILABLE){

        ret = sem_post(&user_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");

        //--------------TRUE, send CONNECTION_REQUEST to target-----------------

        //--------------------SET THIS CLIENT UNAVAILABLE-----------------------
        mod_command = MODIFY;
        update_availability(element_to_update, UNAVAILABLE);
        notify(message_buf, args->client_user_name, &mod_command, element_to_update);

        //SWAPPING USERNAME, SO TARGET CLIENT WILL KNOW WHO SENDED THE REQUEST
        message_buf[0] = CONNECTION_REQUEST;
        memset(message_buf + 1, 0, strlen(message_buf)-1);
        strcpy(message_buf + 1, args->client_user_name);

        size_buf = strlen(message_buf);
        message_buf[size_buf] = '\n';
        message_buf[size_buf+1] = '\0';

        //-----------------------SEARCH FOR TARGET MAILBOX----------------------
        ret = sem_wait(&mailbox_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");
        target_mailbox = NULL;
        value = &target_mailbox;
        ret = g_hash_table_lookup_extended(mailbox_list, (gconstpointer)target_buf, NULL, (gpointer*)value);

        //--------------PUSH CONNECTION_REQUEST IN TARGET MAILBOX----------------

        //push_entry_args preparation
        p_args->message = message_buf;
        p_args->sender_username = args->client_user_name;

        //PUSH CONNECTION_REQUEST
        push_entry(target_buf, target_mailbox, p_args);

        ret = sem_post(&mailbox_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");

        fprintf(stdout, "[CONNECTION THREAD]: MESSAGE = %s\n", message_buf);

        free(p_args);
        return 0;
      }


      else{
        ret = sem_post(&user_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on user_list_mutex");

        //-----FALSE, send a negative CONNECTION_RESPONSE to this client--------

        message_buf[0] = CONNECTION_RESPONSE;
        message_buf[1] = 'n';
        message_buf[2] = '\n';
        message_buf[3] = '\0';


        //-----------------------SEARCH FOR TARGET MAILBOX----------------------
        ret = sem_wait(&mailbox_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");
        target_mailbox = NULL;
        value = &target_mailbox;
        ret = g_hash_table_lookup_extended(mailbox_list, (gconstpointer)args->client_user_name, NULL, (gpointer*)value);

        //push_entry_args preparation
        p_args->message = message_buf;

        if (strcmp(target_buf, args->client_user_name) == 0) {
          //per evitare il blocco della notifica a se stesso
          p_args->sender_username = "-";
        }
        else{
          p_args->sender_username = args->client_user_name;
        }

        //PUSH NEGATIVE CONNECTION_RESPONSE
        push_entry(target_buf, target_mailbox, p_args);

        ret = sem_post(&mailbox_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");

        fprintf(stdout, "[CONNECTION THREAD]: MESSAGE = %s\n", message_buf);

        free(p_args);
        return 0;
      }



    case MESSAGE:
      
      //-----------------------SEARCH FOR TARGET MAILBOX----------------------
      ret = sem_wait(&mailbox_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot wait on mailbox_list_mutex");
      target_mailbox = NULL;
      value = &target_mailbox;
      ret = g_hash_table_lookup_extended(mailbox_list, (gconstpointer)target_buf, NULL, (gpointer*)value);

      ret = sem_post(&mailbox_list_mutex);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot post on mailbox_list_mutex");

      fprintf(stdout, "[CONNECTION THREAD]: MESSAGE = %s\n", message_buf);
      
      //------------------------------PUSH MESSAGE----------------------------
      size_buf = strlen(message_buf);
      message_buf[size_buf] = '\n';
      message_buf[size_buf+1] = '\0';
      
      //push_entry_args preparation
      p_args = (push_entry_args_t*)malloc(sizeof(push_entry_args_t));
      p_args->message = message_buf;
      p_args->sender_username = args->client_user_name;

      push_entry(target_buf, target_mailbox, p_args); //push chat message in target mailbox
      free(p_args);

      //-----------------------CHECK EXIT CHAT CONDITION----------------------
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
          update_availability(element, AVAILABLE); //set target available
        }

        ret = sem_post(&user_list_mutex);
        ERROR_HELPER(ret, "Cannot post on user_list_mutex");

        mod_command = MODIFY;
        notify(message_buf, target_buf, &mod_command, element);
      }


      //delete entry from hastables
      remove_entry(args->client_user_name, args->mailbox_key);

      fprintf(stdout, "[CONNECTION THREAD]: disconnect command processed\n");
      return GENERIC_THREAD_TERM;

    default :
      return ret;
  }

  return ret;
}


//transform a usr_list_elem_t in a string according to mod_command
void serialize_user_element(char* buf_out, usr_list_elem_t* elem, char* buf_username, char mod_command){
  fprintf(stdout, "[SERIALIZE]: serializing user element\n");
  buf_out[0] = mod_command;
  buf_out[1] = '\0';
  strncat(buf_out, "-", 1);
  strncat(buf_out, buf_username, USERNAME_BUF_SIZE);

  if(mod_command == DELETE){
    strncat(buf_out ,"-\n", 2);
    fprintf(stdout, "[SERIALIZE]: end serialize\n");
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
    fprintf(stdout, "[SERIALIZE]: end serialize\n");
    return;
  }

  else{
    strncat(buf_out, "-", 1);
    strncat(buf_out, elem->client_ip, INET_ADDRSTRLEN);
    strncat(buf_out, "-", 1);

    if(elem->a_flag == AVAILABLE){
      strncat(buf_out, "a", 1);
      strncat(buf_out, "-\n", 2);
      fprintf(stdout, "[SERIALIZE]: end serialize\n");
      return;
    }
    else{
      strncat(buf_out, "u", 1);
      strncat(buf_out, "-\n", 2);
      fprintf(stdout, "[SERIALIZE]: end serialize\n");
      return;
    }
  }
}


//client-processes/server-threads communication routine
void* connection_handler(void* arg){
  thread_args_t* args = (thread_args_t*)arg;

  int ret;
  int inactivity_counter = 0;
  static volatile int threads_term = 0; //is 1 when a send_msg in sender thread return -1 because of EPIPE

  char* mod_command = (char*)malloc(sizeof(char));

  //command receiver buffer
  char* message_buf = (char*)calloc(MSG_LEN, sizeof(char));

  char* target_useraname_buf = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

  //alloc an empty userlist element ready to be passed to get_username()
  usr_list_elem_t* element = (usr_list_elem_t*)malloc(sizeof(usr_list_elem_t));

  //-------------------------------GET USERNAME---------------------------------

  while (!GLOBAL_EXIT) {
    ret = get_username(args, element);

    if (ret == 0) {//received username is available
      break;
    }

    if (ret == TRY_AGAIN) {//received an unavailable username
      continue;
    }

    if(ret == GENERIC_THREAD_TERM){ //client closed endpoint || server killed || sigpipe on send_msg

      fprintf(stdout, "[CONNECTION THREAD]: closed endpoint or server killed\n");

      //close operations
      free(target_useraname_buf);
      fprintf(stdout,"[CONNECTION THREAD]: free target_useraname_buf\n");

      free(mod_command);
      fprintf(stdout,"[CONNECTION THREAD]: free mod_command\n");

      free(message_buf);
      fprintf(stdout,"[CONNECTION THREAD]: message_buf\n");

      free(element);
      fprintf(stdout,"[CONNECTION THREAD]: free element\n");

      ret = close(args->socket);
      ERROR_HELPER(ret, "[CONNECTION THREAD]: cannot close socket");

      free(args->client_ip);
      free(args->client_user_name);
      free(args);
      fprintf(stdout,"[CONNECTION THREAD]: free args\n");

      pthread_exit(EXIT_SUCCESS);
    }
  }

  fprintf(stdout, "[CONNECTION THREAD]: %s\n", args->client_user_name);


  //-------------INNIT SEMAPHORES TO SYNC WITH AND STOP SENDER THREAD-----------
  sem_t* sender_sync = (sem_t*)malloc(sizeof(sem_t));
  ret = sem_init(sender_sync, 0, 0);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot init sender_sync sempahore");

  sem_t* sender_stop = (sem_t*)malloc(sizeof(sem_t));
  ret = sem_init(sender_stop, 0, 1);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot init sender_stop sempahore");


  //-----------------------------SENDER THREAD ARGS-----------------------------
  sender_thread_args_t* sender_args = (sender_thread_args_t*)malloc(sizeof(sender_thread_args_t));

  sender_args->sender_stop        = sender_stop;
  sender_args->sender_sync        = sender_sync;
  sender_args->threads_term       = &threads_term;
  sender_args->client_ip          = args->client_ip;
  sender_args->mailbox_key        = args->client_user_name;
  fprintf(stderr, "[CONNECTION THREAD]: sender thread's args ready\n");

  //----------------------------SENDER THREAD SPAWN-----------------------------
  pthread_t thread_sender;
  ret = pthread_create(&thread_sender, NULL, sender_routine, (void*)sender_args);
  PTHREAD_ERROR_HELPER(ret, "[CONNECTION THREAD]: Could not create sender thread");

  fprintf(stderr, "[CONNECTION THREAD]: creato sender thread\n");

  //WAIT FOR SENDER THREAD INIT
  ret = sem_wait(sender_sync);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot wait on sender_sync");


  //NOTIFY THAT A NEW CLIENT IS ONLINE
  char* message = (char*)calloc(MSG_LEN, sizeof(char));
  mod_command[0] = NEW;
  notify(message, args->client_user_name, mod_command, element);
  free(message);


  //----------------------------RECEIVING COMMANDS------------------------------
  while(!GLOBAL_EXIT){

    //sigpipe in sender threads ---->>> terminates this pair of threads gracefully
    //by breaking activity cicle
    if(threads_term){
      break;
    }

    //receive a command from client
    int ret = recv_msg(args->socket, message_buf, MSG_LEN);

    //---------------------------RETURN HANDLING--------------------------------
    //EAGAIN case
    if(ret == -2){
      if(inactivity_counter >= MAX_INACTIVITY){
        ret = sem_wait(&user_list_mutex);
        ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot wait on user_list_mutex");

        if(element->a_flag != UNAVAILABLE){ //se non è in chat uccide il client
          ret = sem_post(&user_list_mutex);
          ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot post on user_list_mutex");

          fprintf(stdout, "[CONNECTION THREAD]: client inactive, killing threads\n");
          message_buf[0] = DISCONNECT;
          //remove client entry from hash-table e notify others
          ret = execute_command(args, message_buf, element, target_useraname_buf);
          break; //term threads
        }
      }
      else{
          inactivity_counter++;
          continue;
      }
    }

    //ENDPOINT CLOSED BY CLIENT
    if(ret == -1){
      fprintf(stdout, "[CONNECTION THREAD]: closed endpoint\n");
      message_buf[0] = DISCONNECT;
      //remove client entry from hash-table e notify others
      ret = execute_command(args, message_buf, element, target_useraname_buf);
      break; //term threads
    }
    //----------------------END OF RECEIVE RETURN HANDLING----------------------


    //------------------------PERFORM REQUESTED ACTIVITY------------------------
    inactivity_counter = 0; //reset counter because is going to be performed an activity
    ret = execute_command(args, message_buf, element, target_useraname_buf);

    //HANDLE CLIENT EXIT
    if (ret == GENERIC_THREAD_TERM){
      break; //term threads
    }
  }

  //-------------------------------EXIT OPERATIONS------------------------------

  //notify sender thread it must stop
  ret = sem_wait(sender_stop);
  ERROR_HELPER(ret, "[CONNECTION THREAD]:cannot wait on sender_stop");


  free(target_useraname_buf);
  fprintf(stdout,"[CONNECTION THREAD]: free target_useraname_buf\n");

  free(mod_command);
  fprintf(stdout,"[CONNECTION THREAD]: free mod_command\n");

  free(message_buf);
  fprintf(stdout,"[CONNECTION THREAD]: message_buf\n");

  fprintf(stdout,"[CONNECTION THREAD]: joining sender thread\n");
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
  fprintf(stdout,"[CONNECTION THREAD]: free args\n");

  pthread_exit(EXIT_SUCCESS);
}


//push notification communication routine
void* sender_routine(void* arg){
  int ret;

  sender_thread_args_t* args = (sender_thread_args_t*)arg;

  //---------------------------SOCKET INSTALLATION------------------------------
  fprintf(stderr, "[SENDER THREAD]: inizializzazione indirizzo client receiver thread\n");

  struct sockaddr_in rec_addr = {0};
  rec_addr.sin_family         = AF_INET;
  rec_addr.sin_port           = htons(CLIENT_THREAD_RECEIVER_PORT);
  inet_pton(AF_INET ,args->client_ip, &(rec_addr.sin_addr.s_addr));

  fprintf(stderr, "[SENDER THREAD]: indirizzo client thread receiver inizializzato con successo\n");

  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Cannot create sender thread socket");

  ret = connect(socket_desc, (struct sockaddr*) &rec_addr, sizeof(struct sockaddr_in));
  if(ret == -1){
    //stop threads
    if(errno == ECONNREFUSED){
      *(args->threads_term) = 1;
    }
    else if(errno == ENETDOWN){
      GLOBAL_EXIT = 1;
    }
    else{
      ERROR_HELPER(ret, "Error trying to connect to client receiver thread");
    }
  }

  fprintf(stderr, "[SENDER THREAD]: conneso al receiver thread\n");


  //------------------------------MAILBOX INIT----------------------------------
  GAsyncQueue* mailbox_queue = mailbox_queue_init();

  GAsyncQueue* my_mailbox = REF(mailbox_queue);

  //inserting mailbox in hash-table mailbox list
  ret = sem_wait(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: could not wait on mailbox_list semaphore");

  INSERT(mailbox_list, (gpointer)(args->mailbox_key), (gpointer)(mailbox_queue));
  fprintf(stdout, "[SENDER THREAD]: inserted entry in mailbox_list\n");

  ret = sem_post(&mailbox_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: Could not wait on mailbox_list semaphore");


  //unlock handler thread
  ret = sem_post(args->sender_sync);
  ERROR_HELPER(ret, "[SENDER THREAD]:cannot post sender_sync sempahore");


  //--------------------SEND LIST ON CLIENT CONNECTION---------------------

  //send_list_on_client_connection args
  send_list_args_t* s_args = (send_list_args_t*)malloc(sizeof(send_list_args_t));
  s_args->socket = &socket_desc;
  s_args->threads_term = args->threads_term;

  //sending list to client receiver thread
  ret = sem_wait(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on user_list_mutex");

  FOR_EACH(user_list, (GHFunc)send_list_on_client_connection, (gpointer)s_args);

  ret = sem_post(&user_list_mutex);
  ERROR_HELPER(ret, "[SENDER THREAD]: cannot wait on user_list_mutex");
  fprintf(stdout, "[SENDER THREAD]: sended list on first connection\n");

  free(s_args);


  //--------------------------GET UPDATES FROM MAILBOX--------------------------
  char* message;
  int sem_value;

  while(1){
    ret = sem_getvalue(args->sender_stop, &sem_value);
    if(!(ret == -1 && errno == EINVAL)){//destroied semaphore
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

    //GET MESSAGE
    message = (char*)POP(my_mailbox, POP_TIMEOUT);
    if(message == NULL)continue;

    //send message to client's receiver thread
    ret = send_msg(socket_desc, message);

    //SIGPIPE
    if(ret == -1){
      *(args->threads_term) = 1; //notify handler thread
    }
    //NETWORK DOWN
    else if(ret == -2){
      GLOBAL_EXIT = 1;
    }
    ERROR_HELPER(ret, "[SENDER THREAD]: cannot send message on socket");

    fprintf(stdout, "[SENDER THREAD]: sended message = %s\n", message);
    fprintf(stdout, "[SENDER THREAD]: message sended to client's reciever thread\n");
    free(message);
  }

  //-----------------------------CLOSE OPERATIONS-------------------------------
  UNREF(my_mailbox);
  UNREF(mailbox_queue);
  fprintf(stdout,"[SENDER THREAD]: UNREF mailbox_queue\n");

  ret = close(socket_desc);
  ERROR_HELPER(ret, "Error closing socket_desc in sender routine");

  fprintf(stdout, "[SENDER THREAD]: routine exit point\n");
  free(args);


  pthread_exit(EXIT_SUCCESS);
}


int main(int argc, char const *argv[]) {
  int ret, server_desc, client_desc;

  //set handler for common signals
  _initSignals();

  //set timeout for sockets
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  //init thread queue
  GAsyncQueue* thread_queue = thread_queue_init();

  //init addresses queue
  GAsyncQueue* addresses_queue = addresses_queue_init();

  //init server userlist
  user_list = usr_list_init();

  //init mailbox_list
  mailbox_list = mailbox_list_init();

  _initMainSemaphores();

  struct sockaddr_in server_addr = {0};
  int sockaddr_len = sizeof(struct sockaddr_in);

  // initialize socket for listening
  server_desc = socket(AF_INET , SOCK_STREAM , 0);
  ERROR_HELPER(server_desc, "[MAIN]: Could not create socket");

  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(SERVER_PORT);

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

  fprintf(stderr, "[MAIN]: INIT DONE\n");


  /*------------------------------------------------MAIN LOOP------------------------------------------------*/
  // loop to manage incoming connections spawning handler threads
    while (!GLOBAL_EXIT) {
      client_desc = accept(server_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);

      // check for interruption by signals
      if (client_desc == -1 &&(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
      ERROR_HELPER(client_desc, "[MAIN]:cannot open socket for incoming connection");

      fprintf(stderr, "[MAIN]: accepted connection\n");

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
    /*--------------------------------------------END OF MAIN LOOP-------------------------------------------*/


    //CLOSE OPERATIONS

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
    fprintf(stdout,"UNREF addresses_queue\n");

    UNREF(thread_queue);
    fprintf(stdout,"UNREF thread_queue\n");

    DESTROY(user_list);
    fprintf(stdout,"DESTROY user_list\n");

    DESTROY(mailbox_list);
    fprintf(stdout,"DESTROY mailbox_list\n");

    ret = sem_destroy(&user_list_mutex);
    ERROR_HELPER(ret, "[MAIN][ERROR]: cannot destroy user_list_mutex semaphore");

    ret = sem_destroy(&mailbox_list_mutex);
    ERROR_HELPER(ret, "[MAIN][ERROR]: cannot destroy mailbox_list_mutex semaphore");

    ret = close(server_desc);
    ERROR_HELPER(ret, "[MAIN]: cannot close server socket");

    exit(EXIT_SUCCESS);
}
