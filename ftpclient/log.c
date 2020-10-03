/*
 * CS472 HW 2
 * Jason R. Carrete
 * log.c
 *
 * This module implements a logging interface for use by other modules. This
 * module must be initialized with a log file before it can function properly.
 */

#include "config.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *logfile;

/* Print a message before any log messages get written. */
static void prolog() {
    fputs("~~~~~~~~~~ session start ~~~~~~~~~~\n", logfile);
    fputs(FTPC_EXE_NAME" "FTPC_VERSION"\n", logfile);
}

/*
 * Print a message when the logging subsystem is stopped i.e. when the
 * application stops.
 */
static void epilog() {
    fputs("~~~~~~~~~~~ session end ~~~~~~~~~~~\n\n", logfile);
}

/* Clean up any resources used. */
static void logdeinit() {
    fclose(logfile);
}

void loginit(FILE *const file) {
    if (file) {
        logfile = file;
    } else {
        fputs(FTPC_EXE_NAME": Failed to init logging; file is NULL", stderr);
    }
    prolog();
    atexit(logdeinit);
    atexit(epilog);
}

/* Prefix of all log messages. */
static void prefix() {
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

/* Suffix of all log messages. */
static void suffix() {
    fprintf(logfile, "\n");
    fflush(logfile);
}

void loginfo(const char *s) {
    prefix();
    fprintf(logfile, "[INFO] %s", s);
    suffix();
}

void logwarn(const char *s) {
    prefix();
    fprintf(logfile, "[WARN] %s", s);
    suffix();
}

void logerr(const char *s) {
    prefix();
    fprintf(logfile, "[ERROR] %s", s);
    suffix();
}
