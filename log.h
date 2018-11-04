#ifndef LOG_H
#define LOG_H

// Includes
#include <stdio.h>      // fopen, fclose, fprintf, vfprintf
#include <stdlib.h>     // exit
#include <time.h>       // time, localtime

// Defines
#define DIE(...) \
{ \
    fprintf(stderr, ##__VA_ARGS__); \
    fprintf(stderr, "\n"); \
    perror(__func__); \
    exit(1); \
}
#define LOG_ERROR   1
#define LOG_WARNING 2
#define LOG_INFO    3
#define LOG_DEBUG   4

// Globals
static const char* log_file;

// Methods
void dlog(int level, const char* fmt, ...)
{
    if (!log_file) return;
    va_list vargs;
    time_t now = time(0);
    struct tm* nowtm = localtime(&now);
    FILE* f = fopen(log_file, "a");
    if (nowtm == NULL) DIE("Failed to calculate time");
    if (f == NULL) DIE("Failed to open log file '%s'", log_file);

    fprintf(f, "%02d:%02d:%02d", nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec);
    switch (level)
    {
        case LOG_ERROR:   fprintf(f, " ERR: "); break;
        case LOG_WARNING: fprintf(f, " WRN: "); break;
        case LOG_INFO:    fprintf(f, " INF: "); break;
        default:          fprintf(f, " DBG: "); break;
    }
    vfprintf(f, fmt, vargs);
    fprintf(f, "\n");

    fclose(f);
}

#endif // LOG_H