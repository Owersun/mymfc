#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "Log.h"

#define LOGDEBUG   0
#define LOGINFO    1
#define LOGNOTICE  2
#define LOGWARNING 3
#define LOGERROR   4
#define LOGSEVERE  5
#define LOGFATAL   6
#define LOGNONE    7

#ifndef V4L2_CAP_VIDEO_M2M_MPLANE
  #define V4L2_CAP_VIDEO_M2M_MPLANE 0x00004000
#endif

#define memzero(x) memset(&(x), 0, sizeof (x))

#ifdef _DEBUG
  #define log(...) CLog::Log(__VA_ARGS__)
#else
  #define log(...)
#endif

