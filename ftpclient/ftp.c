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
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <regex.h>
#include <errno.h>
#include <string.h>

#include "ftp.h"
#include "log.h"
#include "repl.h"
#include "vector.h"

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
static void parse_multi_line_reply(struct vector *reply_msg, int sockfd, enum reply_code code) {
    regex_t pattern_buf;
    char pattern[64];  /* Generous considering code will only be 3 characters */
    sprintf(pattern, "\n%d .*\r$", code);
    if (regcomp(&pattern_buf, pattern, REG_EXTENDED | REG_NOSUB)) {
        fputs(FTPC_EXE_NAME": Invalid regex", stderr);
        exit(EXIT_FAILURE);
    }
    char ch;
    while (true) {
        ch = getchar_from_sock(sockfd);
        if (ch == '\n') {
            /* Check buffered message for the appropriate end sequence */
            vector_append(reply_msg, '\0');
            if (!regexec(&pattern_buf, reply_msg->arr, 0, NULL, 0)) {
                /* We've found the end sequence; replace '\r' with '\0' */
                (reply_msg->size) -= 2;
                vector_append(reply_msg, '\0');
                break;
            }
            (reply_msg->size)--;
        }
        vector_append(reply_msg, ch);
    }
    regfree(&pattern_buf);
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
            break;
        }
        vector_append(reply_msg, ch);
    }
}

/*
 * Wait for a reply from the server and return the reply code. Optionally, a
 * struct vector may be passed in to get the reply message text.
 */
enum reply_code wait_for_reply(const int sockfd, struct vector *out_msg) {
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
        parse_multi_line_reply(&reply_msg, sockfd, code);
    } else {
        parse_single_line_reply(&reply_msg, sockfd);
    }
    loginfo("Received: %u %s", code, reply_msg.arr);
    if (out_msg) {
        vector_append_str(out_msg, reply_msg.arr);
        vector_append(out_msg, '\0');
    }
    vector_free(&reply_msg);
    /* Handle FTP_SERVER_NA here since this should always mean the client
       should quit */
    if (code == FTP_SERVER_NA) {
        puts("Server not available or is shutting down");
        exit(EXIT_SUCCESS);
    }
    return code;
}

/* Send a USER request and await a reply. */
enum reply_code ftp_USER(int sockfd, const char *username) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    vector_append_str(&msg, "USER ");
    vector_append_str(&msg, username);
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}

/* Send a PASS request and await a reply. */
enum reply_code ftp_PASS(int sockfd, const char *password) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    vector_append_str(&msg, "PASS ");
    vector_append_str(&msg, password);
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}

/* Send a QUIT request and await a reply. */
enum reply_code ftp_QUIT(int sockfd) {
    char msg[] = "QUIT\r\n";
    loginfo("Sent: QUIT");
    send(sockfd, msg, strlen(msg), 0);
    return wait_for_reply(sockfd, NULL);
}

/* Send a HELP request and await a reply. */
enum reply_code ftp_HELP(int sockfd, const char *cmd, struct vector *reply_msg) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    if (cmd) {
        vector_append_str(&msg, "HELP ");
        vector_append_str(&msg, cmd);
    } else {
        vector_append_str(&msg, "HELP");
    }
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, reply_msg);
}

/* Send a PWD request and await a reply. */
enum reply_code ftp_PWD(int sockfd, struct vector *reply_msg) {
    assert(reply_msg);
    char msg[] = "PWD\r\n";
    loginfo("Sent: PWD");
    send(sockfd, msg, strlen(msg), 0);
    return wait_for_reply(sockfd, reply_msg);
}

/* Send a SYST request and await a reply. */
enum reply_code ftp_SYST(int sockfd, struct vector *reply_msg) {
    assert(reply_msg);
    char msg[] = "SYST\r\n";
    loginfo("Sent: SYST");
    send(sockfd, msg, strlen(msg), 0);
    return wait_for_reply(sockfd, reply_msg);
}

/* Send a LIST request and await a reply. */
enum reply_code ftp_LIST(int sockfd, const char *path, struct vector *reply_msg) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    if (path) {
        vector_append_str(&msg, "LIST ");
        vector_append_str(&msg, path);
    } else {
        vector_append_str(&msg, "LIST");
    }
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, reply_msg);
}

/* Send a CWD request and await a reply. path must not be NULL. */
enum reply_code ftp_CWD(int sockfd, const char *path) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    vector_append_str(&msg, "CWD ");
    vector_append_str(&msg, path);
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}

/* Send a PORT request and await a reply. */
enum reply_code ftp_PORT(int sockfd, const char *ipstr, uint16_t port) {
    char ipstr_copy[strlen(ipstr) + 1];
    strcpy(ipstr_copy, ipstr);
    struct vector msg;
    vector_create(&msg, 128, 2);
    vector_append_str(&msg, "PORT ");
    char *token = strtok(ipstr_copy, ".");
    for (; token; token = strtok(NULL, ".")) {
        vector_append_str(&msg, token);
        vector_append(&msg, ',');
    }
    char port_str[16];
    sprintf(port_str, "%"PRIu16",%"PRIu16, port & (uint16_t)0xff00, port & (uint16_t)0x00ff);
    vector_append_str(&msg, port_str);
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}

/* Send a PASV request and await a reply. */
enum reply_code ftp_PASV(int sockfd) {
    char msg[] = "PASV\r\n";
    loginfo("Sent: PASV");
    send(sockfd, msg, strlen(msg), 0);
    return wait_for_reply(sockfd, NULL);
}

/* Send a RETR request and await a reply. path must not be NULL. */
enum reply_code ftp_RETR(int sockfd, const char *path) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    vector_append_str(&msg, "RETR ");
    vector_append_str(&msg, path);
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}

/* Send a STOR request and await a reply. path must not be NULL. */
enum reply_code ftp_STOR(int sockfd, const char *path) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    vector_append_str(&msg, "STOR ");
    vector_append_str(&msg, path);
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}

enum reply_code ftp_EPRT(int sockfd, int family, const char *ipstr, const uint16_t port) {
    struct vector msg;
    vector_create(&msg, 128, 2);
    char family_str[2], port_str[6];
    sprintf(family_str, "%d", family);
    sprintf(port_str, "%"PRIu16, port);
    vector_append_str(&msg, "EPRT |");
    vector_append_str(&msg, family_str);
    vector_append(&msg, '|');
    vector_append_str(&msg, ipstr);
    vector_append(&msg, '|');
    vector_append_str(&msg, port_str);
    vector_append(&msg, '|');
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}

enum reply_code ftp_EPSV(int sockfd, const uint16_t port) {
    struct vector msg;
    vector_create(&msg, 64, 2);
    char port_str[6];
    sprintf(port_str, "%"PRIu16, port);
    vector_append_str(&msg, "EPSV ");
    vector_append_str(&msg, port_str);
    vector_append(&msg, '\0');
    loginfo("Sent: %s", msg.arr);
    msg.size--;
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}
