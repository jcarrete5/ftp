/*
 * CS472 HW 2
 * Jason R. Carrete
 * ftp.c
 *
 * This module implements FTP commands sent between the user and the
 * server. Each function is a wrapper around an FTP command.
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
#include <arpa/inet.h>
#include <netdb.h>
#include <regex.h>
#include <errno.h>
#include <string.h>

#include "ftp.h"
#include "log.h"
#include "repl.h"
#include "vector.h"

static struct sockbuf pi_buf = {0};

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

/*
 * Wrapper function for getting characters from the PI.
 */
static char pi_getchar(int sockfd) {
    int ret = getchar_from_sock(sockfd, &pi_buf);
    if (ret == 0) {
        loginfo("user-server PI connection closed by server");
        puts("Connection closed by server");
        exit(EXIT_SUCCESS);
    } else if (ret < -1) {
        perror(FTPC_EXE_NAME": Error while reading socket");
        logerr("Error while reading data from user-PI socket");
        exit(EXIT_FAILURE);
    }
}

/* Set the socket address to connect to when the server is in passive mode. */
static void set_remote_sockaddr
(const char *msg, struct sockaddr_storage *addr, socklen_t *addrlen)
{
    assert(addr);
    assert(addrlen);
    /* Making a potentially dangerous assumption here that msg is fine */
    char msg_copy[strlen(msg)+1];
    strcpy(msg_copy, msg);
    strtok(msg_copy, "(");
    char *substr = strtok(NULL, ")");
    char *token = strtok(substr, ",");
    char ipv4[INET_ADDRSTRLEN];
    /* Replace ',' with '.' in msg and copy into ipv4 */
    for (int i = 0; i < 4; ++i, token = strtok(NULL, ",")) {
        assert(token);
        strcat(ipv4, token);
        strcat(ipv4, ".");
    }
    ipv4[strlen(ipv4)-1] = '\0';
    /* Combine most and least significant byte to form port */
    int msb = atoi(token);
    int lsb = atoi(strtok(NULL, ","));
    in_port_t port = (msb << 8) | lsb;
    /* Create sockaddr */
    *addrlen = sizeof(struct sockaddr_in);
    memset(addr, 0, *addrlen);
    ((struct sockaddr_in *)addr)->sin_family = AF_INET;
    ((struct sockaddr_in *)addr)->sin_port = htons(port);
    inet_pton(AF_INET, ipv4, &((struct sockaddr_in *)addr)->sin_addr);
}

/* Connect to server-DTP. */
int connect_to_dtp(int sockpi, bool rpassive) {
    int sockdtp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockdtp < 0) {
        perror("socket");
        logwarn("Failed to open data connection to the server");
        return -1;
    }
    enum reply_code reply;
    if (rpassive) {
        struct vector reply_msg;
        vector_create(&reply_msg, 128, 2);
        reply = ftp_PASV(sockpi, &reply_msg);
        struct sockaddr_storage addr;
        socklen_t addrlen;
        if (ftp_pos_completion(reply)) {
            set_remote_sockaddr(reply_msg.arr, &addr, &addrlen);
            puts(reply_msg.arr);
        } else {
            puts("Error executing command. See log");
            sockdtp = -1;
        }
        if (connect(sockdtp, (struct sockaddr *)&addr, addrlen) < 0) {
            perror("connect");
            logwarn("Failed to connect to server-DTP");
            sockdtp = -1;
        }
        vector_free(&reply_msg);
    } else {
        /* TODO implement random PORT */
        puts("Not implemented for PORT yet");
        sockdtp = -1;
    }
    return sockdtp;
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
        ch = pi_getchar(sockfd);
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
        ch = pi_getchar(sockfd);
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
        reply_code_buf[i] = pi_getchar(sockfd);
    }
    enum reply_code code = atoi(reply_code_buf);
    /* Parse reply text */
    if (pi_getchar(sockfd) == '-') {
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
    sprintf(port_str, "%"PRIu16",%"PRIu16, (port & 0xff00)>>8, (port & 0x00ff));
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
enum reply_code ftp_PASV(int sockfd, struct vector *reply_msg) {
    assert(reply_msg);
    char msg[] = "PASV\r\n";
    loginfo("Sent: PASV");
    send(sockfd, msg, strlen(msg), 0);
    return wait_for_reply(sockfd, reply_msg);
}

/* Send a RETR request and await a reply. path must not be NULL. */
enum reply_code ftp_RETR(int sockfd, const char *path, struct vector *out_msg) {
    assert(out_msg);
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
    return wait_for_reply(sockfd, out_msg);
}

/* Send a STOR request and await a reply. path must not be NULL. */
enum reply_code ftp_STOR(int sockfd, const char *path, struct vector *out_msg) {
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
    return wait_for_reply(sockfd, out_msg);
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
