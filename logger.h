#if !defined(LOGGER_H)
#define LOGGER_H
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <errno.h>

#define CONN_FAIL 1
#define CONN_SUCCESS 2
#define CONN_CONT_FAIL 3
#define CONN_RESTORED 4
#define CONN_COPYING 5
#define CONN_COPYING_COMPLETE 6
#define CONN_REPLACED 6
#define CONN_RESTORING 7
#define CONN_RESTORING_COMPLETE 8
#define CONN_FAIL_FINAL 9

#include "logger.c"

int logger_init(char* path);
void log_general(char* msg);
void log_server_return(const char *path, struct storage_info* storage, int command, int error);
void log_server_call(const char *path, struct storage_info* storage, int command);
void log_serv_info(int conn_stat, struct storage_info* storage, struct lux_server* server, int time_since_fail);



#endif // LOGGER_H
