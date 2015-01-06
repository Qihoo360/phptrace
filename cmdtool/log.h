#ifndef PHPTRACE_LOG_H
#define PHPTRACE_LOG_H

#define MAX_LOGMSG_LEN 1024

#define LL_DEBUG   0
#define LL_INFO    1
#define LL_NOTICE  2
#define LL_ERROR   3

void log_level_set(int level);
int log_level_get();
void log_msg(int level, const char *msg);
void log_printf(int level, const char *fmt, ...);

#endif // PHPTRACE_LOG_H
