#include "system.h"

void CLog::Log(int loglevel, const char *format, ... )
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    fprintf(stdout, "\n");
    va_end(argptr);
}