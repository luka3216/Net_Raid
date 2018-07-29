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

#include "lux_client.h"
#include "lux_server.h"

int set_up_server(char **args)
{
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_adress;

  server_adress.sin_family = AF_INET;
  server_adress.sin_addr.s_addr = inet_addr(args[0]);
  server_adress.sin_port = htons(atoi(args[1]));

  strcpy(_path_to_storage, args[2]);

  int storage_path_fd = open(_path_to_storage, __O_DIRECTORY);

  bind(server_socket, (struct sockaddr *)&server_adress, sizeof(server_adress));

  listen(server_socket, 10);

  printf("accepting calls\n");
  int client_socket = accept(server_socket, NULL, NULL);
  printf("client connected. \n");
  struct storage_info* storage_info = malloc(sizeof(struct storage_info));
  recv(client_socket, storage_info, sizeof(struct storage_info), 0);
  printf("managing storage: %s. raid: %d \n", storage_info->storage_name, storage_info->raid);
}

int main(int argc, char **argv)
{
  if (argc != 4)
  {
    printf("usage: lux_server ip port path_to_storage");
  }
  return set_up_server(&argv[1]);
}