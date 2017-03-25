#ifndef __UTIL_H__
#define __UTIL_H__

void send_msg(int socket, char *buf);
int recv_msg(int socket, char *buf, size_t buf_len);
GHashTable* usr_list_init();
GHashTable* thread_ref_init();

#endif
