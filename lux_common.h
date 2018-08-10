#if !defined(LUX_COMMON_H)
#define LUX_COMMON_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST 0
#define GETATTR 1
#define READDIR 2
#define OPEN 3
#define READ 4
#define ACCESS 5
#define WRITE 6
#define TRUNCATE 7
#define RENAME 8
#define UNLINK 9

struct raid_one_input
{
  int command;
  char path[256];
  char char_buf[256];
  off_t offset;
  size_t size;
  int flags;
};

struct raid_one_response
{
  int status;
  char filenames[32][32];
  struct stat stats[32];
  struct stat one_stat;
  int size;
  int error;
};

struct raid_one_file_response
{
  int status;
  int size;
  char buff[4088];
};

#endif // LUX_COMMON_H
