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
#include <setjmp.h>
#include <unistd.h>

#include "repl.h"
#include "log.h"
#include "ftp.h"

/* Error jump buffer for error handling. */
jmp_buf err_jmp_buf;

/* Socket file descriptor for the user-PI. */
static int sockpi;

static const size_t MIN_STR_BUFFER_SIZE = (size_t) 1024;

/* Cleanup resources used by the repl on exit. */
static void cleanup(void) {
    close(sockpi);
}

/* Go through authentication sequence with the remove server-PI. */
static void login_with_credentials(void) {
    switch (setjmp(err_jmp_buf)) {
        case FATAL_ERROR:
            logerr("Aborting");
            puts("A fatal error occured. See log for details");
            exit(EXIT_FAILURE);
            break;
        case ILLEGAL_ARG:
            puts("Invalid argument. See log for details");
            break;
    }
    printf("Username: ");
    char username[4096];
    scanf(" %4095s", username);
    ftp_USER(sockpi, username);
    printf("Password: ");
    char password[4096];
    scanf(" %4095s", password);
    ftp_PASS(sockpi, password);
}

/*
 * Read user input from stdin and return the resulting string. The returned
 * string is dynamically allocated and **must** be freed after use.
 */
const char *get_input_str(void) {
    char *buf = calloc(MIN_STR_BUFFER_SIZE, sizeof(char));
    if (!buf) {
        logerr("Failed to allocate memory for user input");
        longjmp(err_jmp_buf, FATAL_ERROR);
    }
}

/* Start the REPL for the user-PI. */
void repl(const int sockfd) {
    sockpi = sockfd;
    if (atexit(cleanup)) {
        logwarn("Socket for user-PI will not be cleaned up on exit");
    }
    /* TODO: Wait for reply from server */
    login_with_credentials();
    puts(FTPC_EXE_NAME" "FTPC_VERSION);
    while (true) {
        printf("> ");
        char buf[4096];
        scanf(" %4095s", buf);
    }
}
