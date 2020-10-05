/*
 * CS472 HW 2
 * Jason R. Carrete
 * main.c
 *
 * This module is the main entry point for the application. It handles parsing
 * command-line arguments and hands off control to the REPL.
 */


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "repl.h"
#include "log.h"

/* Prints usage information about how to invoke the application. */
static void usage(void) {
    fputs("usage: "FTPC_EXE_NAME" HOSTNAME LOGFILE [PORT]\n"
          "    HOSTNAME - FQDN or IP address of the remote host to connect to\n"
          "    LOGFILE - Path to output log information to\n"
          "    PORT - [0, 65535] Port to connect to\n",
          stderr);
}

/*
 * Converts struct addrinfo to an ipv4 or ipv6 string. dst must be large enough
 * to store an ipv4 or ipv6 address. Returns true on success.
 */
static bool addrtostr(struct addrinfo *info, char *dst) {
    struct sockaddr_in *ipv4;
    struct sockaddr_in6 *ipv6;
    switch (info->ai_family) {
        case AF_INET:
            ipv4 = (struct sockaddr_in *) info->ai_addr;
            inet_ntop(AF_INET, &(ipv4->sin_addr), dst, INET_ADDRSTRLEN);
            return true;
        case AF_INET6:
            ipv6 = (struct sockaddr_in6 *) info->ai_addr;
            inet_ntop(AF_INET6, &(ipv6->sin6_addr), dst, INET6_ADDRSTRLEN);
            return true;
        default:
            strcpy(dst, "Unknown AF");
            return false;
    }
}

/*
 * Attempts to resolve a domain name to an FTP server. A linked-list of
 * addresses are written into out. If the domain cannot be resolved, quit the
 * application and print an error message.
 */
static void resolve_domain(const char *node, struct addrinfo **out) {
    struct addrinfo hints = {};
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(node, "ftp", &hints, out);
    if (err) {
        fprintf(stderr, FTPC_EXE_NAME": %s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }
}

/* Validates and returns the specified port or quits with an error. */
static uint16_t valid_port(const char *port) {
    if (!port) {
        fputs(FTPC_EXE_NAME": Invalid port\n", stderr);
        exit(EXIT_FAILURE);
    }
    /* Port must only contain digits */
    for (const char *p = port; *p; p++) {
        if (!isdigit(*p)) {
            fputs(FTPC_EXE_NAME": Invalid port\n", stderr);
            exit(EXIT_FAILURE);
        }
    }
    /* Ensure port is within range */
    errno = 0;
    long val = strtol(port, NULL, 10);
    if (val < 0 || val > 65535) {
        if (errno == ERANGE) {
            perror(FTPC_EXE_NAME": Port out of range");
        } else {
            fputs(FTPC_EXE_NAME": Port out of range", stderr);
        }
        exit(EXIT_FAILURE);
    }
    return val;
}

/*
 * Validates the path and returns a file pointer in append mode for that path.
 * Quits the program if the path cannot be opened.
 */
static FILE *valid_logfile(const char *path) {
    FILE *const logfile = fopen(path, "a");
    if (!logfile) {
        perror(FTPC_EXE_NAME": Error opening log file");
        exit(EXIT_FAILURE);
    }
    return logfile;
}

/*
 * Initialize connection between user-PI and server-PI. Then hand off control
 * to the REPL.
 */
static void init_conn(struct addrinfo *const addrlist) {
    /* Try to connect to a resolved address. Exit if all addresses fail. */
    for (struct addrinfo *info = addrlist; info; info = info->ai_next) {
        char ipstr[INET6_ADDRSTRLEN];
        addrtostr(info, ipstr);
        const int sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (sock == 0) {
            perror(FTPC_EXE_NAME": Failed to create socket");
            logerr("Failed to create socket for %s", ipstr);
        }
        loginfo("Trying %s", ipstr);
        if (connect(sock, info->ai_addr, info->ai_addrlen) == 0) {
            puts(FTPC_EXE_NAME" "FTPC_VERSION);
            loginfo("Connected to %s", ipstr);
            printf("Connected to %s\n", ipstr);
            freeaddrinfo(addrlist);
            /* Socket will be closed on exit in the repl */
            repl(sock);
            return;
        } else {
            logwarn("Failed to connect to %s; trying another address", ipstr);
        }
    }
    char msg[] = "Failed to connect to any address";
    logerr(msg);
    printf(FTPC_EXE_NAME": %s\n", msg);
    exit(EXIT_FAILURE);
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
    FILE *const logfile = valid_logfile(logfilename);
    /* logfile is closed in logging subsystem on exit */
    loginit(logfile);
    struct addrinfo *addrlist;
    resolve_domain(hostname, &addrlist);
    init_conn(addrlist);
    return EXIT_SUCCESS;
}
