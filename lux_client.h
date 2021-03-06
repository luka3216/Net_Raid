#ifndef LUX_CLIENT_H
#define LUX_CLIENT_H
#define RAID_ONE 1
#define RAID_FIVE 5

struct lux_server {
  char server_ip[32];
  uint16_t port;
  time_t fail_time;
  int status;
  int socket_fd;
  struct sockaddr_in server_adress;
};

struct storage_info {
  char storage_name[256];
  char mountpoint[256];
  int raid;
  int server_count;
  struct lux_server* servers[24];
  struct lux_server* hotswap;
};

struct client_info {
  char path_to_error_log[256];
  unsigned long long cache_size;
  int cache_replacement;
  int timeout;
  int storage_count;
  struct storage_info* storages[24];

} _lux_client_info;

#endif