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

#include "repl.h"
#include "log.h"
#include "vector.h"
#include "ftp.h"

/* Socket file descriptor for the user-PI. */
static int sockpi;

static const size_t MIN_STR_BUFFER_SIZE = (size_t) 1024;

/* Read user input from stdin into the vector. */
void get_input_str(struct vector *str) {
    int ch;
    while ((ch=getchar()) != EOF && ch != '\n') {
        vector_append(str, (char) ch);
    }
    vector_append(str, '\0');
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

/* Start the REPL for the user-PI. */
void repl(const int sockfd) {
    sockpi = sockfd;
    if (atexit(cleanup)) {
        logwarn("Socket for user-PI will not be cleaned up on exit");
    }
    enum reply_code reply;
    do {
        reply = wait_for_reply(sockpi);
        if (reply == FTP_SERVER_NA) {
            puts("Server not currently available");
            return;
        }
    } while (reply != FTP_SERVER_READY);
    puts("Server is ready");
    do {
        reply = login_with_credentials();
        if (!ftp_pos_completion(reply)) {
            puts("Invalid login credentials");
        }
    } while (!ftp_pos_completion(reply));
    struct vector in;
    vector_create(&in, 64, 2);
    while (true) {
        printf("> ");
        get_input_str(&in);
    }
    vector_free(&in);
}
