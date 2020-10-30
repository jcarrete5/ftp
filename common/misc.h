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

char *addrtostr(struct sockaddr_storage *const addr);
int getchar_from_sock(int sockfd, struct sockbuf *buf);
int bsearch_strcmp(const void *s1, const void *s2);

#endif /* COMMON_MISC_H */
