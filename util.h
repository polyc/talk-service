#ifndef __UTIL_H__
#define __UTIL_H__

void send_msg(int socket, char *buf);
int recv_msg(int socket, char *buf, size_t buf_len);
void free_user_list_element_value(gpointer data);
void free_user_list_element_key(gpointer data);
void free_mailbox(gpointer data);
GHashTable* usr_list_init();
GHashTable* mailbox_list_init();
GAsyncQueue* mailbox_queue_init();

#endif
