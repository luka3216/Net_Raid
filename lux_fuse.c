#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "lux_client.h"
#include "lux_fuse.h"
#include "lux_common.h"

/* return a socket fd to a live server, -1 if failed to connect with both servers.
   caller's responsible to close the fd. */
int get_live_server_fd()
{
  if (_this_storage.servers[0]->alive)
  {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int res = connect(sock_fd, (struct sockaddr *)&_this_storage.servers[0]->server_adress, sizeof(struct sockaddr_in));

    if (res != -1)
    {
      return sock_fd;
    } else {
      close(sock_fd);
    }
  }
  if (_this_storage.servers[1]->alive)
  {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int res = connect(sock_fd, (struct sockaddr *)&_this_storage.servers[1]->server_adress, sizeof(struct sockaddr_in));

    if (res != -1)
    {
      return sock_fd;
    } else {
      close(sock_fd);
    }
  }
  return -1;
}

static int lux_getattr(const char *path, struct stat *stbuf)
{
  int sock_fd = get_live_server_fd();
  
  memset(stbuf, 0, sizeof(struct stat));

  struct raid_one_input input;
  input.command = GETATTR;
  strcpy(input.path, path);

  printf("attemting (getattr) contact with server for path %s\n", path);
  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  if (sent > 0)
  {
    struct raid_one_response response;

    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

    printf("status %d for %s\n", response.status, input.path);

    memcpy(stbuf, &response.one_stat, sizeof(struct stat));

    close(sock_fd);
    return response.status;
  }
  close(sock_fd);
  return -1;
}

static int lux_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  (void)offset;
  (void)fi;

  int sock_fd = get_live_server_fd();

  struct raid_one_input input;
  input.command = READDIR;
  strcpy(input.path, path);

  printf("attemting (readdir) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  printf("readdir returned.\n");

  if (response.status != 0)
  {
    close(sock_fd);
    return errno;
  }

  for (int i = 0; i < response.size; i++)
  {
    filler(buf, response.filenames[i], &response.stats[i], 0);
  }

  close(sock_fd);
  return 0;
}

static int lux_open(const char *path, struct fuse_file_info *fi)
{
  struct raid_one_input input;
  input.command = OPEN;
  strcpy(input.path, path);
  
  int sock_fd = get_live_server_fd();

  printf("attemting (open) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.status == -1)
  {
    printf("open at %s unsuccessful\n", path);
  }
  else
  {
    printf("open at %s successful\n", path);
  }

  close(sock_fd);
  return response.status;
}

static int lux_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  (void)fi;

  int sock_fd = get_live_server_fd();

  struct raid_one_input input;
  input.command = READ;
  strcpy(input.path, path);
  input.offset = offset;
  input.size = size;

  printf("attemting (read) contact with server for path %s for %zu bytes.\n", path, size);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  if (size > 4088)
    ; //TO-DO

  struct raid_one_file_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_file_response), 0);

  if (response.status == -1)
  {
    printf("read at %s unsuccessful\n", path);
    close(sock_fd);
    return -1;
  }
  else
  {
    printf("read at %s successful\n", path);
  }

  memcpy(buf, response.buff, response.size);

  close(sock_fd);
  return response.size;
}

static struct fuse_operations hello_oper = {
    .getattr = lux_getattr,
    .readdir = lux_readdir,
    .open = lux_open,
    .read = lux_read,
};

int run_storage_raid_one(struct storage_info *storage_info)
{
  _this_storage = *storage_info;
  char **args = malloc(3 * sizeof(char *));
  args[0] = strdup("useless");
  args[1] = _this_storage.mountpoint;
  args[2] = strdup("-f");
  //args[3] = strdup("-s");
  args[3] = NULL;
  return fuse_main(3, args, &hello_oper, NULL);
}