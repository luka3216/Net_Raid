#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>

#include "lux_client.h"
#include "lux_common.h"

char log_path[256];

sem_t sema;

int log_msg(char *msg)
{
  size_t length = strlen(msg);

  sem_wait(&sema);
  int fd = open(log_path, O_APPEND | O_WRONLY);

  if (fd == -1)
  {
    sem_post(&sema);
    return -1;
  }

  if (write(fd, msg, length) == -1)
  {
    sem_post(&sema);
    return -1;
  }

  close(fd);
  sem_post(&sema);

  return 0;
}

void log_server_return(const char *path, struct storage_info *storage, int command, int error)
{
  char buff[512];
  //char buff2[128];
  time_t now;
  struct tm *ts;

  /* Get the current time */
  now = time(NULL);

  /* Format and print the time, "ddd yyyy-mm-dd hh:mm:ss zzz" */
  ts = localtime(&now);
  strftime(buff, sizeof(buff), "[%a %Y-%m-%d %H:%M:%S] ", ts);
  puts(buff);

  strcat(buff, storage->storage_name);

  strcat(buff, " ");

  strcat(buff, commands[command]);
  strcat(buff, " call for path ");

  strcat(buff, path);

  if (error == 0)
  {
    strcat(buff, " returned with success");
  }
  else
  {
    strcat(buff, " returned with error: ");
    strcat(buff, strerror(error));
  }

  /*
  strcat(buff, serv->server_ip);
  strcat(buff, ":");
  sprintf(buff2, "%ud", serv->port);

  strcat(buff, bumff2);*/
  strcat(buff, ".\n");

  log_msg(buff);
}

void log_server_call(const char *path, struct storage_info *storage, int command)
{
  char buff[512];
  //char buff2[128];
  time_t now;
  struct tm *ts;

  /* Get the current time */
  now = time(NULL);

  /* Format and print the time, "ddd yyyy-mm-dd hh:mm:ss zzz" */
  ts = localtime(&now);
  strftime(buff, sizeof(buff), "[%a %Y-%m-%d %H:%M:%S] ", ts);
  puts(buff);

  strcat(buff, storage->storage_name);

  strcat(buff, " ");

  strcat(buff, commands[command]);
  strcat(buff, " called for path ");

  strcat(buff, path);

  /*
  strcat(buff, serv->server_ip);
  strcat(buff, ":");
  sprintf(buff2, "%ud", serv->port);
  
  strcat(buff, buff2);*/
  strcat(buff, "\n");

  log_msg(buff);
}

void log_serv_info(int conn_stat, struct storage_info *storage, struct lux_server *server, int time_since_fail)
{
  char buff[512];
  char buff2[128];
  time_t now;
  struct tm *ts;

  /* Get the current time */
  now = time(NULL);

  /* Format and print the time, "ddd yyyy-mm-dd hh:mm:ss zzz" */
  ts = localtime(&now);
  strftime(buff, sizeof(buff), "[%a %Y-%m-%d %H:%M:%S] ", ts);
  puts(buff);

  strcat(buff, storage->storage_name);

  strcat(buff, " ");

  strcat(buff, server->server_ip);
  strcat(buff, ":");
  sprintf(buff2, "%u ", htons(server->port));

  strcat(buff, buff2);

  if (conn_stat == CONN_SUCCESS)
    strcat(buff, "server status check successful.");
  else if (conn_stat == CONN_CONT_FAIL)
  {
    sprintf(buff2, "server status check failing for %d seconds.", time_since_fail);
    strcat(buff, buff2);
  }
  else if (conn_stat == CONN_FAIL)
    strcat(buff, "server status check failed.");
  else if (conn_stat == CONN_FAIL_FINAL) {
    strcat(buff, "server downtime exceeds timeout limit.");
  }
  else if (conn_stat == CONN_RESTORED)
  {
    sprintf(buff2, "connection with server reestablished after %d seconds.", time_since_fail);
    strcat(buff, buff2);
  }
  else if (conn_stat == CONN_REPLACED)
  {
    strcat(buff, "server replaced with hotswap.");
  }
  else if (conn_stat == CONN_COPYING)
  {
    strcat(buff, "replicating data to hotswap.");
  }
  else if (conn_stat == CONN_COPYING_COMPLETE)
  {
    strcat(buff, "replication of data to hotswap complete.");
  }
  else if (conn_stat == CONN_RESTORING)
  {
    strcat(buff, "restoring data to server.");
  }
  else if (conn_stat == CONN_RESTORING_COMPLETE)
  {
    strcat(buff, "restoration of data to server complete.");
  }

  strcat(buff, "\n");

  log_msg(buff);
}

void log_general(char *msg)
{
  char buff[512];
  time_t now;
  struct tm *ts;

  /* Get the current time */
  now = time(NULL);

  /* Format and print the time, "ddd yyyy-mm-dd hh:mm:ss zzz" */
  ts = localtime(&now);
  strftime(buff, sizeof(buff), "[%a %Y-%m-%d %H:%M:%S] ", ts);
  puts(buff);

  strcat(buff, msg);

  strcat(buff, "\n");

  log_msg(buff);
}

int logger_init(char *path)
{
  strcpy(log_path, path);

  int fd = open(log_path, 0);
  if (fd == -1)
  {
    return -1;
  }

  close(fd);

  sem_init(&sema, 1, 1);

  return 0;
}