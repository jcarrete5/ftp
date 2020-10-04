#ifndef FTPC_LOG_H
#define FTPC_LOG_H

#include <stdio.h>

/* Initialize the logging subsystem with a logfile to append to. */
void loginit(FILE *const logfile);
void loginfo(const char *fmt, ...);
void logwarn(const char *fmt, ...);
void logerr(const char *fmt, ...);

#endif /* FTPC_LOG_H */
