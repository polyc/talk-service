# talk-service
Exam project - OS teaching 6 CFU - Università Sapienza Roma (Uniroma1)

Project specifications are in specs(ENGLISH).txt and specs(ITALIAN).txt.

Sia il server che il client sono basati su chiamate di sistema UNIX.

Per quanto riguarda l'implementazione delle strutture dati si è scelto di appoggiarsi alla libreria esterna GLib.

## server
Scelte progettuali:

  * Server multithread:
    il processo principale accetta nuove connessioni da parte dei client, per i quali vengono generati una coppia di thread di gestione e invio messaggi. Se il client è inattivo per un certo periodo di tempo, il server ne determina la chiusura, liberando così la memoria ad esso associata.

    - Connection handler thread:
      si occupa di ricevere lo username dal client e di validarlo. In caso di username non corretto, lo comunica al client e ne aspetta uno nuovo. Tale thread poi si occupa di gestire i comandi inviati dal client e di generare un altro thread, che si occupa di notificare i client in "tempo reale" inviando messaggi custom. Essi possono essere richieste di connessione con un altro client, risposte a tali richieste e messaggi effetivi scambiati tra client e notifiche di vario genere mirate ad aggiornare le user-list presenti nei vari client.

    - Sender thread:
      il thread in questione riceve messaggi su una mailbox personale a cui ha accesso il relativo connection
      handler. Una volta estratto il messaggio dalla coda, lo invia al relativo client.

    - La sincronizzazione tra i due thread avviene tramite una coppia di semafori binari e una variabile visibile solo alla coppia stessa:
      il primo semafore viene usato per il timing delle operazioni di inizializzazione, l'altro per determinare la terminazione del Sender thread notificata dal Connection handler. La variabile, invece, serve a notificare la necessità di terminare al Connection handler da parte del Sender thread nel caso di un SIGPIPE avvenuto durante l'invio delle notifiche.

  * Il server gestisce i seguenti segnali:
    - SIGTERM
    - SIGINT
    - SIGHUP
    - SIGPIPE

    -Quando viene catturato SIGPIPE, la coppia di thread a causa della quale è avvenuta la segnalazione termina "gracefully".

    -Negli altri casi il main thread aspetta che tutti i thread siano terminati "gracefully" e termina.

  * Strutture dati del server:

      <code>user_list</code>: hash table contenente le strutture dati necessarie a rapresentare ogni singolo client connesso al servizio, la chiave d'accesso è lo username univoco scelto dal client. L'accesso è regolato da un semaforo binario globale.
      Ogni elemento della lista è rappresentato da una struct, la quale ha come campi l'indirizzo ip del client in forma dotted e un flag di disponibilità. Attraverso il flag vengono effettuati (lato server) i controlli necessari affinchè l'integrità del servizio non venga compromessa durante la gestione delle varie attività.

      <code>mailbox_list</code>: hash table contenente le mailbox dei Sender thread su cui inviare i messaggi da spedire, anche qui la chiave d'accesso è lo username. Ogni mailbox è una coda asincrona thread safe che contiene stringhe codificate precedentemente dal connection handler thread.

## client
  Scelte progettuali:

  * Client Multithread e shell utente che gestisce dei comandi user.

  * Il client intergisce con il server attraverso dei comandi integrati nel client.

  * Ad ogni modifica della lista dei client connessi, il server notifica il thread di ricezione dei messagi del client che aggiorna la sua lista utenti.

  * Il client gestisce i seguenti segnali:
    - SIGTERM
    - SIGINT
    - SIGHUP
    - SIGPIPE
    una volta catturato uno di questi segnali il main thread aspetta che tutti i thread siano terminati "gracefully" e invia un messaggio di disconnessione al server.

  * Le strutture dati del client sono:

      <code>user_list</code> una Hash Table aggiornata in "tempo reale" contenente il nome, indirizzo IP e disponibilita' di ogni client connesso al server protetta da un semaforo.

      <code>mail_box</code>  una mail box contente tutti i messaggi ricevuti dal server, la quale è una coda asincrona thread safe.

  * Rappresentazione degli stati del client tramite variabili globali.

  Comandi disponibili per l'utente:

  * <code>connect</code> : chiede all'utente a quale client connettersi e inoltra una richiesta di chat verso l'utente identificato.
  * <code>list</code>    : stampa a schermo la lista aggiornata in tempo reale dei client connessi al server.
  * <code>clear</code>   : pulisce lo schermo.
  * <code>exit</code>    : invia un segnale di disconnessione al server e chiude il client.
  * <code>help</code>    : stampa a schermo la lista dei comandi disponibili.


## complilazione

   Innanzi tutto bisogna installare la libreria Glib attraverso il seguente comando, per esempio su un sistema linux si avrà:

* <code>sudo apt-get install libglib2.0-dev</code>


Succesivamente, si deve inserire l'indirizzo IPv4, associato alla macchina su cui viene eseguito il server, nel file <code>common.h</code> e infine utilizzare il <code>makefile</code> per compilare:
   * <code>make</code> compila sia il server che il client in maniera efficiente, cioè se vi è bisogno di aggiornare ciascun eseguibile.

   oppure:
   * <code>make server</code> compila solo il server.
   * <code>make client</code> compila solo il client.
   * <code>make clean</code> elimina gli eseguibili generati da compilazioni precedenti.


## esecuzione

   Per eseguire il server:
    <code>./server</code>

   Per arrestarlo:
   CTRL-C


   Per eseguire il client:
   <code>./client</code>

   Per arrestarlo:
   * CTRL-C
   * digitare la stringa exit e premere invio (se non si è in chat).
