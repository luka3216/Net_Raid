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
#include <dirent.h>
#include "lux_client.h"
#include "lux_common.h"

char _path_to_storage[256];

int handle_getattr(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling gettattr at: %s\n", path);

  struct raid_one_response response;
  response.status = stat(path, &response.one_stat);
  printf("getattr return: %d\n", response.status);

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_readdir(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling readdir at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.status = -1;

  char buff[256];

  DIR *d;
  struct dirent *dir;
  d = opendir(path);
  if (d)
  {
    int i = 0;
    while ((dir = readdir(d)) != NULL)
    {
      struct stat si;
      sprintf(buff, "%s%s", path, dir->d_name);
      stat(buff, &si);
      fflush(stdout);
      strcpy(response.filenames[i], dir->d_name);
      response.stats[i] = si;
      i++;
    }
    closedir(d);
    response.size = i;
    response.status = 0;
  }

  printf("sending readdir response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_open(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling open at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.status = open(path, 0) == -1 ? -1 : 0;

  printf("sending open response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_read(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling read at: %s\n", path);

  fflush(stdout);

  struct raid_one_file_response response;
  int fd = open(path, 0);
  response.status = fd == -1 ? -1 : 0;

  int size = read(fd, response.buff, 4088);

  response.size = size;

  printf("sending read response.\n");

  send(client_socket, &response, sizeof(struct raid_one_file_response), 0);
}

int handle_input(struct raid_one_input input, int client_socket)
{
  switch (input.command)
  {
  case TEST:
    break;
  case GETATTR:
    handle_getattr(input, client_socket);
    break;
  case READDIR:
    handle_readdir(input, client_socket);
    break;
  case OPEN:
    handle_open(input, client_socket);
    break;
  case READ:
    handle_read(input, client_socket);
    break;
  default:;
  }
}

int manage_server_raid_one(int client_socket, char *path_to_storage)
{
  strcpy(_path_to_storage, path_to_storage);
  while (1)
  {
    struct raid_one_input input;
    int size = recv(client_socket, &input, sizeof(struct raid_one_input), 0);
    handle_input(input, client_socket);
  }
  return 0;
}