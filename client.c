#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "common.h"


int main(){

  int socket_server_desc = 0;

  struct sockaddr_in serv_addr;

  serv_addr.sin_len = sizeof(sockaddr_in);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  serv_addr.sin_addr = SERVER_IP;

  socket_server_desc = socket(AF_INET, SOCK_STREAM, 0);

  ERROR_HELPER(socket_server_desc, "error while creating client socket descriptor");

  int ret = connect(socket_server_desc, (const struct sockaddr)&serv_addr, sizeof(serv_addr));

  ERROR_HELPER(ret, "error trying to connetc to server");

}
