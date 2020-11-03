/*
 * CS472 HW 3
 * Jason R. Carrete
 * misc.h
 *
 * Header for misc.c
 */

#ifndef COMMON_MISC_H
#define COMMON_MISC_H

#include <stdint.h>

/* Stores data in between calls when getting data from a socket. */
struct sockbuf {
    uint8_t data[BUFSIZ];
    size_t i;
    size_t size;
};

/* Converts struct sockaddr_storage to an ipv4 or ipv6 string with port. */
char *addrtostr(struct sockaddr_storage *const addr);
/*
 * Get a character from the socket file descriptor. buf is used to buffer any
 * extraneous data between calls; It will be exhausted before calling recv(2)
 * again. Returns 0 on EOF or -1 on error, otherwise, the character is returned.
 */
int getchar_from_sock(int sockfd, struct sockbuf *buf);
/*
 * Wrapper around strcmp to be used in bsearch(3).
 * https://stackoverflow.com/a/15824981/2981420
 */
int bsearch_strcmp(const void *s1, const void *s2);

#endif /* COMMON_MISC_H */
