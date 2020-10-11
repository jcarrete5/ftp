/*
 * CS472 HW 2
 * Jason R. Carrete
 * util.h
 *
 * Header for util.c
 */

#ifndef FTPC_UTIL_H
#define FTPC_UTIL_H

#include <stddef.h>

struct sockbuf {
    char data[512];
    size_t i;
    size_t size;
};

int getchar_from_sock(int sockfd, struct sockbuf *buf);

#endif /* FTPC_UTIL_H */
