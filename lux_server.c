#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "lux_client.h"
#include "lux_server.h"
#include "raid_one_manager.h"
int set_up_server(char **args)
{
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  
  //no fail socket accept on rerun :@
  int optval = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

  struct sockaddr_in server_adress;

  server_adress.sin_family = AF_INET;
  server_adress.sin_addr.s_addr = inet_addr(args[0]);
  server_adress.sin_port = htons(atoi(args[1]));

  strcpy(_path_to_storage, args[2]);

  _storage_path_fd = open(_path_to_storage, __O_DIRECTORY);

  bind(server_socket, (struct sockaddr *)&server_adress, sizeof(server_adress)) ? printf("bind successful.\n") : printf("bind unsuccessful: %s\n", strerror(errno));

  listen(server_socket, 0) ? printf("failed to listen.\n") : printf("listening to port: %d\n", server_adress.sin_port);

  printf("accepting calls\n");
  int client_socket = accept(server_socket, NULL, NULL);
  printf("client connected. \n");
  _this_storage = malloc(sizeof(struct storage_info));
  recv(client_socket, _this_storage, sizeof(struct storage_info), 0);
  printf("managing storage: %s. raid: %d \n", _this_storage->storage_name, _this_storage->raid);
  return manage_server_raid_one(client_socket, _path_to_storage);
}

int main(int argc, char **argv)
{
  if (argc != 4)
  {
    printf("usage: lux_server ip port path_to_storage");
  }
  return set_up_server(&argv[1]);
}