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

jmp_buf err_jmp_buf;

/*
 * TODO
 * MUST implement USER, PASS, CWD, QUIT, PASV, EPSV, PORT, EPRT, RETR, STOR,
 * PWD, SYST, LIST, and HELP commands
 */

static void login_with_credentials(int sockfd) {
    switch (setjmp(err_jmp_buf)) {
        case FATAL_ERROR:
            logerr("Aborting");
            puts("A fatal error occured. See log for details");
            close(sockfd);
            exit(EXIT_FAILURE);
            break;
        case ILLEGAL_ARG:
            puts("Invalid argument. See log for details");
            break;
    }
    printf("Username: ");
    char username[4096];
    scanf(" %4095s", username);
    ftp_USER(sockfd, username);
    printf("Password: ");
    char password[4096];
    scanf(" %4095s", password);
    ftp_PASS(sockfd, password);
}

void repl(int sockfd) {
    login_with_credentials(sockfd);
    puts(FTPC_EXE_NAME" "FTPC_VERSION);
    while (true) {
        printf("> ");
        char buf[4096];
        scanf(" %4095s", buf);
    }
}
