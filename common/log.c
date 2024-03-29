/*
 * CS472 HW 3
 * Jason R. Carrete
 * log.c
 *
 * This module implements a logging interface for use by other modules. This
 * module must be initialized with a filepath before it can function properly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "log.h"

static FILE *logfile;

/* Clean up any resources used. */
static void logdeinit(void) {
    fclose(logfile);
}

/* Initialize the logging subsystem. */
void loginit(const char *const path) {
    logfile = fopen(path, "a");
    if (!logfile) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    if (atexit(logdeinit)) {
        logwarn("Log file will not be closed on exit");
    }
}

/* Execute immediately before each log message. */
static void prefix(void) {
    const time_t t = time(NULL);
    if (t == (time_t) -1) {
        fprintf(logfile, "Unknown time ");
    } else {
        char *out = ctime(&t);
        /* Remove trailing newline */
        out[strlen(out)-1] = '\0';
        fprintf(logfile, "%s ", out);
    }
}

/* Execute immediately after each log message. */
static void suffix(void) {
    fprintf(logfile, "\n");
    fflush(logfile);
}

/* Log an information message. */
void loginfo(const char *fmt, ...) {
    prefix();
    fputs("[INFO] ", logfile);
    va_list args;
    va_start(args, fmt);
    vfprintf(logfile, fmt, args);
    va_end(args);
    suffix();
}

/* Log a warning message. */
void logwarn(const char *fmt, ...) {
    prefix();
    fputs("[WARN] ", logfile);
    va_list args;
    va_start(args, fmt);
    vfprintf(logfile, fmt, args);
    va_end(args);
    suffix();
}

/* Log an error message. */
void logerr(const char *fmt, ...) {
    prefix();
    fputs("[ERROR] ", logfile);
    va_list args;
    va_start(args, fmt);
    vfprintf(logfile, fmt, args);
    va_end(args);
    suffix();
}
