/*
 * CS472 HW 2
 * Jason R. Carrete
 * client.c
 *
 * This module is the main entry point for the application. It handles parsing
 * of command-line arguments.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "config.h"

/* Prints usage information about how to invoke the application. */
void usage() {
    fputs("usage: "FTPC_EXE_NAME" HOSTNAME LOGFILE [PORT]\n"
          "    HOSTNAME - FQDN or IP address of the remote host to connect to\n"
          "    LOGFILE - Path to output log information to\n"
          "    PORT - [0, 65535] Port to connect to\n",
          stderr);
}

/* Validates the specified port or quits with an error. */
uint16_t valid_port(const char *port) {
    /* Port must only contain digits */
    const char *ptr = port;
    while (*ptr) {
        if (!isdigit(*ptr)) {
            fputs(FTPC_EXE_NAME": Invalid port\n", stderr);
            exit(EXIT_FAILURE);
        }
        ++ptr;
    }
    /* Ensure port is within range */
    errno = 0;
    long val = strtol(port, NULL, 10);
    if (errno == ERANGE || val < 0 || val > 65535) {
        fputs(FTPC_EXE_NAME": Port out of range\n", stderr);
        exit(EXIT_FAILURE);
    }
    return (uint16_t)val;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fputs(FTPC_EXE_NAME": Not enough arguments\n", stderr);
        usage();
        return EXIT_FAILURE;
    }
    const char *hostname = argv[1];
    const char *logfilename = argv[2];
    const uint16_t port = argc > 3 ? valid_port(argv[3]) : FTPC_DEFAULT_PORT;
    return EXIT_SUCCESS;
}
