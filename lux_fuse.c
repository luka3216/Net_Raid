#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <pthread.h>
#include <semaphore.h>

sem_t replication_defender;
int copier_demands_freedom;
int pending_requests;

#include "lux_client.h"
#include "lux_fuse.h"
#include "lux_common.h"
#include "lux_simple_list.c"
#include "logger.h"

struct raid_one_input generate_server_input(
    int command,
    const char *path,
    const char *char_buf,
    off_t offset,
    size_t size,
    mode_t mode,
    dev_t dev,
    const struct timespec *spec,
    int flags)
{
  struct raid_one_input result;
  result.command = command;
  if (path != NULL)
    strcpy(result.path, path);
  if (char_buf != NULL)
    strcpy(result.char_buf, char_buf);
  result.offset = offset;
  result.size = size;
  result.mode = mode;
  result.dev = dev;
  if (spec != NULL)
    memcpy(result.spec, spec, 2 * sizeof(struct timespec));
  result.flags = flags;
  return result;
}

int handle_error(int sock_fd, const char *path, int command)
{
  close(sock_fd);
  log_server_return(path, &_this_storage, command, errno);
  return -errno;
}

int handle_return(int sock_fd, int error, const char *path, int command)
{
  close(sock_fd);
  log_server_return(path, &_this_storage, command, error);
  return -error;
}

int handle_errors(struct raid_one_live_sockets socks, const char *path)
{
  sem_wait(&replication_defender);
  uint32_t hash = jenkins_one_at_a_time_hash(path);
  remove_hash(hash);
  pending_requests--;
  sem_post(&replication_defender);
  for (int i = 0; i < socks.count; i++)
  {
    close(socks.sock_fd[i]);
  }
  return -errno;
}

int handle_returns(struct raid_one_live_sockets socks, struct raid_one_response responses[2], const char *path)
{
  sem_wait(&replication_defender);
  uint32_t hash = jenkins_one_at_a_time_hash(path);
  remove_hash(hash);
  pending_requests--;
  sem_post(&replication_defender);
  int res = 0;
  for (int i = 0; i < socks.count; i++)
  {
    close(socks.sock_fd[i]);
    if (responses[i].error != 0)
      res = responses[i].error;
  }
  return -res;
}

int copy_routine(char *p, struct lux_server *from, struct lux_server *to)
{
  int sock_fd_from = get_server_fd(from);
  char path[256];

  if (sock_fd_from == -1)
  {
    printf("fatal connection failure. closing application.\n");
    exit(0);
  }
  sprintf(path, "%s/", p);
  struct raid_one_input input = generate_server_input(READDIR, path, NULL, 0, 0, 0, 0, NULL, 0);
  struct raid_one_directories_response response_dir;

  if (send(sock_fd_from, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_from, &response_dir, sizeof(struct raid_one_directories_response), 0) < 0)
    return handle_error(sock_fd_from, path, READDIR);

  close(sock_fd_from);

  for (int i = 0; i < response_dir.size; i++)
  {
    char path_tmp[256];
    sprintf(path_tmp, "%s%s", path, response_dir.filenames[i]);
    if (S_ISDIR(response_dir.stats[i].st_mode))
    {

      if (strstr(response_dir.filenames[i], ".") != NULL)
        continue;

      struct raid_one_input input = generate_server_input(MKDIR, path_tmp, NULL, 0, 0, response_dir.stats[i].st_mode, 0, NULL, 0);
      struct raid_one_response response;

      int sock_fd_to = get_server_fd(to);

      if (sock_fd_to == -1)
      {
        printf("fatal connection failure. closing application.\n");
        exit(0);
      }
      if (send(sock_fd_to, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_to, &response, sizeof(struct raid_one_response), 0) < 0)
        return handle_error(sock_fd_to, path_tmp, MKDIR);

      close(sock_fd_to);

      copy_routine(path_tmp, from, to);
    }
    else if (S_ISREG(response_dir.stats[i].st_mode))
    {
      struct raid_one_input input_mknod = generate_server_input(CREATE, path_tmp, NULL, 0, 0, response_dir.stats[i].st_mode, response_dir.stats[i].st_dev, NULL, 0);
      struct raid_one_response response;

      int sock_fd_to = get_server_fd(to);

      if (sock_fd_to == -1)
      {
        printf("fatal connection failure. closing application.\n");
        exit(0);
      }
      if (send(sock_fd_to, &input_mknod, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_to, &response, sizeof(struct raid_one_response), 0) < 0)
        return handle_error(sock_fd_to, path_tmp, CREATE);

      close(sock_fd_to);

      struct raid_one_input input_trunc = generate_server_input(TRUNCATE, path_tmp, NULL, 0, 0, 0, 0, NULL, 0);

      sock_fd_to = get_server_fd(to);

      if (sock_fd_to == -1)
      {
        printf("fatal connection failure. closing application.\n");
        exit(0);
      }
      if (send(sock_fd_to, &input_trunc, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_to, &response, sizeof(struct raid_one_response), 0) < 0)
        return handle_error(sock_fd_to, path_tmp, TRUNCATE);

      close(sock_fd_to);

      off_t f_size = response_dir.stats[i].st_size;
      off_t offse = 0;

      char data_buffer[4096];

      while (f_size > 0)
      {
        size_t req_size = 4096;
        struct raid_one_input input = generate_server_input(READ, path_tmp, NULL, offse, req_size, 0, 0, NULL, 0);
        int sock_fd_from = get_server_fd(from);

        if (sock_fd_from == -1)
        {
          printf("fatal connection failure. closing application.\n");
          exit(0);
        }
        if (send(sock_fd_from, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_from, data_buffer, req_size, 0) < 0)
          return handle_error(sock_fd_from, path_tmp, READ);

        close(sock_fd_from);

        req_size = (f_size > 4096) ? 4096 : f_size;
        input = generate_server_input(WRITE, path_tmp, NULL, offse, req_size, 0, 0, NULL, 0);
        struct raid_one_response response;
        int sock_fd_to = get_server_fd(to);

        if (sock_fd_to == -1)
        {
          printf("fatal connection failure. closing application.\n");
          exit(0);
        }
        if (send(sock_fd_to, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_to, &response, sizeof(struct raid_one_response), 0) < 0 || send(sock_fd_to, data_buffer, req_size, 0) < 0)
          return handle_error(sock_fd_to, path_tmp, WRITE);

        f_size -= 4096;
        offse += 4096;
        close(sock_fd_to);
      }
    }
  }
  return 0;
}

int increment_pending_number(const char *path)
{
  int res = 0;
  sem_wait(&replication_defender);
  if (copier_demands_freedom)
  {
    sem_post(&replication_defender);
    res = -1;
  }
  else
  {
    pending_requests++;
    uint32_t hash = jenkins_one_at_a_time_hash(path);
    if (add_hash(hash) == -1)
    {
      pending_requests--;
      res = -1;
    }
    sem_post(&replication_defender);
  }
  return res;
}

int copy_server_contents(struct lux_server *from, struct lux_server *to)
{
  sem_wait(&replication_defender);
  copier_demands_freedom = 1;
  sem_post(&replication_defender);
  int p;
  while (1)
  {
    sem_wait(&replication_defender);
    p = pending_requests;
    sem_post(&replication_defender);
    if (p <= 0)
      break;
    continue;
  }
  copy_routine("", from, to);
  sem_wait(&replication_defender);
  copier_demands_freedom = 0;
  sem_post(&replication_defender);
  return 0;
}

void *monitor_routine(void *args)
{
  int server_index = *(int *)args;
  struct lux_server *this_server = _this_storage.servers[server_index];
  while (1)
  {
    sleep(1);
    int sock_fd = get_server_fd(this_server);
    if (this_server->status == STATUS_ALIVE)
    {
      if (sock_fd == -1)
      {
        this_server->status = STATUS_DEGRADED;
        this_server->fail_time = time(NULL);
        printf("connection failed with server: %s:%d.\n", this_server->server_ip, this_server->port);
        log_serv_info(CONN_FAIL, &_this_storage, this_server, 0);
      }
      else
      {
        struct raid_one_input input = generate_server_input(DUMMY, NULL, NULL, 0, 0, 0, 0, NULL, 0);
        send(sock_fd, &input, sizeof(struct raid_one_input), 0);
        close(sock_fd);
        log_serv_info(CONN_SUCCESS, &_this_storage, this_server, 0);
      }
    }
    else if (this_server->status == STATUS_DEGRADED)
    {
      int time_since_fail = difftime(time(NULL), this_server->fail_time);
      if (sock_fd == -1)
      {
        printf("coudln't connect with server: %s:%d for %d seconds\n", this_server->server_ip, this_server->port, time_since_fail);
        log_serv_info(CONN_CONT_FAIL, &_this_storage, this_server, time_since_fail);
        if (time_since_fail >= _lux_client_info.timeout)
        {
          log_serv_info(CONN_FAIL_FINAL, &_this_storage, this_server, 0);
          log_serv_info(CONN_COPYING, &_this_storage, this_server, 0);
          copy_server_contents(_this_storage.servers[(server_index + 1) % 2], _this_storage.hotswap);
          log_serv_info(CONN_COPYING_COMPLETE, &_this_storage, this_server, 0);
          _this_storage.servers[server_index] = _this_storage.hotswap;
          this_server = _this_storage.servers[server_index];
          log_serv_info(CONN_REPLACED, &_this_storage, this_server, 0);
        }
      }
      else
      {
        struct raid_one_input input = generate_server_input(DUMMY, NULL, NULL, 0, 0, 0, 0, NULL, 0);
        send(sock_fd, &input, sizeof(struct raid_one_input), 0);
        close(sock_fd);
        printf("connection restored with server: %s:%d after %d seconds\n", this_server->server_ip, this_server->port, time_since_fail);
        log_serv_info(CONN_RESTORED, &_this_storage, this_server, time_since_fail);
        printf("replicating data to reconnected server.\n");
        log_serv_info(CONN_RESTORING, &_this_storage, this_server, 0);
        copy_server_contents(_this_storage.servers[(server_index + 1) % 2], this_server);
        this_server->status = STATUS_ALIVE;
        printf("replication complete\n");
        log_serv_info(CONN_RESTORING_COMPLETE, &_this_storage, this_server, 0);
      }
    }
  }
  return NULL;
}

int monitor_server(int server_index)
{
  pthread_t thread_id;
  int *buf = malloc(sizeof(int));
  memcpy(buf, &server_index, sizeof(int));
  pthread_create(&thread_id, NULL, monitor_routine, (void *)buf);
  return 0;
}

int get_server_fd(struct lux_server* server)
{
  {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sock_fd, (struct sockaddr *)&server->server_adress, sizeof(struct sockaddr_in)) != -1)
    {
      return sock_fd;
    }
    else
    {
      close(sock_fd);
    }
  }
  return -1;
}

/* return a socket fd to a live server, -1 if failed to connect with both servers.
   caller's responsible to close the fd. */
int get_live_server_fd()
{
  int res = -1;
  for (int i = 0; i < 2; i++)
  {
    struct lux_server *server = _this_storage.servers[i];
    if (server->status == STATUS_ALIVE)
    {
      res = get_server_fd(server);
      if (res != -1)
        break;
    }
  }
  return res;
}

struct raid_one_live_sockets get_live_sockets(struct lux_server *servers[2])
{
  struct raid_one_live_sockets result;
  result.count = 0;
  for (int i = 0; i < 2; i++)
  {
    struct lux_server *server = servers[i];
    if (server->status == STATUS_ALIVE)
    {
      int sock_fd = get_server_fd(server);
      if (sock_fd != -1)
        result.sock_fd[result.count++] = sock_fd;
    }
  }
  return result;
}
/*
int lux_init_server(int i)
{
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input;
  //input.command = INIT;

  printf("attemting (init) contact with server%s:%d.\n", _this_storage.servers[i]->server_ip, _this_storage.servers[i]->port);
  fflush(stdout);

  if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || send(socks.sock_fd[i], &_this_storage, sizeof(struct storage_info), 0) < 0)
    return handle_errors(socks, "bl");

  printf("connected to server: %s:%d.\n", _this_storage.servers[i]->server_ip, _this_storage.servers[i]->port);
  close(socks.sock_fd[0]);
  close(socks.sock_fd[1]);
  return 0;
}*/

static int lux_getattr(const char *path, struct stat *stbuf)
{
  log_server_call(path, &_this_storage, GETATTR);
  int sock_fd = get_live_server_fd();

  memset(stbuf, 0, sizeof(struct stat));

  struct raid_one_input input = generate_server_input(GETATTR, path, NULL, 0, 0, 0, 0, NULL, 0);

  printf("attemting (getattr) contact with server for path %s\n", path);
  fflush(stdout);

  struct raid_one_response response;

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, &response, sizeof(struct raid_one_response), 0) < 0)
    return handle_error(sock_fd, path, GETATTR);

  printf("status %d for %s\n", response.error, input.path);
  fflush(stdout);

  memcpy(stbuf, &response.one_stat, sizeof(struct stat));

  return handle_return(sock_fd, response.error, path, GETATTR);
}

static int lux_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  log_server_call(path, &_this_storage, READDIR);
  (void)offset;
  (void)fi;

  int sock_fd = get_live_server_fd();

  struct raid_one_input input = generate_server_input(READDIR, path, NULL, 0, 0, 0, 0, NULL, 0);

  printf("attemting (readdir) contact with server for path %s\n", path);
  fflush(stdout);

  struct raid_one_directories_response response;

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, &response, sizeof(struct raid_one_directories_response), 0) < 0)
    return handle_error(sock_fd, path, READDIR);

  printf("readdir returned.\n");
  fflush(stdout);

  for (int i = 0; i < response.size; i++)
  {
    filler(buf, response.filenames[i], &response.stats[i], 0);
  }

  return handle_return(sock_fd, response.error, path, READDIR);
}

int copy_file(struct lux_server *from, struct lux_server *to, const char path[256])
{
  struct raid_one_input input_getattr = generate_server_input(GETATTR, path, NULL, 0, 0, 0, 0, NULL, 0);
  struct raid_one_response response;

  int sock_fd_from = get_server_fd(from);

  if (sock_fd_from == -1)
  {
    printf("fatal connection failure. closing application.\n");
    exit(0);
  }
  if (send(sock_fd_from, &input_getattr, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_from, &response, sizeof(struct raid_one_response), 0) < 0)
    return handle_error(sock_fd_from, path, GETATTR);

  close(sock_fd_from);

  struct raid_one_input input_mknod = generate_server_input(CREATE, path, NULL, 0, 0, response.one_stat.st_mode, response.one_stat.st_dev, NULL, 0);

  int sock_fd_to = get_server_fd(to);

  if (sock_fd_to == -1)
  {
    printf("fatal connection failure. closing application.\n");
    exit(0);
  }
  if (send(sock_fd_to, &input_mknod, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_to, &response, sizeof(struct raid_one_response), 0) < 0)
    return handle_error(sock_fd_to, path, CREATE);

  close(sock_fd_to);

  struct raid_one_input input_trunc = generate_server_input(TRUNCATE, path, NULL, 0, 0, 0, 0, NULL, 0);

  sock_fd_to = get_server_fd(to);

  if (sock_fd_to == -1)
  {
    printf("fatal connection failure. closing application.\n");
    exit(0);
  }
  if (send(sock_fd_to, &input_trunc, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_to, &response, sizeof(struct raid_one_response), 0) < 0)
    return handle_error(sock_fd_to, path, CREATE);

  close(sock_fd_to);

  off_t f_size = response.one_stat.st_size;
  off_t offse = 0;

  char data_buffer[4096];

  while (f_size > 0)
  {
    size_t req_size = 4096;
    struct raid_one_input input = generate_server_input(READ, path, NULL, offse, req_size, 0, 0, NULL, 0);
    int sock_fd_from = get_server_fd(from);

    if (sock_fd_from == -1)
    {
      printf("fatal connection failure. closing application.\n");
      exit(0);
    }
    if (send(sock_fd_from, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_from, data_buffer, req_size, 0) < 0)
      return handle_error(sock_fd_from, path, READ);

    close(sock_fd_from);

    req_size = (f_size > 4096) ? 4096 : f_size;
    input = generate_server_input(WRITE, path, NULL, offse, req_size, 0, 0, NULL, 0);
    struct raid_one_response response;
    int sock_fd_to = get_server_fd(to);

    if (sock_fd_to == -1)
    {
      printf("fatal connection failure. closing application.\n");
      exit(0);
    }
    if (send(sock_fd_to, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd_to, &response, sizeof(struct raid_one_response), 0) < 0 || send(sock_fd_to, data_buffer, req_size, 0) < 0)
      return handle_error(sock_fd_to, path, WRITE);

    f_size -= 4096;
    offse += 4096;
    close(sock_fd_to);
  }
  return 0;
}

static int lux_open(const char *path, struct fuse_file_info *fi)
{
  log_server_call(path, &_this_storage, OPEN);
  int ret_stat = increment_pending_number(path);
  if (ret_stat)
    return -EAGAIN;
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(OPEN, path, NULL, 0, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (open) contact with server for path %s\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0)
      return handle_errors(socks, path);
  }

  int hash_check_one = memcmp(responses[0].checked_hash, responses[0].recorded_hash, 16 * sizeof(unsigned char));
  int hash_check_two = memcmp(responses[1].checked_hash, responses[1].recorded_hash, 16 * sizeof(unsigned char));
  if ((responses[0].error != 0 || hash_check_one != 0) && hash_check_two == 0)
  {
    copy_file(_this_storage.servers[1], _this_storage.servers[0], path);
  }
  else if ((responses[1].error != 0 || hash_check_one == 0) && hash_check_two != 0)
  {
    copy_file(_this_storage.servers[0], _this_storage.servers[1], path);
  }

  return handle_returns(socks, responses, path);
}

static int lux_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  log_server_call(path, &_this_storage, READ);
  (void)fi;

  int sock_fd = get_live_server_fd();

  struct raid_one_input input = generate_server_input(READ, path, NULL, offset, size, 0, 0, NULL, 0);

  printf("attemting (read) contact with server for path %s for %zu bytes.\n", path, size);
  fflush(stdout);

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, buf, size, 0) < 0)
  {
    return handle_error(sock_fd, path, READ);
  }

  close(sock_fd);
  return size;
}

static int lux_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
  log_server_call(path, &_this_storage, WRITE);
  int ret_stat = increment_pending_number(path);
  if (ret_stat)
    return -EAGAIN;
  (void)fi;

  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(WRITE, path, NULL, offset, size, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (write) contact with server for path %s for %zu bytes.\n", path, size);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0 || send(socks.sock_fd[i], buf, input.size, 0) < 0)
      return handle_errors(socks, path);
  }
  int error = handle_returns(socks, responses, path);
  return error == 0 ? size : error;
}

static int lux_access(const char *path, int flags)
{
  log_server_call(path, &_this_storage, ACCESS);
  struct raid_one_input input = generate_server_input(ACCESS, path, NULL, 0, 0, 0, 0, NULL, flags);

  int sock_fd = get_live_server_fd();

  printf("attemting (access) contact with server for path %s\n", path);
  fflush(stdout);

  struct raid_one_response response;

  if (send(sock_fd, &input, sizeof(struct raid_one_input), 0) < 0 || recv(sock_fd, &response, sizeof(struct raid_one_response), 0) < 0)
  {
    return handle_error(sock_fd, path, ACCESS);
  }

  return handle_return(sock_fd, response.error, path, ACCESS);
}

static int lux_truncate(const char *path, off_t size)
{
  log_server_call(path, &_this_storage, TRUNCATE);
  int ret_stat = increment_pending_number(path);
  if (ret_stat)
    return -EAGAIN;
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(TRUNCATE, path, NULL, size, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (truncate) contact with server for path %s for %zu bytes.\n", path, size);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0)
      return handle_errors(socks, path);
  }

  return handle_returns(socks, responses, path);
}

static int lux_rename(const char *old, const char *new)
{
  log_server_call(old, &_this_storage, RENAME);
  int ret_stat = increment_pending_number(old);
  if (ret_stat)
    return -EAGAIN;
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(RENAME, old, new, 0, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (rename) contact with server for path %s for bytes.\n", old);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0)
      return handle_errors(socks, old);
  }

  return handle_returns(socks, responses, old);
}

static int lux_unlink(const char *path)
{
  log_server_call(path, &_this_storage, UNLINK);
  int ret_stat = increment_pending_number(path);
  if (ret_stat)
    return -EAGAIN;
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(UNLINK, path, NULL, 0, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (unlink) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0)
      return handle_errors(socks, path);
  }

  return handle_returns(socks, responses, path);
}

static int lux_rmdir(const char *path)
{
  log_server_call(path, &_this_storage, RMDIR);
  int ret_stat = increment_pending_number(path);
  if (ret_stat)
    return -EAGAIN;
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(RMDIR, path, NULL, 0, 0, 0, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (rmdir) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0)
      return handle_errors(socks, path);
  }

  return handle_returns(socks, responses, path);
}

static int lux_mknod(const char *path, mode_t mode, dev_t dev)
{
  log_server_call(path, &_this_storage, CREATE);
  int ret_stat = increment_pending_number(path);
  if (ret_stat)
    return -EAGAIN;
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(CREATE, path, NULL, 0, 0, mode, dev, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (mknod) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0)
      return handle_errors(socks, path);
  }

  return handle_returns(socks, responses, path);
}

static int lux_mkdir(const char *path, mode_t mode)
{
  log_server_call(path, &_this_storage, MKDIR);
  int ret_stat = increment_pending_number(path);
  if (ret_stat)
    return -EAGAIN;
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(MKDIR, path, NULL, 0, 0, mode, 0, NULL, 0);

  struct raid_one_response responses[2];

  printf("attemting (mkdir) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0)
      return handle_errors(socks, path);
  }

  return handle_returns(socks, responses, path);
}

static int lux_utimens(const char *path, const struct timespec tv[2])
{
  log_server_call(path, &_this_storage, UTIMENS);
  int ret_stat = increment_pending_number(path);
  if (ret_stat)
    return -EAGAIN;
  server_sockets socks = get_live_sockets(_this_storage.servers);

  struct raid_one_input input = generate_server_input(UTIMENS, path, NULL, 0, 0, 0, 0, tv, 0);

  struct raid_one_response responses[2];

  printf("attemting (utimens) contact with server for path %s for bytes.\n", path);
  fflush(stdout);
  for (int i = 0; i < socks.count; i++)
  {
    if (send(socks.sock_fd[i], &input, sizeof(struct raid_one_input), 0) < 0 || recv(socks.sock_fd[i], &responses[i], sizeof(struct raid_one_response), 0) < 0)
      return handle_errors(socks, path);
  }

  return handle_returns(socks, responses, path);
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
  for (int i = 0; i < 2; i++)
  {
    monitor_server(i);
  }
  sem_init(&replication_defender, 0, 1);
  pending_requests = 0;
  copier_demands_freedom = 0;
  char **args = malloc(3 * sizeof(char *));
  args[0] = strdup("useless");
  args[1] = _this_storage.mountpoint;
  args[2] = strdup("-f");
  //args[3] = strdup("-s"); // not needed in vagrant
  args[3] = NULL;
  return fuse_main(3, args, &hello_oper, NULL);
}
