/*
 * CS472 HW 2
 * Jason R. Carrete
 * log.h
 *
 * Header for log.c
 */

#ifndef COMMON_LOG_H
#define COMMON_LOG_H

/*
 * Initialize the logging subsystem with a path to a logfile.
 *
 * Precondition: path must not be NULL
 */
void loginit(const char *const path);

/*
 * Log an information message. fmt is specified just like it is for printf
 * family functions.
 *
 * Precondition: loginit has been called and initialized with a path to a logfile
 */
void loginfo(const char *fmt, ...);

/*
 * Log an information message. fmt is specified just like it is for printf
 * family functions.
 *
 * Precondition: loginit has been called and initialized with a path to a logfile
 */
void logwarn(const char *fmt, ...);

/*
 * Log an information message. fmt is specified just like it is for printf
 * family functions.
 *
 * Precondition: loginit has been called and initialized with a path to a logfile
 */
void logerr(const char *fmt, ...);

#endif /* COMMON_LOG_H */
