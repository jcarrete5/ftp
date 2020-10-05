/*
 * CS472 HW 2
 * Jason R. Carrete
 * ftp.c
 *
 * This module implements FTP commands sent between the user-PI and the
 * server-PI. Each function is a wrapper around an FTP command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#include "ftp.h"
#include "log.h"
#include "repl.h"

/*
 * Returns a message buffer sized according to the send buffer size of the
 * socket.
 */
static unsigned int get_msgbuf(const int sockfd, char **buf) {
    unsigned int sndbuflen;
    socklen_t sz = sizeof sndbuflen;
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (void *) &sndbuflen, &sz) == -1) {
        sndbuflen = 4096;
    }
    *buf = (char *) calloc(sndbuflen, sizeof(char));
    if (!*buf) {
        logerr("Failed to allocate memory for msgbuf");
        longjmp(err_jmp_buf, FATAL_ERROR);
    }
    return sndbuflen;
}

void ftp_USER(int sockfd, const char *username) {
    char *msg = NULL;
    unsigned int bufsize = get_msgbuf(sockfd, &msg);
    if (strlen(username) + 7 > bufsize) {
        logerr("Username is too long");
        longjmp(err_jmp_buf, ILLEGAL_ARG);
    }
    sprintf(msg, "USER %s\r\n", username);
    send(sockfd, msg, strlen(msg), 0);
    free(msg);
}

void ftp_PASS(int sockfd, const char *password) {
    char *msg = NULL;
    unsigned int bufsize = get_msgbuf(sockfd, &msg);
    if (strlen(password) + 7 > bufsize) {
        logerr("Password is too long");
        longjmp(err_jmp_buf, ILLEGAL_ARG);
    }
    sprintf(msg, "PASS %s\r\n", password);
    send(sockfd, msg, strlen(msg), 0);
    free(msg);
}
