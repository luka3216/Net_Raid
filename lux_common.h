#if !defined(LUX_COMMON_H)
#define LUX_COMMON_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define DUMMY 0
#define GETATTR 1
#define READDIR 2
#define OPEN 3
#define READ 4
#define ACCESS 5
#define WRITE 6
#define TRUNCATE 7
#define RENAME 8
#define UNLINK 9
#define RMDIR 10
#define CREATE 11
#define UTIMENS 12
#define MKDIR 13
#define STATUS_ALIVE 0
#define STATUS_DEGRADED 1
#define STATUS_DEAD 2

const char commands[14][16] = {"dummy", "getattr", "readdir", "open", "read", "access", "write", "truncate", "rename", "unlink", "rmdir", "mknod", "utimens", "mkdir"};

typedef struct raid_one_live_sockets
{
  int count;
  int sock_fd[2];
} server_sockets;

struct raid_one_input
{
  int command;
  char path[256];
  char char_buf[256];
  off_t offset;
  size_t size;
  mode_t mode;
  dev_t dev;
  struct timespec spec[2];
  int flags;
};

struct raid_one_response
{
  struct stat one_stat;
  unsigned char recorded_hash[16];
  unsigned char checked_hash[16];
  int size;
  int error;
};

struct raid_one_directories_response
{
  char filenames[32][32];
  struct stat stats[32];
  int size;
  int error;
};

#endif // LUX_COMMON_H
