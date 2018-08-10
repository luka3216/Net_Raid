#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <signal.h>

#include "lux_client.h"
#include "lux_fuse.h"
#include "lux_common.h"

struct raid_one_input generate_server_input(
  int command,
  const char* path,
  const char* char_buf,
  off_t offset,
  size_t size,
  mode_t mode,
  dev_t dev,
  const struct timespec* spec,
  int flags) {
    struct raid_one_input result;
    result.command = command;
    if (path != NULL) strcpy(result.path, path);
    if (char_buf != NULL) strcpy(result.char_buf, char_buf);
    result.offset = offset;
    result.size = size;
    result.mode = mode;
    result.dev = dev;
    if (spec != NULL) memcpy(result.spec, spec, 2 * sizeof(struct timespec));
    result.flags = flags;
    return result;
  }

/* return a socket fd to a live server, -1 if failed to connect with both servers.
   caller's responsible to close the fd. */
int get_live_server_fd()
{
  for (int i = 0; i < 2; i++)
  {
    if (_this_storage.servers[i]->alive)
    {
      int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
      int res = connect(sock_fd, (struct sockaddr *)&_this_storage.servers[i]->server_adress, sizeof(struct sockaddr_in));

      if (res != -1)
      {
        return sock_fd;
      }
      else
      {
        close(sock_fd);
      }
    }
  }
  return -1;
}

struct raid_one_live_sockets get_live_sockets()
{
  struct raid_one_live_sockets result;
  result.count = 0;
  for (int i = 0; i < 2; i++)
  {
    if (_this_storage.servers[i]->alive)
    {
      int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
      int res = connect(sock_fd, (struct sockaddr *)&_this_storage.servers[i]->server_adress, sizeof(struct sockaddr_in));

      if (res != -1)
      {
        result.sock_fd[result.count++] = sock_fd;
      }
      else
      {
        close(sock_fd);
      }
    }
  }
  return result;
}

int handle_error(int sock_fd)
{
  close(sock_fd);
  return -errno;
}

int handle_return(int sock_fd, int error)
{
  close(sock_fd);
  return -error;
}

static int lux_getattr(const char *path, struct stat *stbuf)
{
  int sock_fd = get_live_server_fd();

  memset(stbuf, 0, sizeof(struct stat));

  struct raid_one_input input = generate_server_input(GETATTR, path, NULL, 0, 0, 0, 0, NULL, 0);

  printf("attemting (getattr) contact with server for path %s\n", path);
  fflush(stdout);

  struct raid_one_response response;

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, &response, sizeof(struct raid_one_response), 0) < 0)
    return handle_error(sock_fd);

  printf("status %d for %s\n", response.error, input.path);
  fflush(stdout);

  memcpy(stbuf, &response.one_stat, sizeof(struct stat));

  return handle_return(sock_fd, response.error);
}

static int lux_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  (void)offset;
  (void)fi;

  int sock_fd = get_live_server_fd();

  struct raid_one_input input = generate_server_input(READDIR, path, NULL, 0, 0, 0, 0, NULL, 0);

  printf("attemting (readdir) contact with server for path %s\n", path);
  fflush(stdout);

  struct raid_one_directories_response response;

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, &response, sizeof(struct raid_one_directories_response), 0) < 0)
    return handle_error(sock_fd);

  printf("readdir returned.\n");
  fflush(stdout);

  for (int i = 0; i < response.size; i++)
  {
    filler(buf, response.filenames[i], &response.stats[i], 0);
  }

  return handle_return(sock_fd, response.error);
}

static int lux_open(const char *path, struct fuse_file_info *fi)
{
  struct raid_one_input input = generate_server_input(OPEN, path, NULL, 0, 0, 0, 0, NULL, 0);

  int sock_fd = get_live_server_fd();

  printf("attemting (open) contact with server for path %s\n", path);
  fflush(stdout);

  struct raid_one_response response;

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, &response, sizeof(struct raid_one_response), 0) < 0)
  {
    return handle_error(sock_fd);
  }

  return handle_return(sock_fd, response.error);
}

static int lux_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  (void)fi;

  int sock_fd = get_live_server_fd();

  struct raid_one_input input = generate_server_input(READ, path, NULL, offset, size, 0, 0, NULL, 0);

  printf("attemting (read) contact with server for path %s for %zu bytes.\n", path, size);
  fflush(stdout);

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, buf, size, 0) < 0)
  {
    return handle_error(sock_fd);
  }

  close(sock_fd);
  return size;
}

static int lux_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
  (void)fi;

  server_sockets socks = get_live_sockets();

  struct raid_one_input input = generate_server_input(WRITE, path, NULL, offset, size, 0, 0, NULL, 0);

  int responses[2];

  printf("attemting (write) contact with server for path %s for %zu bytes.\n", path, size);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0
        || recv(socks.sock_fd[i], &responses[i], sizeof(int), 0) < 0 
        || send(socks.sock_fd[i], buf, input.size, 0) < 0)
      return handle_error(socks.sock_fd[i]);
  }
  return size;
}

static int lux_access(const char *path, int flags)
{
  struct raid_one_input input = generate_server_input(ACCESS, path, NULL, 0, 0, 0, 0, NULL, flags);

  int sock_fd = get_live_server_fd();

  printf("attemting (access) contact with server for path %s\n", path);
  fflush(stdout);

  struct raid_one_response response;

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, &response, sizeof(struct raid_one_response), 0) < 0)
  {
    return handle_error(sock_fd);
  }

  close(sock_fd);
  return -response.error;
}

static int lux_truncate(const char *path, off_t size)
{
  server_sockets socks = get_live_sockets();

  struct raid_one_input input = generate_server_input(TRUNCATE, path, NULL, size, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (truncate) contact with server for path %s for %zu bytes.\n", path, size);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0
        || recv(socks.sock_fd[i], &responses[i], sizeof(int), 0) < 0 )
      return handle_error(socks.sock_fd[i]);
  }
  return 0;
}

static int lux_rename(const char *old, const char *new)
{
  server_sockets socks = get_live_sockets();

  struct raid_one_input input = generate_server_input(RENAME, old, new, 0, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (rename) contact with server for path %s for bytes.\n", old);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0
        || recv(socks.sock_fd[i], &responses[i], sizeof(int), 0) < 0 )
      return handle_error(socks.sock_fd[i]);
  }
  return 0;
}

static int lux_unlink(const char *path)
{   
  server_sockets socks = get_live_sockets();

  struct raid_one_input input = generate_server_input(UNLINK, path, NULL, 0, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (unlink) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0
        || recv(socks.sock_fd[i], &responses[i], sizeof(int), 0) < 0 )
      return handle_error(socks.sock_fd[i]);
  }
  return 0;
}

static int lux_rmdir(const char *path)
{
  server_sockets socks = get_live_sockets();

  struct raid_one_input input = generate_server_input(RMDIR, path, NULL, 0, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (rmdir) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0
        || recv(socks.sock_fd[i], &responses[i], sizeof(int), 0) < 0 )
      return handle_error(socks.sock_fd[i]);
  }
  return 0;
}

static int lux_mknod(const char *path, mode_t mode, dev_t dev)
{
  server_sockets socks = get_live_sockets();

  struct raid_one_input input = generate_server_input(CREATE, path, NULL, 0, 0, mode, dev, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (mknod) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0
        || recv(socks.sock_fd[i], &responses[i], sizeof(int), 0) < 0 )
      return handle_error(socks.sock_fd[i]);
  }
  return 0;
}

static int lux_mkdir(const char *path, mode_t mode)
{  
  server_sockets socks = get_live_sockets();

  struct raid_one_input input = generate_server_input(MKDIR, path, NULL, 0, 0, mode, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (mkdir) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0
        || recv(socks.sock_fd[i], &responses[i], sizeof(int), 0) < 0 )
      return handle_error(socks.sock_fd[i]);
  }
  return 0;
}

static int lux_utimens(const char *path, const struct timespec tv[2])
{
  server_sockets socks = get_live_sockets();

  struct raid_one_input input = generate_server_input(UTIMENS, path, NULL, 0, 0, 0, 0, tv, 0);

  struct raid_one_response responses[2];

  printf("attemting (utimens) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0
        || recv(socks.sock_fd[i], &responses[i], sizeof(int), 0) < 0 )
      return handle_error(socks.sock_fd[i]);
  }
  return 0;
}

static int lux_release(const char *path, struct fuse_file_info *fi)
{
  return 0;
}

static struct fuse_operations hello_oper = {
    .getattr = lux_getattr,
    .readdir = lux_readdir,
    .open = lux_open,
    .read = lux_read,
    .access = lux_access,
    .write = lux_write,
    .truncate = lux_truncate,
    .rename = lux_rename,
    .release = lux_release,
    .unlink = lux_unlink,
    .rmdir = lux_rmdir,
    .mknod = lux_mknod,
    .utimens = lux_utimens,
    .mkdir = lux_mkdir,
};

int run_storage_raid_one(struct storage_info *storage_info)
{
  _this_storage = *storage_info;
  char **args = malloc(3 * sizeof(char *));
  args[0] = strdup("useless");
  args[1] = _this_storage.mountpoint;
  args[2] = strdup("-f");
  args[3] = strdup("-s");
  args[4] = NULL;
  return fuse_main(4, args, &hello_oper, NULL);
}