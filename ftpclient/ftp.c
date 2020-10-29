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
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "ftp.h"
#include "log.h"
#include "vector.h"
#include "misc.h"

/* Socket buffer for the PI. */
static struct sockbuf pi_buf = {0};

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
    return ret;
}

/* Thread function to accept a connection and return the connecting socket. */
static void *accept_connection(void *arg) {
    int *socklisten = (int *)arg;
    int *sockdtp = malloc(sizeof *sockdtp);
    if (!sockdtp) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    loginfo("Waiting for a connection to user-DTP");
    *sockdtp = accept(*socklisten, NULL, NULL);
    loginfo("Got for a connection to user-DTP");
    close(*socklisten);
    free(socklisten);
    return sockdtp;
}

/* Set the socket address to connect to when the server is in passive mode. */
static void parse_remote_sockaddr
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

static int do_PASV(int sockpi) {
    struct sockaddr_storage addr;
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    int sockdtp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockdtp < 0) {
        perror("socket");
        logwarn("Failed to open data connection to the server");
        sockdtp = -1;
        goto exit;
    }

    /* Send PASV and wait for reply */
    enum reply_code code = ftp_PASV(sockpi, &reply_msg);
    socklen_t addrlen;
    if (ftp_pos_completion(code)) {
        parse_remote_sockaddr(reply_msg.arr, &addr, &addrlen);
        puts(reply_msg.arr);
    } else {
        puts("Error executing command. See log");
        close(sockdtp);
        sockdtp = -1;
        goto exit;
    }

    /* Connect to server-DTP */
    if (connect(sockdtp, (struct sockaddr *)&addr, addrlen) < 0) {
        perror("connect");
        logwarn("Failed to connect to server-DTP");
        close(sockdtp);
        sockdtp = -1;
        goto exit;
    }

    exit:
    vector_free(&reply_msg);
    return sockdtp;
}

static int do_EPSV(int sockpi, const char *ipstr) {
    uint16_t port;
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    int af = strchr(ipstr, ':') ? AF_INET6 : AF_INET;
    int sockdtp = socket(af, SOCK_STREAM, IPPROTO_TCP);
    if (sockdtp < 0) {
        perror("socket");
        logwarn("Failed to open data connection to the server");
        sockdtp = -1;
        goto exit;
    }

    /* Send EPSV and wait for reply */
    enum reply_code code = ftp_EPSV(sockpi, &reply_msg);
    if (ftp_pos_completion(code)) {
        puts(reply_msg.arr);
        strtok(reply_msg.arr, "|");
        char *port_str = strtok(NULL, "|");
        port = atoi(port_str);
    } else {
        puts("Error executing command. See log");
        close(sockdtp);
        sockdtp = -1;
        goto exit;
    }

    /* Connect to server-DTP */
    struct sockaddr addr = {0};
    struct sockaddr_in *ipv4;
    struct sockaddr_in6 *ipv6;
    socklen_t addrlen;
    switch (af) {
        case AF_INET:
            ipv4 = (struct sockaddr_in *)&addr;
            inet_pton(af, ipstr, &(ipv4->sin_addr));
            ipv4->sin_port = htons(port);
            ipv4->sin_family = af;
            addrlen = sizeof(struct sockaddr_in);
            break;
        case AF_INET6:
            ipv6 = (struct sockaddr_in6 *)&addr;
            inet_pton(af, ipstr, &(ipv6->sin6_addr));
            ipv6->sin6_port = htons(port);
            ipv6->sin6_family = af;
            addrlen = sizeof(struct sockaddr_in6);
            break;
    }
    if (connect(sockdtp, &addr, addrlen) < 0) {
        perror("connect");
        logwarn("Failed to connect to server-DTP");
        close(sockdtp);
        sockdtp = -1;
        goto exit;
    }

    exit:
    vector_free(&reply_msg);
    return sockdtp;
}

static int do_PORT(int sockpi, pthread_t *tid, const char *ipstr) {
    /* Listen and get the port we are listening on */
    int *socklisten = malloc(sizeof *socklisten);
    if (!socklisten) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    *socklisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*socklisten < 0) {
        perror("socket");
        logwarn("Failed to create listen socket");
        goto err;
    }
    if (listen(*socklisten, 1) < 0) {
        perror("listen");
        goto err;
    }
    struct sockaddr saddr_myport;
    socklen_t len = sizeof saddr_myport;
    if (getsockname(*socklisten, &saddr_myport, &len) < 0) {
        perror("getsockname");
        goto err;
    }
    struct sockaddr_in *myport = (struct sockaddr_in *)&saddr_myport;
    uint16_t port = htons(myport->sin_port);

    /* Start waiting for connections */
    if (pthread_create(tid, NULL, accept_connection, socklisten)) {
        logerr("Failed to create a thread for accepting connections");
        goto err;
    }

    /* Send PORT and wait for reply */
    enum reply_code reply = ftp_PORT(sockpi, ipstr, port);
    if (ftp_pos_completion(reply)) {
        puts("PORT OK");
    } else {
        puts("Error executing command. See log");
        pthread_cancel(*tid);
        goto err;
    }

    return 0;
    err:
    if (*socklisten >= 0)
        close(*socklisten);
    free(socklisten);
    return -1;
}

static int do_EPRT(int sockpi, pthread_t *tid, const char *ipstr) {
    const int af = strchr(ipstr, ':') ? AF_INET6 : AF_INET;
    /* Listen and get the port we are listening on */
    int *socklisten = malloc(sizeof *socklisten);
    if (!socklisten) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    *socklisten = socket(af, SOCK_STREAM, IPPROTO_TCP);
    if (*socklisten < 0) {
        perror("socket");
        logwarn("Failed to create listen socket");
        goto err;
    }
    if (listen(*socklisten, 1) < 0) {
        perror("listen");
        goto err;
    }
    struct sockaddr saddr_myport;
    socklen_t len = sizeof saddr_myport;
    if (getsockname(*socklisten, &saddr_myport, &len) < 0) {
        perror("getsockname");
        goto err;
    }
    uint16_t port;
    switch (af) {
        case AF_INET:
            port = ntohs(((struct sockaddr_in *)&saddr_myport)->sin_port);
            break;
        case AF_INET6:
            port = ntohs(((struct sockaddr_in6 *)&saddr_myport)->sin6_port);
            break;
    }

    /* Start waiting for connections */
    if (pthread_create(tid, NULL, accept_connection, socklisten)) {
        logerr("Failed to create a thread for accepting connections");
        goto err;
    }

    /* Send EPRT and wait for reply */
    enum reply_code reply = ftp_EPRT(sockpi, af, ipstr, port);
    if (ftp_pos_completion(reply)) {
        puts("EPRT OK");
    } else {
        puts("Error executing command. See log");
        pthread_cancel(*tid);
        goto err;
    }

    return 0;
    err:
    if (*socklisten >= 0)
        close(*socklisten);
    free(socklisten);
    return -1;
}

/* Connect to server-DTP. */
int connect_to_dtp(int sockpi, unsigned int delivery_option, const char *ripstr) {
    assert((delivery_option & FTPC_DO_PASV) == 1);
    int sockdtp;
    if ((delivery_option & FTPC_DO_EXT) == 0) {
        sockdtp = do_PASV(sockpi);
    } else {
        assert(ripstr);
        sockdtp = do_EPSV(sockpi, ripstr);
    }
    return sockdtp;
}

/* Start a thread waiting for server connection. */
int accept_server(int sockpi, unsigned int delivery_option, pthread_t *tid) {
    assert((delivery_option & FTPC_DO_PASV) == 0);
    struct sockaddr myaddr;
    socklen_t saddrlen;
    if (getsockname(sockpi, &myaddr, &saddrlen) < 0) {
        perror("getsockname");
        logerr("Failed to get bound IP of sockpi");
        return -1;
    }

    /* Get my IP string */
    char ipstr[INET6_ADDRSTRLEN];
    switch (myaddr.sa_family) {
        case AF_INET:
            inet_ntop(myaddr.sa_family, &((struct sockaddr_in *)&myaddr)->sin_addr, ipstr, saddrlen);
            break;
        case AF_INET6:
            inet_ntop(myaddr.sa_family, &((struct sockaddr_in6 *)&myaddr)->sin6_addr, ipstr, saddrlen);
            break;
    }

    if ((delivery_option & FTPC_DO_EXT) == 0) {
        if (do_PORT(sockpi, tid, ipstr) < 0) {
            return -1;
        }
    } else {
        if (do_EPRT(sockpi, tid, ipstr) < 0) {
            return -1;
        }
    }
    return 0;
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

/* Send a PORT request and await a reply. port is network-byte-order. */
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
    sprintf(family_str, "%d", (family == AF_INET) ? 1 : 2);
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
    vector_append_str(&msg, "\r\n");
    send(sockfd, msg.arr, msg.size, 0);
    vector_free(&msg);
    return wait_for_reply(sockfd, NULL);
}

enum reply_code ftp_EPSV(int sockfd, struct vector *out_msg) {
    assert(out_msg);
    char msg[] = "EPSV\r\n";
    loginfo("Sent: ESPV");
    send(sockfd, msg, strlen(msg), 0);
    return wait_for_reply(sockfd, out_msg);
}
