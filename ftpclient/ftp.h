/*
 * CS472 HW 2
 * Jason R. Carrete
 * ftp.h
 *
 * Header for ftp.c
 */

#ifndef FTPC_FTP_H
#define FTPC_FTP_H

#define FATAL_ERROR -1
#define ILLEGAL_ARG -2

void ftp_USER(int sockfd, const char *username);
void ftp_PASS(int sockfd, const char *password);

#endif /* FTPC_FTP_H */
