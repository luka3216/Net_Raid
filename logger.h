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

#include "logger.c"

int logger_init(char* path);
void log_general(char* msg);
void log_server_return(const char *path, struct storage_info* storage, int command, int error);
void log_server_call(const char *path, struct storage_info* storage, int command);



#endif // LOGGER_H
