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
#include <sys/sendfile.h>
#include <errno.h>
#include "lux_client.h"
#include "lux_common.h"

char _path_to_storage[256];


int handle_getattr(struct raid_one_input input, int client_socket)
{ 
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling getattr at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (stat(path, &response.one_stat) != 0) {
    response.error = errno;
  }

  printf("sending getattr response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_readdir(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling readdir at: %s\n", path);

  fflush(stdout);

  struct raid_one_directories_response response;
  response.error = 0;

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
  } else {
    response.size = 0;
    response.error = errno;
  }

  printf("sending readdir response.\n");

  send(client_socket, &response, sizeof(struct raid_one_directories_response), 0);
}

int handle_open(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling open at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (open(path, 0) == -1) {
    response.error = errno;
  }

  printf("sending open response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_access(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling access at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (access(path, input.flags) != 0) {
    response.error = errno;
  }

  printf("sending access response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_read(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling read at: %s\n", path);

  fflush(stdout);

  int fd = open(path, O_RDONLY);

  printf("sending read response.\n");

  sendfile(client_socket, fd, &input.offset, input.size);
  
  close(fd);
}

int handle_write(struct raid_one_input input, int client_socket)
{
  char path[256];
  char buff[32 * 4096];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling write at: %s\n", path);

  fflush(stdout);

  int fd = open(path, O_WRONLY);

  lseek(fd, input.offset, SEEK_SET);

  int response = 0;
  send(client_socket, &response, sizeof(int), 0);

  recv(client_socket, buff, input.size, 0);
  
  write(fd, buff, input.size);
  
  close(fd);
}

int handle_truncate(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling truncate at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (truncate(path, input.offset) != 0) {
    response.error = errno;
  }

  printf("sending truncate response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}


int handle_rename(struct raid_one_input input, int client_socket)
{
  char path[256];
  char path2[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  sprintf(path2, "%s%s", _path_to_storage, input.char_buf);
  printf("handling rename at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (rename(path, path2) != 0) {
    response.error = errno;
  }

  printf("sending rename response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_unlink(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling unlink at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (unlink(path) != 0) {
    response.error = errno;
  }

  printf("sending unlink response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_rmdir(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling rmdir at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (rmdir(path) != 0) {
    response.error = errno;
  }

  printf("sending rmdir response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_mknod(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling mknod at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (mknod(path, input.mode, input.dev) != 0) {
    response.error = errno;
  }

  printf("sending mknod response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_mkdir(struct raid_one_input input, int client_socket)
{
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling mkdir at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (mkdir(path, input.mode) != 0) {
    response.error = errno;
  }

  printf("sending mkdir response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0);
}

int handle_utimens(struct raid_one_input input, int client_socket) {
  char path[256];
  sprintf(path, "%s%s", _path_to_storage, input.path);
  printf("handling utimens at: %s\n", path);

  fflush(stdout);

  struct raid_one_response response;
  response.error = 0;
  if (utimensat(AT_FDCWD, path, input.spec, 0) != 0) {
    response.error = errno;
  }

  printf("sending utimens response.\n");

  send(client_socket, &response, sizeof(struct raid_one_response), 0); 
}

int handle_input(struct raid_one_input input, int client_socket)
{
  switch (input.command)
  {
  case DIE:
    exit(0);
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
  case ACCESS:
    handle_access(input, client_socket);
    break;
  case WRITE:
    handle_write(input, client_socket);
    break;
  case TRUNCATE:
    handle_truncate(input, client_socket);
    break;
  case RENAME:
    handle_rename(input, client_socket);
    break;
  case UNLINK:
    handle_unlink(input, client_socket);
    break;
  case RMDIR:
    handle_rmdir(input, client_socket);
    break;
  case CREATE:
    handle_mknod(input, client_socket);
    break;
  case UTIMENS:
    handle_utimens(input, client_socket);
    break;
  case MKDIR:
    handle_mkdir(input, client_socket);
    break;
  default:;
  }
}

int manage_server_raid_one(int server_socket, char *path_to_storage)
{
  strcpy(_path_to_storage, path_to_storage);
  while (1)
  {
    struct raid_one_input input;
    int client_socket = accept(server_socket, NULL, NULL);
    int size = recv(client_socket, &input, sizeof(struct raid_one_input), 0);
    handle_input(input, client_socket);
    close(client_socket);
  }
  return 0;
}