/*
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"

void handle_new_client(const int sockpi,
                       struct sockaddr_storage *peeraddr,
                       socklen_t peeraddrlen) {
    close(sockpi);
    _exit(EXIT_SUCCESS);
}
