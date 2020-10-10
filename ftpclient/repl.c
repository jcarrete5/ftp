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
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include "repl.h"
#include "log.h"
#include "vector.h"
#include "ftp.h"

/* Socket file descriptor for the user-PI. */
static int sockpi;
static bool repl_running = true;

/* Read user input from stdin into the vector. Return 1 if EOF was reached. */
static int get_input_str(struct vector *str) {
    int ch;
    while ((ch=getchar()) != EOF && ch != '\n') {
        vector_append(str, (char) ch);
    }
    vector_append(str, '\0');
    if (ch == EOF) {
        repl_running = false;
        return 1;
    }
    return 0;
}

/* Cleanup resources used by the repl on exit. */
static void cleanup(void) {
    loginfo("Closing user-PI socket");
    close(sockpi);
}

/* Go through authentication sequence with the remote server-PI. */
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
        get_input_str(&str);
        reply = ftp_PASS(sockpi, str.arr);
        if (ftp_pos_completion(reply)) {
            puts("Password OK");
        } /* We don't need to implement ACCT as per requirements */
    }
    vector_free(&str);
    return reply;
}

/* Wait for the server to be ready. */
static void wait_for_server(void) {
    enum reply_code reply;
    do {
        reply = wait_for_reply(sockpi, NULL);
        if (reply == FTP_SERVER_NA) {
            puts("Server not currently available");
            return;
        }
    } while (reply != FTP_SERVER_READY);
    puts("Server is ready");
}

/* Handle quit repl command. */
static void handle_quit(void) {
    repl_running = false;
    enum reply_code reply = ftp_QUIT(sockpi);
    /* TODO handle reply code */
}

/* Handle help repl command. */
static void handle_help(const char *cmd) {
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    enum reply_code reply = ftp_HELP(sockpi, cmd, &reply_msg);
    if (ftp_pos_completion(reply)) {
        puts(reply_msg.arr);
    } else {
        /* TODO Handle error replies */
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
        /* TODO Handle error replies */
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
        /* TODO Handle error replies */
    }
    vector_free(&reply_msg);
}

/* Start the REPL for the user-PI. */
void repl(const int sockfd) {
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
        if (get_input_str(&in)) break;
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
        } else {
            puts("Unknown command");
        }
        in.size = 0;
    }
    vector_free(&in);
}
