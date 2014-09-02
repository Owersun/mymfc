#include <stdio.h>
#include <stdarg.h> 

#define LOGDEBUG   0
#define LOGINFO    1
#define LOGNOTICE  2
#define LOGWARNING 3
#define LOGERROR   4
#define LOGSEVERE  5
#define LOGFATAL   6
#define LOGNONE    7

class CLog
{
    public:
        static void Log(int loglevel, const char *format, ...);
};
