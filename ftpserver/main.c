/*
 * CS472 HW 3
 * Jason R. Carrete
 * main.c
 *
 * This module is the main entry point for the server. It handles parsing
 * command-line arguments and accepts incoming connections.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "log.h"
#include "misc.h"
#include "client.h"
#include "auth.h"
#include "cfgparse.h"

#include <openssl/ssl.h>

/* True while the server is accepting connections. Set to false to stop. */
static bool accept_connections = true;

/* Print usage information. */
static void usage(void) {
    fputs("usage: "FTPS_EXE_NAME" LOGFILE PORT\n"
          "    LOGFILE - Path to output log information to\n"
          "    PORT - Port number to listen for connections on\n",
          stderr);
}

/* Validates and returns the specified port or quits with an error. */
static uint16_t valid_port(const char *port) {
    if (!port) {
        fputs(FTPS_EXE_NAME": Invalid port\n", stderr);
        exit(EXIT_FAILURE);
    }
    /* Port must only contain digits */
    for (const char *p = port; *p; p++) {
        if (!isdigit(*p)) {
            fputs(FTPS_EXE_NAME": Invalid port\n", stderr);
            exit(EXIT_FAILURE);
        }
    }
    /* Ensure port is within range */
    errno = 0;
    long val = strtol(port, NULL, 10);
    if (val < 0 || val > 65535) {
        if (errno == ERANGE) {
            perror(FTPS_EXE_NAME": Port out of range");
        } else {
            fputs(FTPS_EXE_NAME": Port out of range", stderr);
        }
        exit(EXIT_FAILURE);
    }
    return val;
}

/* Handler for keyboard interrupt. */
static void handle_SIGINT(__attribute__((unused)) int signum) {
    loginfo("Main: keyboard interrupt");
    accept_connections = false;
}

/* Wait for exiting child processes. */
static void handle_SIGCHLD(__attribute__((unused)) int signum) {
    loginfo("Main: got SIGCHLD");
    if (wait(NULL) == -1) {
        logwarn("Main: failed to wait for a child (wait: %s)", strerror(errno));
    }
}

/* Setup signal handlers. */
static void setup_sighandlers(void) {
    struct sigaction sigint = {0};
    sigint.sa_handler = handle_SIGINT;
    sigaction(SIGINT, &sigint, NULL);
    struct sigaction sigchld = {0};
    sigchld.sa_handler = handle_SIGCHLD;
    sigaction(SIGCHLD, &sigchld, NULL);
}

/* Start the server listening for connections on `port`. */
static void start_server(const uint16_t port) {
    /* Create listen socket */
    int socklisten = socket(AF_INET, SOCK_STREAM, 0);
    if (socklisten < 0) {
        logerr("Main: failed to create socket (socket: %s)", strerror(errno));
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in listen_addr = {0};
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(port);
    if (bind(socklisten, (struct sockaddr *)&listen_addr, sizeof listen_addr) < 0) {
        logerr("Main: error binding socket (bind: %s)", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (listen(socklisten, 10) < 0) {
        logerr("Main: error listening on socket (listen: %s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Wait for connections */
    loginfo("Main: now accepting connections");
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof addr;
    pid_t pid = 1;  /* Set to 1 so the loop continues in case of initial error */
    int sockclient, conn_count = 0;
    do {
        if ((sockclient=accept(socklisten, (struct sockaddr *)&addr, &addrlen)) == -1) {
            if (errno != EINTR) {
                logwarn("Main: error accepting a connection (accept: %s)", strerror(errno));
            }
        } else {
            loginfo("Main: accepted a connection from %s", addrtostr(&addr));
            conn_count++;
            if ((pid=fork()) == -1) {
                logwarn("Main: error forking a process for accepted connection "
                        "(fork: %s)",
                        strerror(errno));
            }
        }
    } while (accept_connections && pid);

    if (pid) /* in parent */ {
        close(socklisten);
        loginfo("Main: no longer accepting connections");
        /* TODO: Wait for children to end */
    } else  /* in child */ {
        handle_new_client(conn_count, sockclient);
    }
}

/* Main entry point. */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fputs(FTPS_EXE_NAME": Not enough arguments\n", stderr);
        usage();
        return EXIT_FAILURE;
    }
    const char *const logpathstr = argv[1];
    const char *const portstr = argv[2];
    uint16_t port = valid_port(portstr);
    loginit(logpathstr);
    setup_sighandlers();
    read_cfg();
    auth_read_passwd();
    start_server(port);
    return EXIT_SUCCESS;
}
