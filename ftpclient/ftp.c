/*
 * CS472 HW 2
 * Jason R. Carrete
 * ftp.c
 *
 * This module implements FTP commands sent between the user-PI and the
 * server-PI. Each function is a wrapper around an FTP command.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#include "ftp.h"
#include "log.h"
#include "repl.h"
#include "vector.h"

/*
 * TODO
 * MUST implement USER, PASS, CWD, QUIT, PASV, EPSV, PORT, EPRT, RETR, STOR,
 * PWD, SYST, LIST, and HELP commands
 */

/*
 * **Not thread-safe**
 * Get the next character from sockfd. Will terminate the application if the
 * socket is closed or there is a communication error on the between the PI.
 */
static char getchar_from_sock(int sockfd) {
    static char buf[1024];
    static size_t buf_i = 0;
    /* Number of contiguous data in buf 0-offset */
    static size_t size = 0;
    if (buf_i == size) {
        size = recv(sockfd, buf, sizeof buf, 0);
        buf_i = 0;
        if (size == 0) {
            loginfo("user-server PI connection closed by server");
            puts("Connection closed by server");
            exit(EXIT_SUCCESS);
        } else if (size == -1) {
            perror(FTPC_EXE_NAME": Error while reading socket");
            logerr("Error while reading data from user-PI socket");
            exit(EXIT_FAILURE);
        }
    }
    return buf[buf_i++];
}

/* Parse multi-line reply from FTP Server. */
static void parse_multi_line_reply(struct vector *vec, int sockfd) {
    /* TODO Implement parsing multi-line replies */
    logerr("Multi-line replies not yet implemented");
    fputs("Multi-line replies not yet implemented\n", stderr);
    exit(EXIT_FAILURE);
}

/* Parse single-line reply from FTP Server. */
static void parse_single_line_reply(struct vector *reply_msg, int sockfd) {
    char ch;
    while (true) {
        ch = getchar_from_sock(sockfd);
        if (ch == '\n' && reply_msg->arr[reply_msg->size-1] == '\r') {
            /* We've found "\r\n"! Replace '\r' with '\0' */
            reply_msg->size--;
            vector_append(reply_msg, '\0');
            return;
        }
        vector_append(reply_msg, ch);
    }
}

/* Wait for a reply from the server and return the reply code. */
enum reply_code wait_for_reply(const int sockfd) {
    char reply_code_buf[4];
    reply_code_buf[3] = '\0';
    struct vector reply_msg;
    vector_create(&reply_msg, 64, 2);
    /* Get first 3 characters to get reply code */
    for (int i = 0; i < 3; i++) {
        reply_code_buf[i] = getchar_from_sock(sockfd);
    }
    enum reply_code code = atoi(reply_code_buf);
    /* Parse reply text */
    if (getchar_from_sock(sockfd) == '-') {
        parse_multi_line_reply(&reply_msg, sockfd);
    } else {
        parse_single_line_reply(&reply_msg, sockfd);
    }
    loginfo("Received: %u %s", code, reply_msg.arr);
    vector_free(&reply_msg);
    return code;
}

/* Send a USER request and await a reply. */
enum reply_code ftp_USER(int sockfd, const char *username) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    vector_append_str(&msg, "USER ");
    vector_append_str(&msg, username);
    loginfo("Sent: %s", msg);
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd);
}

/* Send a PASS request and await a reply. */
enum reply_code ftp_PASS(int sockfd, const char *password) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    vector_append_str(&msg, "PASS ");
    vector_append_str(&msg, password);
    loginfo("Sent: %s", msg);
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd);
}
