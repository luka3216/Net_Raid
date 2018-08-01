#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "lux_client.h"
#include "lux_fuse.h"
#include "lux_common.h"


int get_live_server()
{
  struct raid_one_input test;
  test.command = TEST;
  if (_this_storage.servers[0]->alive)
  {
    int res = send(_this_storage.servers[0]->socket_fd, &test, sizeof(struct raid_one_input), 0);
    if (res > 0)
      return 0;
  }
  if (_this_storage.servers[1]->alive)
  {
    int res = send(_this_storage.servers[1]->socket_fd, &test, sizeof(struct raid_one_input), 0);
    if (res > 0)
      return 1;
  }
  return -1;
}

static int lux_getattr(const char *path, struct stat *stbuf)
{
  memset(stbuf, 0, sizeof(struct stat));

  struct raid_one_input input;
  input.command = GETATTR;
  strcpy(input.path, path);

  int server_id = get_live_server();

  printf("attemting (getattr) contact with server %d for path %s\n", server_id, path);
  int sent = send(_this_storage.servers[server_id]->socket_fd, &input, sizeof(struct raid_one_input), 0);

  if (sent > 0)
  {
    struct raid_one_response response;

    recv(_this_storage.servers[server_id]->socket_fd, &response, sizeof(struct raid_one_response), 0);

    printf("status %d for %s\n", response.status, input.path);

    memcpy(stbuf, &response.one_stat, sizeof(struct stat));

    return response.status;
  }
  return -1;
}

static int lux_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  (void)offset;
  (void)fi;

  struct raid_one_input input;
  input.command = READDIR;
  strcpy(input.path, path);

  int server_id = get_live_server();

  printf("attemting (readdir) contact with server %d for path %s\n", server_id, path);
  fflush(stdout);

  int sent = send(_this_storage.servers[server_id]->socket_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(_this_storage.servers[server_id]->socket_fd, &response, sizeof(struct raid_one_response), 0);

  printf("readdir returned.\n");

  if (response.status != 0)
    return errno;

  for (int i = 0; i < response.size; i++)
  {
    filler(buf, response.filenames[i], &response.stats[i], 0);
  }

  return 0;
}

static int lux_open(const char *path, struct fuse_file_info *fi)
{
  struct raid_one_input input;
  input.command = OPEN;
  strcpy(input.path, path);

  int server_id = get_live_server();
  
  printf("attemting (open) contact with server %d for path %s\n", server_id, path);
  fflush(stdout);

  int sent = send(_this_storage.servers[server_id]->socket_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(_this_storage.servers[server_id]->socket_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.status == -1) {
    printf("open at %s unsuccessful\n", path);
  } else {
    printf("open at %s successful\n", path);
  }

  return response.status;
}

static int lux_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
  (void)fi;

  struct raid_one_input input;
  input.command = READ;
  strcpy(input.path, path);
  input.offset = offset;
  input.size = size;

  int server_id = get_live_server();
  
  printf("attemting (read) contact with server %d for path %s for %zu bytes.\n", server_id, path, size);
  fflush(stdout);

  int sent = send(_this_storage.servers[server_id]->socket_fd, &input, sizeof(struct raid_one_input), 0);

  if (size > 4088)
  ; //TO-DO

  struct raid_one_file_response response;

  if (sent > 0)
    recv(_this_storage.servers[server_id]->socket_fd, &response, sizeof(struct raid_one_file_response), 0);

  if (response.status == -1) {
    printf("read at %s unsuccessful\n", path);

    return -1;

  } else {
    printf("read at %s successful\n", path);
  }

  memcpy(buf, response.buff, response.size);

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
  args[3] = NULL;
  return fuse_main(3, args, &hello_oper, NULL);
}