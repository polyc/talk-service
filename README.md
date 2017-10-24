# talk-service
Exam project - OS teaching 6 CFU - Università Sapienza Roma (Uniroma1)

Project specifications are in specs(ENGLISH).txt and specs(ITALIAN).txt.


## server
Scelte progettuali:

  * Server multithread:
    il processo principale accetta nuove connessioni da parte dei client, per i quali vengono generati una coppia di thread di gestione e invio messaggi. Se il client è inattivo per un certo periodo di tempo, il server ne determina la chiusura, liberando così la memoria ad esso associata.

    - Connection handler thread:
      si occupa di ricevere lo username dal client e di validarlo. In caso di username non corretto, lo comunica al client e ne aspetta uno nuovo. Tale thread poi si occupa di gestire i comandi inviati dal client e di generare un altro thread, che si occupa di notificare i client in "tempo reale" inviando messaggi custom. Essi possono essere richieste di connessione con un altro client, risposte a tali richieste e messaggi effetivi scambiati tra client enotifiche di vario genere mirate ad aggiornare le user-list presenti nei vari client.

    -Sender thread:
      il thread in questione riceve messaggi su una mailbox personale a cui ha accesso il relativo connection
      handler. Una volta estratto il messaggio dalla coda, lo invia al relativo client.

    -La sincronizzazione tra i due thread avviene tramite una coppia di semafori binari e una variabile visibile solo alla coppia stessa:
      il primo semafore viene usato per il timing delle operazioni, l'altro per determinare la terminazione del Sender thread notificata dal Connection handler. La variabile invece serve a notificare la necessità di terminare al Connection handler da parte del Sender thread nel caso di un SIGPIPE.

  * Il server gestisce i seguenti segnali:
    -SIGTERM
    -SIGINT
    -SIGHUP
    -SIGPIPE

    -Quando viene catturato SIGPIPE, termina "gracefully" la coppia di thread a causa della quale è avvenuta la segnalazione.

    -Negli altri casi il main thread aspetta che tutti i thread siano terminati "gracefully" e termina.

  * Strutture dati del server:

      <code>user_list</code>: Hash table contenente le strutture dati necessarie a rapresentare ogni singolo client connesso al servizio. L'accesso è regolato da un semaforo binario globale.

      <code>mailbox_list</code>: Hash table contenente le mailbox dei Sender thread su cui inviare i messaggi da spedire. Ogni mailbox è una coda asincrona thread safe.
