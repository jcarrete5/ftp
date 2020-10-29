/*
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "misc.h"

/* Converts struct sockaddr_storage to an ipv4 or ipv6 string with port. */
char *addrtostr(struct sockaddr_storage *const addr) {
    static char str[INET6_ADDRSTRLEN+7];
    struct sockaddr_in *addrv4;
    struct sockaddr_in6 *addrv6;
    char *ptr = NULL;
    switch (addr->ss_family) {
        case AF_INET:
            addrv4 = (struct sockaddr_in *)addr;
            inet_ntop(AF_INET, &addrv4->sin_addr,
                      str, INET_ADDRSTRLEN);
            ptr = &str[strlen(str)];
            *(ptr++) = ':';
            sprintf(ptr, "%"PRIu16, ntohs(addrv4->sin_port));
            break;
        case AF_INET6:
            addrv6 = (struct sockaddr_in6 *)addr;
            inet_ntop(AF_INET6, &addrv6->sin6_addr,
                      str, INET6_ADDRSTRLEN);
            ptr = &str[strlen(str)];
            *(ptr++) = ':';
            sprintf(ptr, "%"PRIu16, ntohs(addrv6->sin6_port));
            break;
        default:
            strcpy(str, "Unknown AF");
            break;
    }
    return str;
}

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
        buf->size = read < 0 ? 0 : read;
        if (read <= 0) {
            /* Signal exceptional case */
            return read;
        }
    }
    return buf->data[(buf->i)++];
}
