/*
 * CS472 HW 3
 * Jason R. Carrete
 * client.h
 *
 * Header for client.c
 */

#ifndef FTPS_CLIENT_H
#define FTPS_CLIENT_H

/* Start handling commands from a newly connected client. */
void handle_new_client(const int conn_id, const int sockpi);

#endif /* FTPS_CLIENT_H */
