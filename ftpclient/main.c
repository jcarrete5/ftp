/*
 * CS472 HW 2
 * Jason R. Carrete
 * client.c
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

/* Prints usage information about how to invoke the application. */
static void usage() {
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
static bool iptostr(struct addrinfo *info, char *dst) {
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
    int err = getaddrinfo(node, "ftp", NULL, out);
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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fputs(FTPC_EXE_NAME": Not enough arguments\n", stderr);
        usage();
        return EXIT_FAILURE;
    }
    const char *hostname = argv[1];
    const char *logfilename = argv[2];
    const uint16_t port = argc > 3 ? valid_port(argv[3]) : FTPC_DEFAULT_PORT;
    struct addrinfo *addrlist;
    resolve_domain(hostname, &addrlist);
    for (struct addrinfo *p = addrlist; p; p = p->ai_next) {
        char ipstr[INET6_ADDRSTRLEN];
        iptostr(p, ipstr);
        puts(ipstr);
    }
    return EXIT_SUCCESS;
}
