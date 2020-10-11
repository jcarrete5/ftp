/*
 * CS472 HW 2
 * Jason R. Carrete
 * util.c
 *
 * This module contains misc. function that are useful for the application.
 */

#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "util.h"
#include "log.h"

/*
 * Get a character from the socket file descriptor. buf is used to buffer any
 * extraneous data between calls; It will be exhausted before calling recv(2)
 * again. Returns 0 on EOF or -1 on error, otherwise, the character is returned.
 */
int getchar_from_sock(int sockfd, struct sockbuf *buf) {
    assert(buf);
    if (buf->i == buf->size) {
        ssize_t read = recv(sockfd, buf->data, sizeof buf->data, 0);
        buf->i = 0;
        if (read <= 0) {
            /* Signal exceptional case */
            return read;
        }
    }
    return buf->data[(buf->i)++];
}


