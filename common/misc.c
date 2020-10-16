/*
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
