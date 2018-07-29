#if !defined(LUX_COMMON_H)
#define LUX_COMMON_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST 0
#define GETATTR 1
#define READDIR 2

struct raid_one_input
{
  int command;
  char path[256];
};

struct raid_one_response
{
  int status;
  char filenames[32][32];
  struct stat stats[32];
  struct stat one_stat;
  int size;
};

#endif // LUX_COMMON_H
