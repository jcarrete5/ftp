/*
 *
 */

#ifndef FTPS_CLIENT_H
#define FTPS_CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>

void handle_new_client(const int sockpi,
                       struct sockaddr_storage *peeraddr,
                       socklen_t peeraddrlen);

#endif /* FTPS_CLIENT_H */
