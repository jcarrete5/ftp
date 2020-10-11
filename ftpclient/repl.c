/*
 * CS472 HW 2
 * Jason R. Carrete
 * repl.c
 *
 * This module implements the Read-Eval-Print-Loop (REPL) for the application.
 * The REPL is responsible for processing user input and displaying output in
 * response.
 */

#include "config.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "repl.h"
#include "log.h"
#include "vector.h"
#include "ftp.h"

/* Socket file descriptor for the user-PI. */
static int sockpi;

/* True while the repl is running. False will stop on next iteration. */
static bool repl_running = true;

/*
 * When true, the server is in passive mode (i.e. the client will initiate
 * connections for data transfer).
 */
static bool rpassive = false;

/* Read user input from stdin into the vector. Return 1 if EOF was reached. */
static void get_input_str(struct vector *str) {
    int ch;
    while ((ch=getchar()) != EOF && ch != '\n') {
        vector_append(str, (char) ch);
    }
    vector_append(str, '\0');
    if (ch == EOF) {
        puts("stdin closed. Goodbye");
        ftp_QUIT(sockpi);
        exit(EXIT_SUCCESS);
    }
}

/* Cleanup resources used by the repl on exit. */
static void cleanup(void) {
    loginfo("Closing user-PI socket");
    close(sockpi);
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

/*
 * Go through authentication sequence with the remote server-PI. Returns the
 * reply code.
 */
static enum reply_code login_with_credentials(void) {
    struct vector str;
    vector_create(&str, 64, 2);
    printf("Username: ");
    get_input_str(&str);
    enum reply_code reply = ftp_USER(sockpi, str.arr);
    if (ftp_pos_completion(reply)) {
        puts("Username OK");
    } else if (ftp_pos_intermediate(reply)) {
        puts("Username OK");
        printf("Password: ");
        str.size = 0;  /* Reset array */
        get_input_str(&str);  /* TODO Don't echo the password */
        reply = ftp_PASS(sockpi, str.arr);
        if (ftp_pos_completion(reply)) {
            puts("Password OK");
        } else if (ftp_pos_intermediate(reply)) {
            /* We don't support ACCT as per requirements */
            puts("ACCT not supported by this client. Cannot complete auth");
            exit(EXIT_SUCCESS);
        }
    }
    vector_free(&str);
    return reply;
}

/* Wait for the server to be ready. */
static void wait_for_server(void) {
    enum reply_code reply;
    do {
        reply = wait_for_reply(sockpi, NULL);
    } while (reply != FTP_SERVER_READY);
    puts("Server is ready");
}

/* Handle quit repl command. */
static void handle_quit(void) {
    repl_running = false;
    enum reply_code reply = ftp_QUIT(sockpi);
    /* Reply is either 221 or 500; Ignoring since we are quitting anyway. */
}

/* Handle help repl command. */
static void handle_help(const char *cmd) {
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    enum reply_code reply = ftp_HELP(sockpi, cmd, &reply_msg);
    if (ftp_pos_completion(reply)) {
        puts(reply_msg.arr);
    } else if (reply == FTP_SYNTAX_ERR || reply == FTP_SYNTAX_ERR_ARGS) {
        puts("Syntax error; check your command");
    } else if (reply == FTP_CMD_NOT_IMPL) {
        puts("Command not recognized by the server");
    } else {
        puts("Error executing command. See log");
    }
    vector_free(&reply_msg);
}

/* Handle pwd repl command. */
static void handle_pwd(void) {
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    enum reply_code reply = ftp_PWD(sockpi, &reply_msg);
    if (ftp_pos_completion(reply)) {
        puts(reply_msg.arr);
    } else {
        puts("Error executing command. See log");
    }
    vector_free(&reply_msg);
}

/* Handle system repl command. */
static void handle_system(void) {
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    enum reply_code reply = ftp_SYST(sockpi, &reply_msg);
    if (ftp_pos_completion(reply)) {
        puts(reply_msg.arr);
    } else {
        puts("Error executing command. See log");
    }
    vector_free(&reply_msg);
}

/* Handle ls repl command. */
static void handle_ls(const char *path) {
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    int sockdtp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockdtp < 0) {
        perror("socket");
        logwarn("Failed to open data connection to the server");
        return;
    }
    /* Establish connection to server-DTP */
    enum reply_code reply;
    if (rpassive) {
        reply = ftp_PASV(sockpi, &reply_msg);
        struct sockaddr_storage addr;
        socklen_t addrlen;
        if (ftp_pos_completion(reply)) {
            set_remote_sockaddr(reply_msg.arr, &addr, &addrlen);
            puts(reply_msg.arr);
            reply_msg.size = 0;
        } else {
            puts("Error executing command. See log");
            goto exit;
        }
        if (connect(sockdtp, (struct sockaddr *)&addr, addrlen) < 0) {
            perror("connect");
            logwarn("Failed to connect to server-DTP");
            goto exit;
        }
    } else {
        /* TODO implement random PORT */
        puts("Not implemented for PORT yet");
        goto exit;
    }
    /* Send LIST command */
    reply = ftp_LIST(sockpi, path, &reply_msg);
    while (ftp_pos_preliminary(reply)) {
        puts(reply_msg.arr);
        reply_msg.size = 0;
        reply = wait_for_reply(sockpi, &reply_msg);
    }
    if (ftp_pos_completion(reply) || ftp_trans_neg(reply)) {
        struct sockbuf buf = {0};
        int ch;
        /* Read stream of bytes from server-DTP */
        while ((ch = getchar_from_sock(sockdtp, &buf))) {
            if (ch < 0) {
                perror("recv");
                logerr("Error while reading from server-DTP");
            } else {
                putchar(ch);
            }
        }
        puts(reply_msg.arr);
    } else {
        puts("Error executing command. See log");
    }
    exit:
          close(sockdtp);
          vector_free(&reply_msg);
}

/* Handle cd repl command. */
static void handle_cd(const char *path) {
    if (!path) {
        puts("A path must be specified");
        return;
    }
    enum reply_code reply = ftp_CWD(sockpi, path);
    if (!ftp_pos_completion(reply)) {
        puts("Failed to change working directory. See log");
    }
}

/* Handle passive repl command. */
static void handle_passive(void) {
    rpassive = !rpassive;
    printf("PASV before data transfers: %s\n", rpassive ? "enabled" : "disabled");
}
    vector_free(&msg);
}

/* Start the REPL for the user-PI. */
void repl(const int sockfd, const char *ipstr) {
    sockpi = sockfd;
    if (atexit(cleanup)) {
        logwarn("Socket for user-PI will not be cleaned up on exit");
    }
    wait_for_server();
    enum reply_code reply;
    do {
        reply = login_with_credentials();
        if (!ftp_pos_completion(reply)) {
            puts("Invalid login credentials");
        }
    } while (!ftp_pos_completion(reply));
    /* Parse user input and execute associated handler function */
    struct vector in;
    vector_create(&in, 64, 2);
    while (repl_running) {
        printf("> ");
        get_input_str(&in);
        const char *token = strtok(in.arr, " \t");
        if (strcmp(token, "quit") == 0) {
            handle_quit();
        } else if (strcmp(token, "help") == 0) {
            token = strtok(NULL, " \t");
            handle_help(token);
        } else if (strcmp(token, "pwd") == 0) {
            handle_pwd();
        } else if (strcmp(token, "system") == 0) {
            handle_system();
        } else if (strcmp(token, "ls") == 0) {
            token = strtok(NULL, " \t");
            handle_ls(token);
        } else if (strcmp(token, "cd") == 0) {
            token = strtok(NULL, " \t");
            handle_cd(token);
        } else if (strcmp(token, "passive") == 0) {
            handle_passive();
        } else {
            puts("Unknown command");
        }
        in.size = 0;
    }
    vector_free(&in);
}
