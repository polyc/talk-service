#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <semaphore.h>
#include <glib.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <signal.h>
#include <bits/sigaction.h>

#include "client.h"
#include "common.h"
#include "util.h"

sem_t sync_receiver;
sem_t sync_userList;
sem_t wait_response;
GHashTable* user_list;

char* USERNAME;          //buffer contenente lo username del client
char* USERNAME_CHAT;     //buffer contenente lo useranem del client con cui sto chattando
char* USERNAME_REQUEST;  //buffer contenente lo username del client a cui ho fatto la richiesta di connessione
char* USERNAME_RESPONSE; //buffer contenente lo username del client che mi ha inviato una richiesta di connessione
char* buf_commands;
static volatile int   IS_CHATTING;      // 0 se non connesso 1 se connesso
static volatile int   WAITING_RESPONSE; // 0 se non sta aspettando una risposta dal server 1 altrimenti
static volatile int   IS_RESPONDING;    // 1 se sta aspettando che lo user accetti o no la connessione 0 altrimenti
static sig_atomic_t   GLOBAL_EXIT;      // 1 se bisogna interrompere il programma 0 altrimenti

//handler di segnale per SIGHUP SIGINT SIGTERM SIGPIPE
void sigHandler(int sig){

  fprintf(stdout, "\n<<<<<<CATCHED SIGNAL>>>>>>\n");

  GLOBAL_EXIT = 1;
  return;

}

void _initSignals(){

  int ret;

  sigset_t mask;
  struct sigaction sigint_act, sigpipe_act, sigterm_act, sighup_act;

  memset(&sigint_act,  0, sizeof(struct sigaction));
  memset(&sigpipe_act, 0, sizeof(struct sigaction));
  memset(&sigterm_act, 0, sizeof(struct sigaction));

  ret = sigfillset(&mask);
  ERROR_HELPER(ret, "[MAIN] Error in sigfillset function");

  //SIGINT
  sigint_act.sa_mask    = mask;
  sigint_act.sa_flags   = 0;
  sigint_act.sa_handler = sigHandler;

  ret = sigaction(SIGINT, &sigint_act, NULL);
  ERROR_HELPER(ret, "[MAIN] Error in sigaction function");

  //SIGPIPE
  sigpipe_act.sa_mask    = mask;
  sigpipe_act.sa_flags   = 0;
  sigpipe_act.sa_handler = sigHandler;

  ret = sigaction(SIGPIPE, &sigpipe_act, NULL);
  ERROR_HELPER(ret, "[MAIN] Error in sigaction function");

  //SIGTERM
  sigterm_act.sa_mask    = mask;
  sigterm_act.sa_flags   = 0;
  sigterm_act.sa_handler = sigHandler;

  ret = sigaction(SIGTERM, &sigterm_act, NULL);
  ERROR_HELPER(ret, "[MAIN] Error in sigaction function");

  //SIGHUP
  sighup_act.sa_mask    = mask;
  sighup_act.sa_flags   = 0;
  sighup_act.sa_handler = sigHandler;

  ret = sigaction(SIGHUP, &sighup_act, NULL);
  ERROR_HELPER(ret, "[MAIN] Error in sigaction function");

  return;

}

void _initSemaphores(){

  int ret;

  //creating sempahore for listen function in usrl_liste_thread_routine for bind action
  ret = sem_init(&sync_receiver, 0, 0);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_receiver semaphore");

  //creating semaphore for connect_to function
  ret = sem_init(&wait_response, 0, 0);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &wait_response semaphore");

  //creating sempahore for mutual exlusion in userList
  ret = sem_init(&sync_userList, 0, 1);
  ERROR_HELPER(ret, "[MAIN] [FATAL ERROR] Could not init &sync_userList semaphore");

  return;
}

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

  //utilizzo il semaforo sync_userList per l'accesso alla GLibHashTable
  ret = sem_wait(&sync_userList);
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in wait function on sync_userList semaphore\n");

  FOR_EACH(user_list, (GHFunc)print_userList, NULL); //printing hashtable

  ret = sem_post(&sync_userList);
  ERROR_HELPER(ret, "[CONNECT_ROUTINE] Error in post function on sync_userList semaphore\n");

}

void send_message(int socket){

  //il client si trova nello stato IS_CHATTING

  int ret, len;

  //indico al server che il messaggio che gli arriva e' un messaggio di chat
  buf_commands[0] = MESSAGE;
  len = strlen(buf_commands);
  buf_commands[len-1] = '\n';
  buf_commands[len]   = '\0';

  //invio messaggio al server
  ret = send_msg(socket, buf_commands);
  //server morto, client termina
  if(ret == -2){
    GLOBAL_EXIT = 1;
    return;
  }

  //se il messaggio inviato al server era EXIT esco dallo stato IS_CHATTING
  //pulisco tutto buf_commands
  if(strncmp(buf_commands+1, EXIT, strlen(buf_commands+1))==0){
    IS_CHATTING = 0;
    memset(buf_commands, 0, MSG_LEN);
    return;
  }

  //preparo buf_commands a prendere da input un nuovo messaggio di chat
  //il primo byte rimane MESSAGE
  memset(buf_commands+1, 0, MSG_LEN-1);
  return;
}

void connect_to(int socket, char* target_client){

  int ret;

  //entro nello stato WAITING_RESPONSE
  WAITING_RESPONSE = 1;

  fprintf(stdout, "Connect to: ");

  //primo byte di target_client e' CONNECTION_REQUEST cosi il server sa che e'
  //una richiesta di connessione ad un altro client
  target_client[0] = CONNECTION_REQUEST;

  //prende lo username da terminale
  fgets(target_client+1, MSG_LEN-1, stdin);
  //riempio USERNAME_REQUEST con lo username indicato dall'utente
  strncpy(USERNAME_REQUEST, target_client+1, strlen(target_client)-2);

  //invio la richiesta di connessione a USERNAME_REQUEST al server
  ret = send_msg(socket, target_client);
  //server morto, client termina
  if(ret == -2){
    GLOBAL_EXIT = 1;
    return;
  }

  //pulisco i buffer buf_commands e target_client per prossimi input
  memset(buf_commands,  0, MSG_LEN);
  memset(target_client, 0, MSG_LEN);

  //aspetto una risposta dal server riguardo la mia richiesta di connessione
  ret = sem_wait(&wait_response);
  ERROR_HELPER(ret, "[MAIN] Error in sem_wait wait_response");

  //risposta arrivata esco dallo stato WAITING_RESPONSE ed entro nello stato IS_CHATTING
  //pulisco USERNAME_REQUEST per future richieste di connessione
  memset(USERNAME_REQUEST, 0, USERNAME_BUF_SIZE);
  WAITING_RESPONSE = 0;

  return;
}

void reply(int socket){

  int ret;

  //input preso su buf_commands
  //controllo sull'input
  if(((buf_commands[1]=='y' || buf_commands[1]=='n') && strlen(buf_commands)==3)){

    //ho accettato la connessione
    if(buf_commands[0]== CONNECTION_RESPONSE && buf_commands[1] == 'y'){
      IS_RESPONDING = 0; //esco dallo stato IS_RESPONDING
      IS_CHATTING = 1;   //entro nello stato IS_CHATTING
      //lo USERNAME_RESPONSE diviene USERNAME_CHAT
      strncpy(USERNAME_CHAT, USERNAME_RESPONSE, strlen(USERNAME_RESPONSE));
    }
    //ho rifiutato la connessione
    else{
      IS_RESPONDING = 0; //esco dallo stato IS_RESPONDING
      IS_CHATTING = 0;   //mi accerto che lo stato non sia IS_CHATTING
    }

    //copio USERNAME_RESPONSE per rispondere alla sua richiesta di connessione
    strncpy(buf_commands+2, USERNAME_RESPONSE, strlen(USERNAME_RESPONSE));
    buf_commands[strlen(buf_commands)]   = '\n';
    buf_commands[strlen(buf_commands)+1] = '\0';

    //invio al server buf_commands indicando che e' un comando di risposta ad una
    //richiesta di connessione contenente la risposta e lo username del client
    //che ha effettutato la richiesta di connessione
    ret = send_msg(socket, buf_commands);
    //il server e' morto, il client termina
    if(ret == -2){
      fprintf(stdout, "GRAVEEE\n");
      GLOBAL_EXIT = 1;
      return;
    }
    //pulisco buf_commands cosi da poter prendere altri input da utente
    memset(buf_commands, 0, MSG_LEN);
    return;
  }
  //input da utente sbagliato viene pulito buf_commands dal secondo byte in poi
  //il primo byte rimane CONNECTION_RESPONSE e viene chiesto allo user di inserire
  //una risposta valida y/n
  else{
    fprintf(stdout, "Wrong input for connetion response. Insert y/n....\n");
    memset(buf_commands+1, 0, MSG_LEN-1);
    return;
  }

}

int get_username(char* username, int socket){

  int i = 0, ret, length;

  fprintf(stdout, "Enter username(max 16 char): ");

  //fgets su buffer di 256 per controllare input dell'utente
  username = fgets(username, 256, stdin);
  length = strlen(username);

  if(GLOBAL_EXIT){
    return 1;
  }

  //----------------------CONTROLLO USERNAME------------------------------------
  //
  //se lunghezza del buffer di input e' maggiore o uguale di 255 allora puo essere
  //che il buffer di sistema stdin sia stato sporcato e la prossima fgets avrebbe un comportamente anomalo
  //il programma termina
  else if(length>=255){
    GLOBAL_EXIT = 1;
    fprintf(stdout, "\nUser input excessively long...closing program\n");
    return 1;
  }

  //se la lunghezza delll'input super quella consentita per lo username ritorna 0
  else if(length>16){
    //username troppo lungo ritorno 0
    return 0;
  }

  //aggiunta di terminatore di stringa
  username[length]   = '\0';

  //controllo per carattere non consentito '-'
  for(i=0; i < length; i++){
    if(username[i]=='-'){
      fprintf(stdout, "Char '-' found in username this char is not allowed in a username\n");
      //username contiene il carattere '-' ritorno 0
      return 0;
    }
  }
  //
  //----------------------------------------------------------------------------


  //----------------------------INVIO USERNAME----------------------------------
  //
  //allocazione buffer di invio dello username al server
  char* buf = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
  strncpy(buf, username, length);

  //invio dello username al server
  ret = send_msg(socket, buf);
  //se ret = -2 il server e' morto, il client termina
  if(ret == -2){
    GLOBAL_EXIT = 1;
    return 1;
  }
  //
  //----------------------------------------------------------------------------


  //--------------ASPETTO CONFERMA DI DISPONIBILITA' DAL SERVER-----------------
  //
  //pulisco buffer di invio per essere usato in ricezione
  memset(buf, 0, USERNAME_BUF_SIZE);
  ret = recv_msg(socket, buf, 3);

  //server e' morto il client termina
  if(ret == -1){
    GLOBAL_EXIT = 1;
    free(buf);
    return 1;
  }
  //
  //----------------------------------------------------------------------------

  //controllo messaggio ricevuto dal server riguardo la disponibilita' dello username
  if(buf[0] == UNAVAILABLE){
    free(buf);
    fprintf(stdout, "This username is already in use...choose a different username\n");
    //username non disponibile, gia in uso da un altro client
    return 0;
  }

  //altrimenti username disponibile
  free(buf);
  username[length-1] = '\0';
  return 1;
}

void display_commands(){
  //stampa della stringa dei comandi, vedere client.h
  fprintf(stdout, CMD_STRING);
  return;
}

void update_list(char* buf_userName, usr_list_elem_t* elem, char* mod_command){

  int ret;

  //********************AGGIORNAMENTO DELLA USER LIST***************************
  //
  /*
    Aggiornamento dell user list in base al comando mod_command passato in argomento
  */
  //****************************************************************************

  //blocco accesso alla user list attraverso sync_userList, entro in sezione critica
  ret = sem_wait(&sync_userList);
  ERROR_HELPER(ret, "[UPDATE_LIST] Error in wait function on sync_userList semaphore\n");

  //caso di aggiunta di nuovo elemento alla lista
  if(mod_command[0] == NEW){

    INSERT(user_list, (gpointer)buf_userName, (gpointer)elem);
    //esco dalla sezione critica
    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[UPDATE_LIST] Error in post function on sync_userList semaphore\n");

    return;
  }
  //caso di modifica di un elemento della lista
  else if(mod_command[0] == MODIFY){
    //estraggo il puntatore all'elemento della user list e ne modifico il flag di disponibilita'
    usr_list_elem_t* element = (usr_list_elem_t*)LOOKUP(user_list, (gconstpointer)buf_userName);
    element->a_flag = elem->a_flag;
    //esco dalla sezione critica
    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[UPDATE_LIST] Error in post function on sync_userList semaphore\n");

    return;
  }
  //caso di rimozione di un utente dalla lista
  else{
    //caso in cui l'utente da rimuovere e' lo stesso con cui stavo chattando
    if(strcmp(buf_userName, USERNAME_CHAT)==0){\
      //esco dallo stato IS_CHATTING
      IS_CHATTING = 0;
    }

    //caso in cui l'utente da rimuovere e' quello a cui ho inviato una richiesta di connessione
    else if(strcmp(buf_userName, USERNAME_REQUEST)==0){
      //sblocco semaforo &wait_response per il main
      ret = sem_post(&wait_response);
      ERROR_HELPER(ret, "[READ_UPDATES] Error in sem_post on &wait_response semaphore");
    }

    //caso in cui l'utente da rimuovere e' quello che mi ha richiesto la connessione
    else if(strcmp(buf_userName, USERNAME_RESPONSE)==0){
      buf_commands[0] = 0;
      memset(USERNAME_RESPONSE, 0, USERNAME_BUF_SIZE);
    }

    //rimozione e relativa liberazione della memoria dell'elemento della user list
    ret = REMOVE(user_list, (gpointer)buf_userName);
    if(!ret){
      fprintf(stdout, "[UPDATE_LIST] Error in remove function\n");
    }

    //esco dalla zona critica
    ret = sem_post(&sync_userList);
    ERROR_HELPER(ret, "[UPDATE_LIST] Error in post function on sync_userList semaphore\n");

    return;
  }

}

void parse_elem_list(const char* buf, usr_list_elem_t* elem, char* buf_userName, char* mod_command){

  //************PARSING DEL MESSAGGIO DI AGGIORNAMENTO USER LIST****************
  /*
    Questa funzione estrae quattro campi dai messaggi realativi all'aggiornamento
    della user_list i quali sono tutti nel seguente formato:

      "COMANDO DI MODIFICA-USERNAME-IP ADDRESS-FLAG DI AVAILABILITY-"

    dove:
      COMANDO DI MODIFICA e' un char
      USERNAME e' una stringa di lunghezza massima USERNAME_LENGTH
      IP ADDRESS e' una stringa di lunghezza massima INET_ADDRSTRLEN
      FLAG AVAILABILITY e' un char
    tutti separati da un trattino alto '-'.
  */
  //****************************************************************************

  int i, j;

  mod_command[0] = buf[0];

  for(j=2; j<42; j++){
    if(buf[j]=='-'){
      j++;
      break;
    }
  }

  strncpy(buf_userName, buf+2, j-3);
  buf_userName[j-3] = '\0';

  if(mod_command[0] == DELETE){
    elem->client_ip = "";
    elem->a_flag    = UNAVAILABLE;
    return;
  }

  i = j;

  for(; j<42; j++){
    if(buf[j]=='-'){
      j++;
      break;
    }
  }

  strncpy(elem->client_ip, buf+i, j-i-1);
  elem->client_ip[j-i-1] = '\0';

  if(buf[j] == AVAILABLE){
    elem->a_flag = AVAILABLE;
  }
  else{
    elem->a_flag = UNAVAILABLE;
  }

  return;
}

void* read_updates(void* args){

  int ret;

  //REF sulla mail box passata in argomento dal thread_usrl_recv
  read_updates_args_t* arg = (read_updates_args_t*)args;
  GAsyncQueue* buf = REF(arg->read_updates_mailbox);

  while(1){

    //------------------ESTRAZIONE MESSAGGI DALLA MAIL BOX------------------------
    //
    //buffer di appoggio per il messaggio estratto dalla mail box
    char* elem_buf;

    while(1){

      if(GLOBAL_EXIT){
        break;
      }
      //tentativo di estrazione di un messaggio dalla mail box con timer
      elem_buf = (char*)POP(buf, POP_TIMEOUT);
      //se l'elemento esiste allora esco dal ciclo di estrazione
      if(elem_buf != NULL){
        break;
      }
    }
    //
    //--------------------------------------------------------------------------

    //controllo variabile di stato GLOBAL_EXIT
    if(GLOBAL_EXIT){
      break;
    }

    //----------------ELABORAZIONE COMANDI E MESSAGGI CHAT----------------------
    //
    //controllo se messaggio e' di chat
    if(elem_buf[0]==MESSAGE){
      //controllo se il messagio di exit
      if(!strcmp(elem_buf+1,"exit")){
        //esco dallo stato di IS_CHATTING
        IS_CHATTING = 0;
        fprintf(stdout, ">> exit message received from [%s] press ENTER to exit chat\n", USERNAME_CHAT);
        //pulisco USERNAME_CHAT per future connessioni
        memset(USERNAME_CHAT, 0, USERNAME_BUF_SIZE);
        continue;
      }
      //stampa del messaggio chat a schermo
      fprintf(stdout, "\n[%s]>> %s\n", USERNAME_CHAT, elem_buf+1);
      //libero memoria dalla mail box
      free(elem_buf);
      continue;
    }

    //richiesta di connessione da parte di un client
    else if(elem_buf[0] == CONNECTION_REQUEST){
      //entro nello stato IS_RESPONDING
      IS_RESPONDING = 1;
      //copio dentro USERNAME_RESPONSE il nome del client che mi ha richiesto la connessione
      strncpy(USERNAME_RESPONSE, elem_buf+1, strlen(elem_buf)-1);
      fprintf(stdout, "\nConnection request from [%s] accept [y] refuse [n]: \n", elem_buf+1);
      //setto il primo byte di buf_commands a CONNECTION_RESPONSE
      buf_commands[0] = CONNECTION_RESPONSE;
      //libero memoria dalla mail box
      free(elem_buf);
      continue;

    }

    //risposta del server relativa ad una connection request effettuata da me
    else if(elem_buf[0] == CONNECTION_RESPONSE){

      if(elem_buf[1] == 'y'){

        strncpy(USERNAME_CHAT, elem_buf+2, strlen(elem_buf)-2);
        fprintf(stdout, "You are now chatting with: %s\n", USERNAME_CHAT);
        //entro nello stato IS_CHATTING
        IS_CHATTING = 1;
        //setto primo byte di buf_commands a MESSAGE
        buf_commands[0] = MESSAGE;

      }
      else{
        fprintf(stdout, "Connection declined\n");
        IS_CHATTING = 0;
      }

      //sblocco &wait_response per il processo main
      ret = sem_post(&wait_response);
      ERROR_HELPER(ret, "[READ_UPDATES] Error in sem_post on &wait_response semaphore");
      //libero memoria dalla mail box
      free(elem_buf);
      continue;

    }
    //
    //--------------------------------------------------------------------------


    //-------------ELABORAZIONE DI AGGIORNAMENTI DELLA USER LIST----------------
    //
    //allocazione memoria per gli elementi da passare alla funzione parse_elem_list
    usr_list_elem_t* elem = (usr_list_elem_t*)calloc(1, sizeof(usr_list_elem_t));
    elem->client_ip       = (char*)calloc(INET_ADDRSTRLEN,sizeof(char));
    char* userName        = (char*)calloc(USERNAME_BUF_SIZE,sizeof(char));
    char* command         = (char*)calloc(2, sizeof(char));

    //parsing del messaggio elem_buf ricevuto dal server
    //vengono estratti indirizzo IP, nome e comando da eseguire per l'aggiornamento della user list
    parse_elem_list(elem_buf, elem, userName, command);

    //libero memoria dalla mail box
    free(elem_buf);

    //passo lo username del client da modificare e il relativo comando di modifica
    //alla funzione di aggiornamento della user list update_list()
    update_list(userName, elem, command);
    //libero memoria non piu utilizzata
    free(command);
    //
    //--------------------------------------------------------------------------

  }

  //-------------------------TERMINAZIONE THREAD--------------------------------
  //
  //UNREF della mail box passata in argomento dal thread_usrl_recv
  UNREF(arg->read_updates_mailbox);
  UNREF(buf);

  fprintf(stdout, "[READ_UPDATES] exiting\n");

  pthread_exit(NULL);
  //
  //----------------------------------------------------------------------------

}

void* recv_updates(void* args){

  int ret, NO_CONNECTION;

  NO_CONNECTION = 1; //variabile di stato per il thread, 0 se connesso al server 1 altrimenti

  //inizializzo struttura timeval per la recv()
  struct timeval timeout;
  timeout.tv_sec  = 2; //2 secondi
  timeout.tv_usec = 0; //0 nano secondi


  //--------------PREPARAZIONE A RICEVERE CONNESSIONE DAL SERVER----------------
  //
  //descrittore socket per ricevere la connessione del server
  int usrl_recv_socket = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(usrl_recv_socket, "[RECV_UPDATES] Error while creating user list receiver thread socket descriptor");

  //inizializzazione mail box per immagazzinare i messaggi del server
  GAsyncQueue* mail_box = mailbox_queue_init();
  GAsyncQueue* buf_modifications = REF(mail_box); //aggiungo una reference alla mail box

  //creazione thread manage_updates per l'elaborazione dei messaggi della mail box
  pthread_t manage_updates;
  ((read_updates_args_t*) args)->read_updates_mailbox = mail_box;
  ret = pthread_create(&manage_updates, NULL, read_updates, (void*)args);
  PTHREAD_ERROR_HELPER(ret, "[RECV_UPDATES] Unable to create manage_updates thread");

  //--INIZIALIZZAZIONE STRUTTURA DATI PER ACCETTAZIONE CONNESSIONE DEL SERVER---
  struct sockaddr_in *usrl_sender_address = (struct sockaddr_in*)calloc(1, sizeof(struct sockaddr_in));
  socklen_t usrl_sender_address_len = sizeof(usrl_sender_address);

  struct sockaddr_in thread_addr = {0};
  thread_addr.sin_family      = AF_INET;
  thread_addr.sin_port        = htons(CLIENT_THREAD_RECEIVER_PORT);
  thread_addr.sin_addr.s_addr = INADDR_ANY;
  //----------------------------------------------------------------------------

  //abilitiamo reuseaddr_opt cosi da riutilizzare lo stesso indirizzo sulla
  //stessa socket in seguito alcrash del programma
  int reuseaddr_opt = 1;
  ret = setsockopt(usrl_recv_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot set SO_REUSEADDR option");

  //bind dello user list receiver thread address a quello user list receiver thread socket
  ret = bind(usrl_recv_socket, (const struct sockaddr*)&thread_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "[RECV_UPDATES] Error while binding address to user list receiver thread socket");

  //in attesa di connesione da parte del server
  ret = listen(usrl_recv_socket, 1);
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot listen on user list receiver thread socket");
  //
  //----------------------------------------------------------------------------


  //sblocco &sync_receiver notificando al processo main che sono pronto per ricevere
  //la connessione del server
  ret = sem_post(&sync_receiver);
  ERROR_HELPER(ret, "[RECV_UPDATES] Error in sem_post on &sync_receiver semaphore (user list receiver thread routine)");

  //abilito SO_RCVTIMEO rendendo le operazioni su usrl_recv_socket non bloccanti
  //SO_RCVTIMEO verra' ereditato anche da rec_socket
  ret = setsockopt(usrl_recv_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot set SO_RCVTIMEO option on usrl_recv_socket");

  int rec_socket; //socket connessa al server
  int tim = 0;    //contatore


  //---------------ACCETTAZIONE CONNESSIONE DA PARTE DEL SERVER-----------------
  //
  while(!GLOBAL_EXIT){

    //accettazione connessione da parte del server
    rec_socket = accept(usrl_recv_socket, (struct sockaddr*) usrl_sender_address, &usrl_sender_address_len);
    //gestione errori accept()
    if(rec_socket==-1 && (errno == EAGAIN || errno == EWOULDBLOCK )){
      tim++;
      if(tim >= MAX_WAIT_SERVER){ // se tim >=MAX_WAIT_SERVER si chiude il client
        GLOBAL_EXIT=1;
        fprintf(stdout, "\n<<<<MAX_WAIT_SERVER timer expired>>>>\n");
      }
      //SO_RCVTIMEO scaduto, ritento l'accettazione della connessione
      continue;
    }
    ERROR_HELPER(ret, "[RECV_UPDATES] Cannot accept connection on user list receiver thread socket");

    //connessione con server avvenuta con successo
    NO_CONNECTION = 0;
    break;
  }
  //
  //----------------------------------------------------------------------------


  //allocazione buffer per la ricezione di messaggi dal server
  char* buf = (char*)calloc(MSG_LEN, sizeof(char));


  //-------------------RICEZIONE MESSAGGI DAL SERVER----------------------------
  //
  while(1){

      //controllo GLOBAL_EXIT
      if(GLOBAL_EXIT){
        break;
      }

      //pulizia del buffer di ricezione messaggi
      memset(buf, 0, MSG_LEN);
      //ricevo messaggi dal server su socket non bloccante
      ret = recv_msg(rec_socket, buf, MSG_LEN);
      fprintf(stdout, "BUF: %s\n", buf);

      //controllo se scaduto SO_RCVTIMEO
      if(ret==-2){
        continue; //ritento a ricevere
      }

      //server morto segnalo chiusura del programma
      else if(ret==-1){
        GLOBAL_EXIT = 1;
        break;
      }

      if(!GLOBAL_EXIT){
        size_t len = strlen(buf);
        //allocazione buffer da inserire nella mail box
        char* queueBuf_elem = (char*)calloc(len+1, sizeof(char));
        //copio messaggio del server nel buffer da inserire nella mail box
        strncpy(queueBuf_elem, buf, strlen(buf));
        //inserisco messaggio in testa alla mail box
        PUSH(buf_modifications, (gpointer)queueBuf_elem);
      }

  }
  //
  //----------------------------------------------------------------------------


  //------------------OPERAZIONI DI CHIUSURA DEL THREAD-------------------------
  //
  //aspetto che il thread di elaborazioni dei messaggi termini
  ret = pthread_join(manage_updates, NULL);

  //libero memoria allocata
  free(buf);
  free(usrl_sender_address);

  //unreference della mail box
  UNREF(buf_modifications);

  fprintf(stderr, "[RECV_UPDATES] closing rec_socket and arg->socket...\n");

  //chiusura socket e controllo se rec_socket era connessa altrimenti fallisce la close()
  if(!NO_CONNECTION){
    ret = close(rec_socket);
    ERROR_HELPER(ret, "[RECV_UPDATES] Cannot close socket");
  }
  ret = close(usrl_recv_socket);
  ERROR_HELPER(ret, "[RECV_UPDATES] Cannot close usrl_recv_socket");

  fprintf(stderr, "[RECV_UPDATES] closed rec_socket and arg->socket succesfully\n");

  fprintf(stderr, "[RECV_UPDATES] exiting recv_updates\n");

  fprintf(stderr, "[RECV_UPDATES] Press any key to exit\n");

  //controllo stato del client, se WAITING_RESPONSE = 1 sblocco il semaforo al main
  if(WAITING_RESPONSE){
    //sblocco &wait_response per il processo main
    ret = sem_post(&wait_response);
    ERROR_HELPER(ret, "[RECV_UPDATES] Error in sem_post on &wait_response semaphore");
  }
  //
  //----------------------------------------------------------------------------

  pthread_exit(NULL);

}

int main(int argc, char* argv[]){

  int ret;

  system("clear");

  fprintf(stdout, "\n\nWelcome to Talk Service 2017\n\n");

  //-------------PREPARAZIONE CLIENT PER CONNESSIONE AL SERVER------------------
  //
  //inizializzazione dei segnali
  _initSignals();

  //inizializzazione delle variabili di stato e dei buffer globali
  IS_CHATTING       = 0; // non sono connesso a nessuno per adesso
  GLOBAL_EXIT       = 0; // non devo terminare
  WAITING_RESPONSE  = 0; // non aspetto nessuna risposta da server
  IS_RESPONDING     = 0; // non ho richieste a cui devo rispondere
  USERNAME          = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
  USERNAME_CHAT     = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
  USERNAME_REQUEST  = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));
  USERNAME_RESPONSE = (char*)calloc(USERNAME_BUF_SIZE, sizeof(char));

  //allocazione memoria per il buffer di controllo di input dello username da parte dello user
  char* check_username    = (char*)calloc(256, sizeof(char));

  //inizializzazione dei semafori sync_receiver, sync_userList e wait_response;
  _initSemaphores();

  //inizializzazione della GLibHashTable per la lista utenti
  user_list = usr_list_init();

  //allocazione memoria per il buffer di disconnect
  char* disconnect  = (char*)calloc(3, sizeof(char));
  strcpy(disconnect,  "c\n");

  //descrittore socket per la connessione al server
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "[MAIN] Error while creating client socket descriptor");

  //creazione della struttura dati sockaddr_in per la connessione al server
  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family         = AF_INET;
  serv_addr.sin_port           = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &(serv_addr.sin_addr.s_addr));

  //----CREAZIONE thread_usrl_recv THREAD PER RICEZIONE MESSAGGI DAL SERVER-----
  pthread_t thread_usrl_recv;
  read_updates_args_t* thread_usrl_recv_args = (read_updates_args_t*)malloc(sizeof(read_updates_args_t));
  ret = pthread_create(&thread_usrl_recv, NULL, recv_updates, (void*)thread_usrl_recv_args);
  PTHREAD_ERROR_HELPER(ret, "[MAIN] Unable to create user list receiver thread");

  //in attesa che il thread_usrl_recv si metta in ascolto sulla socket
  ret = sem_wait(&sync_receiver);
  ERROR_HELPER(ret, "[MAIN] Error in sem_wait (main process)");

  //distruggo il semaforo sync_receiver
  ret = sem_destroy(&sync_receiver);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_receiver semaphore");
  //
  //----------------------------------------------------------------------------


  //----------------------CONNESSIONE AL SERVER---------------------------------
  //
  ret = connect(socket_desc, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr_in));
  if(ret == -1 && (errno == ENETUNREACH || errno == ECONNREFUSED)){
    fprintf(stdout, "[MAIN] Connection refused from host or no connection available\n");
    GLOBAL_EXIT = 1; //nessuna connesione verso il server, chiudo il programma
  }
  else{
    ERROR_HELPER(ret, "[MAIN] Error trying to connect to server");
  }
  //
  //----------------------------------------------------------------------------

  //-----------------------INVIO USERNAME AL SERVER-----------------------------
  //
  while(!GLOBAL_EXIT){
    ret = get_username(check_username, socket_desc);
    if(ret==1){
      break;
    }
    memset(check_username, 0, 256); //pulisco il buffer di controllo input
  }
  //
  //----------------------------------------------------------------------------

  //se GLOBAL_EXIT = 1 non effettuo la strncpy
  if(!GLOBAL_EXIT){
    strncpy(USERNAME, check_username, strlen(check_username));
  }

  //libero memoria non piu' utilizzata
  free(check_username);

  //inizializzazione memoria per buffer di input dallo user
  buf_commands = (char*)calloc(MSG_LEN, sizeof(char));
  char* user_buf = (char*)calloc(MSG_LEN, sizeof(char));

  //stampa a schermo dei comandi disponibili per l'utente
  if(!GLOBAL_EXIT) display_commands();

  //--------------CICLO DI VALIDAZIONE DEGLI INPUT DELLO USER-------------------
  //
  while(1){

    if(GLOBAL_EXIT){
      fprintf(stdout, "Ready to exit process...\n");
      break;
    }

    else if(IS_CHATTING){
      fprintf(stdout, "[%s]>>: ", USERNAME);
    }

    else if(IS_RESPONDING){
      fprintf(stdout, "Input y/n: ");
    }

    else{
      fprintf(stdout, "IMPUT COMMAND (help to display commands): ");
    }

    fgets(buf_commands+1, MSG_LEN-3, stdin);

    //se IS_CHATTING = 1 il client e' nello stato di chat e buf_commands viene gestito come messaggio di chat
    if(IS_CHATTING){
      send_message(socket_desc);
      continue;
    }

    //stato del client IS_RESPONDING = 1
    else if(buf_commands[0] == CONNECTION_RESPONSE){
      reply(socket_desc);
      continue;
    }

    //stampa a schermo la lista utenti
    else if(strcmp(buf_commands+1, LIST)==0 && !GLOBAL_EXIT){  //per la lista
      list_command();
      memset(buf_commands, 0, MSG_LEN);
      continue;
    }

    //richiesta di conessione ad un client
    else if(strcmp(buf_commands+1, CONNECT)==0 && !GLOBAL_EXIT){ //per connettersi al client
      connect_to(socket_desc, user_buf);
      continue;
    }

    //stampa a schermo la lista dei comandi disponibili all'utente
    else if(strcmp(buf_commands+1, HELP)==0 && !GLOBAL_EXIT){
      display_commands();
      memset(buf_commands,  0, MSG_LEN);
      continue;
    }

    //setta GLOBAL_EXIT = 1 indicando la chiusura del programma
    else if(strcmp(buf_commands+1, EXIT)==0 && !GLOBAL_EXIT){
      GLOBAL_EXIT = 1;
      break;
    }

    //pulisce schermo
    else if(strcmp(buf_commands+1, CLEAR)==0 && !GLOBAL_EXIT){
      memset(buf_commands,  0, MSG_LEN);
      system("clear");
      continue;
    }

    //input dell'utente non corretto
    else{
      if(GLOBAL_EXIT){
        break;
      }
      fprintf(stdout, "\n>>command not found\n");
      display_commands();
      memset(buf_commands,  0, MSG_LEN);
    }

  }
  //
  //----------------------------------------------------------------------------

  //--------------------------CHIUSURA PROGRAMMA--------------------------------
  //
  //invio messaggio di disconnessione al server, se la send() fallisce il programma non termina
  //ma continua con le funzioni di pulizia di memoria
  send_msg(socket_desc, disconnect);

  //in attesa che il thread_usrl_recv termini
  ret = pthread_join(thread_usrl_recv, NULL);

  fprintf(stdout, "[MAIN] destroying semaphores....\n");

  //-----------------------DISTRUZIONE SEMAFORI---------------------------------
  ret = sem_destroy(&sync_userList);
  ERROR_HELPER(ret, "[MAIN] Error destroying &sync_userList semaphore");

  ret = sem_destroy(&wait_response);
  ERROR_HELPER(ret, "[MAIN] Error destroying &wait_response semaphore");
  //----------------------------------------------------------------------------

  fprintf(stdout, "[MAIN] freeing allocated data...\n");

  //----------------------LIBERO MEMORIA ALLOCATA-------------------------------
  free(USERNAME);
  free(USERNAME_CHAT);
  free(USERNAME_REQUEST);
  free(USERNAME_RESPONSE);
  free(buf_commands);
  free(user_buf);
  free(thread_usrl_recv_args);
  free(disconnect);
  //----------------------------------------------------------------------------

  fprintf(stdout, "[MAIN] Destroying user list...\n");
  //distruzione della GLibHashTable e relativa liberazione della memoria dei campi
  DESTROY(user_list);

  fprintf(stdout, "[MAIN] closing socket descriptors...\n");

  //chiusura della socket di connessione al server
  ret = close(socket_desc);
  ERROR_HELPER(ret, "[MAIN] Cannot close socket_desc");

  fprintf(stdout, "[MAIN] exiting main process\n\n");

  exit(EXIT_SUCCESS);

}
