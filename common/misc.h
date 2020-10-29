/*
 *
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

#endif /* COMMON_MISC_H */
