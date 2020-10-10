/*
 * CS472 HW 2
 * Jason R. Carrete
 * log.h
 *
 * Header for log.c
 */

#ifndef FTPC_LOG_H
#define FTPC_LOG_H

#include <stdio.h>

/*
 * Initialize the logging subsystem with a logfile to append to.
 *
 * Precondition: logfile must not be NULL
 */
void loginit(FILE *const logfile);

/*
 * Log an information message. fmt is specified just like it is for printf
 * family functions.
 *
 * Precondition: loginit has been called and initialized with a logfile
 */
void loginfo(const char *fmt, ...);

/*
 * Log an information message. fmt is specified just like it is for printf
 * family functions.
 *
 * Precondition: loginit has been called and initialized with a logfile
 */
void logwarn(const char *fmt, ...);

/*
 * Log an information message. fmt is specified just like it is for printf
 * family functions.
 *
 * Precondition: loginit has been called and initialized with a logfile
 */
void logerr(const char *fmt, ...);

#endif /* FTPC_LOG_H */
