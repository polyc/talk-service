#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <glib.h>
#include "common.h"



int send_msg(int socket, char *buf) {
    int ret;

    int bytes_left = strlen(buf); //bruscolini al posto di pere ahahaha
    int bytes_sent = 0;

    while (bytes_left > 0) {
        ret = send(socket, buf + bytes_sent, bytes_left, 0);

        if (ret == -1 && errno == EINTR) continue;
        else if(ret == -1 && errno == EPIPE) return -1; //sigpipe
        else if(ret == -1 && (errno == ENETDOWN || errno == ENETUNREACH)) return -2;
        ERROR_HELPER(ret, "Errore nella scrittura su socket");

        bytes_left -= ret;
        bytes_sent += ret;
    }
  return 0;
}

int recv_msg(int socket, char *buf, size_t buf_len, int timer) {
    int ret;
    int bytes_read = 0;

    // messages longer that buf_len wont be read all
    while (bytes_read <= buf_len) {
        ret = recv(socket, buf + bytes_read, 1, 0);

        if (ret == 0) return -1; // client closed the socket
        else if(ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) return -2;
        else if(ret == -1 && errno == EINTR) continue;
        else if(ret == -1 && errno == ECONNRESET) return -1;
        else if(ret == -1 && errno == ENOTCONN) return -1;
        ERROR_HELPER(ret, "Errore nella lettura da socket");

        // controlling last bye read
        if (buf[bytes_read] == '\n') break; //end of message

        bytes_read++;
    }

    buf[bytes_read] = '\0'; //adding string terminator
    return 0;
}

//free hash table value
void free_user_list_element_value(gpointer data){
  free(((usr_list_elem_t*)data)->client_ip);
  free((usr_list_elem_t*)data);
  return;
}

//free hash table key
void free_user_list_element_key(gpointer data){
  free((char*)data);
  return;
}

//free mailbox entry
void free_mailbox(gpointer data){
  free((char*)data);
  return;
}

GHashTable* usr_list_init(){
  GHashTable* list = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)free_user_list_element_key, (GDestroyNotify)free_user_list_element_value);
  return list;
}

GHashTable* mailbox_list_init(){
  GHashTable* mailbox = g_hash_table_new(g_str_hash, g_str_equal);
  return mailbox;
}

GAsyncQueue* mailbox_queue_init(){
  return g_async_queue_new_full((GDestroyNotify) free_mailbox);
}

GAsyncQueue* thread_queue_init(){
  return g_async_queue_new();
}

GAsyncQueue* addresses_queue_init(){
  return g_async_queue_new();
}
