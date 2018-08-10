#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>

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
  fflush(stdout);
  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  if (sent > 0)
  {
    struct raid_one_response response;

    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

    printf("status %d for %s\n", response.error, input.path);
    fflush(stdout);

    memcpy(stbuf, &response.one_stat, sizeof(struct stat));

    close(sock_fd);
    return -response.error;
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
  fflush(stdout);

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

  send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  recv(sock_fd, buf, size, 0);

  close(sock_fd);
  return size;
}

static int lux_write(const char *path, const char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  (void)fi;

  int sock_fd = get_live_server_fd();

  struct raid_one_input input;
  input.command = WRITE;
  strcpy(input.path, path);
  input.offset = offset;
  input.size = size;

  printf("attemting (write) contact with server for path %s for %zu bytes.\n", path, size);
  fflush(stdout);

  send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  int response;
  recv(sock_fd, &response, sizeof(int), 0);

  send(sock_fd, buf, input.size, 0);

  close(sock_fd);
  return size;
}

static int lux_access(const char* path, int flags) {
  
  struct raid_one_input input;
  input.command = ACCESS;
  strcpy(input.path, path);
  input.flags = flags;
  
  int sock_fd = get_live_server_fd();

  printf("attemting (access) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.error != 0)
  {
    printf("access at %s unsuccessful\n", path);
  }
  else
  {
    printf("access at %s successful\n", path);
  }

  close(sock_fd);
  return -response.error;
}

static int lux_truncate(const char* path, off_t size) {
  
  struct raid_one_input input;
  input.command = TRUNCATE;
  strcpy(input.path, path);
  input.offset = size;
  
  int sock_fd = get_live_server_fd();

  printf("attemting (truncate) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.error != 0)
  {
    printf("truncate at %s unsuccessful\n", path);
  }
  else
  {
    printf("truncate at %s successful\n", path);
  }

  close(sock_fd);
  return -response.error;
}

static int lux_rename(const char* old, const char* new) {
  struct raid_one_input input;
  input.command = RENAME;
  strcpy(input.path, old);
  strcpy(input.char_buf, new);
  
  int sock_fd = get_live_server_fd();

  printf("attemting (rename) contact with server for path %s\n", old);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.error != 0)
  {
    printf("rename to %s unsuccessful\n", new);
  }
  else
  {
    printf("rename to %s successful\n", new);
  }

  close(sock_fd);
  return -response.error;
}


static int lux_unlink(const char* path) {
  struct raid_one_input input;
  input.command = UNLINK;
  strcpy(input.path, path);
  
  int sock_fd = get_live_server_fd();

  printf("attemting (unlink) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.error != 0)
  {
    printf("unlink to %s unsuccessful\n", path);
  }
  else
  {
    printf("unlink to %s successful\n", path);
  }

  close(sock_fd);
  return -response.error;
}

static int lux_rmdir(const char* path) {
  struct raid_one_input input;
  input.command = RMDIR;
  strcpy(input.path, path);
  
  int sock_fd = get_live_server_fd();

  printf("attemting (rmdir) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.error != 0)
  {
    printf("rmdir to %s unsuccessful\n", path);
  }
  else
  {
    printf("rmdir to %s successful\n", path);
  }

  close(sock_fd);
  return -response.error;
}

static int lux_mknod(const char* path, mode_t mode, dev_t dev) {
  struct raid_one_input input;
  input.command = CREATE;
  strcpy(input.path, path);
  input.mode = mode;
  input.dev = dev;
  
  int sock_fd = get_live_server_fd();

  printf("attemting (mknod) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.error != 0)
  {
    printf("mknod to %s unsuccessful\n", path);
  }
  else
  {
    printf("mknod to %s successful\n", path);
  }

  close(sock_fd);
  return -response.error;
}


static int lux_mkdir(const char* path, mode_t mode) {
  struct raid_one_input input;
  input.command = MKDIR;
  strcpy(input.path, path);
  input.mode = mode;
  
  int sock_fd = get_live_server_fd();

  printf("attemting (mkdir) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.error != 0)
  {
    printf("mkdir to %s unsuccessful\n", path);
  }
  else
  {
    printf("mkdir to %s successful\n", path);
  }

  close(sock_fd);
  return -response.error;
}


static int lux_utimens(const char* path, const struct timespec tv[2]) {
    struct raid_one_input input;
  input.command = UTIMENS;
  strcpy(input.path, path);
  memcpy(input.spec, tv, 2*sizeof(struct timespec));
  
  int sock_fd = get_live_server_fd();

  printf("attemting (utimens) contact with server for path %s\n", path);
  fflush(stdout);

  int sent = send(sock_fd, &input, sizeof(struct raid_one_input), 0);

  struct raid_one_response response;

  if (sent > 0)
    recv(sock_fd, &response, sizeof(struct raid_one_response), 0);

  if (response.error != 0)
  {
    printf("utimens to %s unsuccessful\n", path);
  }
  else
  {
    printf("utimens to %s successful\n", path);
  }

  close(sock_fd);
  return -response.error;
}

static int lux_release(const char* path, struct fuse_file_info* fi) {
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