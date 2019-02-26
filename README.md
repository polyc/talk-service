# talk-service
Exam project - Operating Systems teaching - Sapienza University of Rome

Both the server and the client are based on UNIX system calls.

Regarding the implementation of data structures, it was decided to rely on the GLib external library.

## server
Design choices:

  * Multithread server:
    the main process accepts new connections from clients, for which a couple of management/sending threads are spawned. If the client is idle for a certain period of time, the server timeout it, thus freeing up the memory associated with it.

    - Connection handler thread:
      takes care of receiving the username from the client and validating it. In case of incorrect username, it communicates it to the client and waits for a new one. This thread then handles the commands sent by the client and generates another thread, which is responsible for notifying clients in "real time" by sending custom messages. They can be requests to connect with another client, answers to these requests and actual messages exchanged between clients and notifications of various kinds aimed at updating the user-lists present in clients.

    - Sender thread:
      the thread in question receives messages on a personal mailbox to which the related connection handler thread has access.  
      Once the message has been extracted from the queue, it is sent to the relevant client.

    - The synchronization between the two threads occurs through a pair of binary semaphores and a variable visible only to the couple itself:
      the first semaphore is used for the timing of the initialization operations, the other to determine the termination of the Sender thread notified by the Connection handler. The variable, on the other hand, serves to notify the need to terminate the Sender thread to the Connection handler in the case of a SIGPIPE occurred during the sending of notifications.

  * The server manages the following signals:
    - SIGTERM
    - SIGINT
    - SIGHUP
    - SIGPIPE

    -When SIGPIPE is captured, the thread pair, due to which the message occurred, terminates "gracefully".

    -In other cases the main thread waits for all threads to be "gracefully" terminated and ends.

  * Server data structures:

      `user_list`: hash table containing the data structures necessary to represent every single client connected to the service, the access key is the unique username chosen by the client. Access is regulated by a global binary semaphore.
      Each element of the list is represented by a struct, which has, as fields, the ip address of the client in dotted form and an availability flag. Through the flag are made (server side) the necessary controls so that the integrity of the service is not compromised during the management of the various activities.

       `mailbox_list`: hash table containing the mailboxes of the Sender threads on which to send the messages to client, also here the username is the access key. Each mailbox is an asynchronous thread safe queue that contains strings previously encoded by the connection handler thread.

## client
  Design choices:

  * Multithread client and user shell that manages user commands.

  * The client interacts with the server through commands integrated into it.

  * Each time the list of connected clients is changed, the server notifies the client's message receiving thread that updates its user list.

  * The client manages the following signals:
    - SIGTERM
    - SIGINT
    - SIGHUP
    - SIGPIPE
    once one of these signals is captured, the main thread waits for all threads to be "gracefully" terminated and sends a disconnection message to the server.

  * The client data structures are:

      `user_list`: an updated Hash Table in "real time" containing the name, IP address and availability of each client connected to the server protected by a semaphore.

      `mail_box`: a mail box containing all messages received from the server, which is a thread safe asynchronous queue.

  * Representation of client states via global variables.

  Commands available to the user:

  * `connect <username>`: asks the user which client to connect to and forwards a chat request to the identified user.
  * `list`: print the updated list in real time of the clients connected to the server.
  * `clear`: clean the terminal.
  * `exit`: sends a disconnection signal to the server and closes the client.
  * `help`: print the list of available commands on the screen.


## Build

   First you need to install the Glib library through the following command, for example on a linux system you will have:

* `sudo apt-get install libglib2.0-dev`


Next, you must enter the IPv4 address associated with the machine on which the server is running, in the common.h file and finally use the makefile to compile:

`make` compiles both the server and the client efficiently, that is, if there is a need to update each executable.

or:

* `make server` compiles only the server.
* `make client` compiles only the client.
* `make clean` deletes the executables generated by previous compilations.

## Execution

To run the server: `./server`

To stop it: `CTRL-C`

To run the client: `./client`

To stop it:
  * `CTRL-C`
  * Or type exit and press enter (if not in chat).
