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
#include "lux_fuse.c"
#include "logger.h"

void config_servers(struct storage_info *storage)
{
  for (int i = 0; i < storage->server_count; i++)
  {
    struct sockaddr_in server_adress;

    server_adress.sin_family = AF_INET;
    server_adress.sin_port = storage->servers[i]->port;
    server_adress.sin_addr.s_addr = inet_addr(storage->servers[i]->server_ip);
    storage->servers[i]->server_adress = server_adress;
  }
  struct sockaddr_in server_adress;

  server_adress.sin_family = AF_INET;
  server_adress.sin_port = storage->hotswap->port;
  server_adress.sin_addr.s_addr = inet_addr(storage->hotswap->server_ip);
  storage->hotswap->server_adress = server_adress;
}

void print_client_info()
{
  printf("=====================================\n");
  printf("%d storages.\n", _lux_client_info.storage_count);
  printf("cache size: %lld.\n", _lux_client_info.cache_size);
  printf("path to errorlog: %s.\n", _lux_client_info.path_to_error_log);
  printf("timeout: %d seconds.\n", _lux_client_info.timeout);
  for (int i = 0; i < _lux_client_info.storage_count; i++)
  {
    printf("-------------------------------------\n");
    struct storage_info *st = _lux_client_info.storages[i];
    printf("name %s\n", st->storage_name);
    printf("raid %d\n", st->raid);
    printf("%d servers\n", st->server_count);
    printf("mountpoint %s\n", st->mountpoint);
    for (int j = 0; j < st->server_count; j++)
    {
      printf("%s:%d\n", st->servers[j]->server_ip, htons(st->servers[j]->port));
    }
    printf("hotswap %s:%d\n", st->hotswap->server_ip, htons(st->hotswap->port));
    printf("-------------------------------------\n");
  }
  printf("=====================================\n");
}

int parse_config_file(int config_fd)
{
  char whole_config[4096];
  char delims[9] = " \t\n\v\f\r:=";
  if (read(config_fd, &whole_config, 4096) <= 0)
  {
    printf("Could not parse the config file or empty");
  }
  char *token = strtok(whole_config, delims);
  int current_storage_index = 0;
  struct storage_info *current_storage = NULL;
  while (token != NULL)
  {
    if (strcmp(token, "errorlog") == 0)
    {
      token = strtok(NULL, delims);
      strcpy(_lux_client_info.path_to_error_log, token);
    }
    else if (strcmp(token, "cache_size") == 0)
    {
      token = strtok(NULL, delims);
      _lux_client_info.cache_size = atoll(token);
    }
    else if (strcmp(token, "timeout") == 0)
    {
      token = strtok(NULL, delims);
      _lux_client_info.timeout = atoi(token);
    }
    else if (strcmp(token, "diskname") == 0)
    {
      current_storage = malloc(sizeof(struct storage_info));
      _lux_client_info.storages[current_storage_index] = current_storage;
      current_storage_index++;
      token = strtok(NULL, delims);
      strcpy(current_storage->storage_name, token);
    }
    else if (strcmp(token, "mountpoint") == 0)
    {
      token = strtok(NULL, delims);
      strcpy(current_storage->mountpoint, token);
    }
    else if (strcmp(token, "raid") == 0)
    {
      token = strtok(NULL, delims);
      if (strcmp(token, "1") == 0)
        current_storage->raid = RAID_ONE;
      else if (strcmp(token, "5") == 0)
        current_storage->raid = RAID_FIVE;
    }
    else if (strcmp(token, "servers") == 0)
    {
      struct lux_server *cur_server = NULL;
      int server_count = 0;
      token = strtok(NULL, delims);
      while (isdigit(token[0]))
      {
        cur_server = malloc(sizeof(struct lux_server));
        strcpy(cur_server->server_ip, token);
        cur_server->port = htons(atoi(strtok(NULL, delims)));
        cur_server->status = STATUS_ALIVE;
        current_storage->servers[server_count] = cur_server;
        server_count++;
        token = strtok(NULL, delims);
      }
      current_storage->server_count = server_count;
      continue;
    }
    else if (strcmp(token, "hotswap") == 0)
    {
      struct lux_server *hotswap_server = NULL;
      token = strtok(NULL, delims);
      hotswap_server = malloc(sizeof(struct lux_server));
      strcpy(hotswap_server->server_ip, token);
      hotswap_server->port = htons(atoi(strtok(NULL, delims)));
      current_storage->hotswap = hotswap_server;
    }
    token = strtok(NULL, delims);
  }
  _lux_client_info.storage_count = current_storage_index;
  return 0;
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    printf("usage ./lux_client /path/to/config\n");
    return -1;
  }
  char *path_to_config = argv[1];
  printf("Attempting to open config file at: %s.\n", path_to_config);
  int config_fd = open(path_to_config, O_RDONLY);
  if (config_fd == -1 || S_ISDIR(config_fd))
  {
    printf("No such file found.\n");
    return -1;
  }

  parse_config_file(config_fd);
  printf("Config file parsed.\n");
  logger_init(_lux_client_info.path_to_error_log);

  log_general("Config file parse.");

  for (int i = 0; i < _lux_client_info.storage_count; i++)
  {
    if (fork() == 0)
    {
      if (_lux_client_info.storages[i]->raid == RAID_ONE)
      {
        config_servers(_lux_client_info.storages[i]);
        return run_storage_raid_one(_lux_client_info.storages[i]);
      }
      else if (_lux_client_info.storages[i]->raid == RAID_FIVE)
      {
      }
      break;
    }
    else
    {
      continue;
    }
  }

  print_client_info();
}