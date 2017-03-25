#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>

#include "common.h"



void send_msg(int socket, char *buf) {
    int ret;

    int bytes_left = strlen(buf); //bruscolini al posto di pere ahahaha
    int bytes_sent = 0;

    while (bytes_left > 0) {
        ret = send(socket, buf + bytes_sent, bytes_left, 0);

        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Errore nella scrittura su socket");

        bytes_left -= ret;
        bytes_sent += ret;
    }
}

int recv_msg(int socket, char *buf, size_t buf_len) {
    int ret;
    int bytes_read = 0;

    // messages longer that buf_len wont be read all
    while (bytes_read <= buf_len) {
        ret = recv(socket, buf + bytes_read, 1, 0);

        if (ret == 0) return -1; // client closed the socket
        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Errore nella lettura da socket");

        // controlling last bye read
        if (buf[bytes_read] == '\n') break; //end of message

        bytes_read++;
    }

    buf[bytes_read] = '\0'; //adding string terminator
    return 0;
}

GHashTable* usr_list_init(){
  GHashTable* list = g_hash_table_new(g_str_hash, g_str_equal);
  return list;
}

GHashTable* thread_ref_init(){
  GHashTable* list = g_hash_table_new(g_int_hash, g_int_equal);
  return list;
}
