#ifndef FTPC_LOG_H
#define FTPC_LOG_H

#include <stdio.h>

/* Initialize the logging subsystem with a logfile to append to. */
void loginit(FILE *const logfile);
void loginfo(const char *s);
void logwarn(const char *s);
void logerr(const char *s);

#endif /* FTPC_LOG_H */
