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
#include <stdio.h>
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
#include "misc.h"

/* Socket file descriptor for the user-PI. */
static int sockpi;

/* IP address of remote server. */
static const char *ripstr;

/* True while the repl is running. False will stop on next iteration. */
static bool repl_running = true;

/*
 * Specifies the delivery option for data transfer commands. This field is a
 * 2-bit bit-string where msb is PASV or PORT and lsb specifies EXT.
 */
static unsigned int delivery_option = FTPC_DO_PASV;

/* Buffer for DTP data. */
static struct sockbuf dtp_buf = {0};

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
    ftp_QUIT(sockpi);
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
    const bool rpassive = (delivery_option & FTPC_DO_PASV) == 1;

    /* Connect to or wait for server */
    enum reply_code reply;
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    int sockdtp = -1;
    if (rpassive) {
        sockdtp = connect_to_dtp(sockpi, delivery_option, ripstr);
        if (sockdtp < 0) {
            goto exit;
        }
        reply = ftp_LIST(sockpi, path, &reply_msg);
    } else {
        pthread_t tid;
        if (accept_server(sockpi, delivery_option, &tid) < 0) {
            logerr("Error during accept_server");
            goto exit;
        }
        reply = ftp_LIST(sockpi, path, &reply_msg);
        int *ret;
        if (pthread_join(tid, (void **)&ret)) {
            logerr("Error waiting for thread in join");
            goto exit;
        }
        sockdtp = *ret;
        free(ret);
    }

    /* Parse reply */
    while (ftp_pos_preliminary(reply)) {
        puts(reply_msg.arr);
        reply_msg.size = 0;
        reply = wait_for_reply(sockpi, &reply_msg);
    }
    if (ftp_pos_completion(reply) || ftp_trans_neg(reply)) {
        int ch;
        /* Read stream of bytes from server-DTP */
        while ((ch=getchar_from_sock(sockdtp, &dtp_buf))) {
            if (ch < 0) {
                perror("recv");
                logerr("Error while reading from server-DTP");
                break;
            } else {
                putchar(ch);
            }
        }
        puts(reply_msg.arr);
    } else {
        puts("Error executing command. See log");
    }

    exit:
    if (sockdtp >= 0)
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
    if (ftp_pos_completion(reply)) {
        puts("Directory change OK");
    } else {
        puts("Failed to change working directory. See log");
    }
}

/* Print the delivery options as a string. */
static void print_delivery_opt(void) {
    switch (delivery_option) {
        case FTPC_DO_PASV:
            puts("PASV before data transfer is enabled");
            break;
        case FTPC_DO_EPSV:
            puts("EPSV before data transfer is enabled");
            break;
        case FTPC_DO_PORT:
            puts("PORT before data transfer is enabled");
            break;
        case FTPC_DO_EPRT:
            puts("EPRT before data transfer is enabled");
            break;
    }
}

static void handle_passive(void) {
    delivery_option ^= FTPC_DO_PASV;
    print_delivery_opt();
}

static void handle_extend(void) {
    delivery_option ^= FTPC_DO_EXT;
    print_delivery_opt();
}

/* Handle get repl command. path points to a remote file. */
static void handle_get(const char *path) {
    if (!path) {
        puts("Path must not be NULL");
        return;
    }
    const bool rpassive = (delivery_option & FTPC_DO_PASV) == 1;
    int sockdtp = -1;
    struct vector reply_msg;
    vector_create(&reply_msg, 128, 2);
    uint8_t *databuf = calloc(BUFSIZ, sizeof *databuf);
    if (!databuf) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    /* Ask for file save location */
    printf("Save location: ");
    struct vector in;
    vector_create(&in, 128, 2);
    get_input_str(&in);
    FILE *file = fopen(in.arr, "w");
    vector_free(&in);
    if (!file) {
        perror("fopen");
        goto exit;
    }

    /* Connect to or wait for server */
    enum reply_code reply;
    if (rpassive) {
        sockdtp = connect_to_dtp(sockpi, delivery_option, ripstr);
        if (sockdtp < 0) {
            goto exit;
        }
        reply = ftp_RETR(sockpi, path, &reply_msg);
    } else {
        pthread_t tid;
        if (accept_server(sockpi, delivery_option, &tid) < 0) {
            logerr("Error during accept_server");
            goto exit;
        }
        reply = ftp_RETR(sockpi, path, &reply_msg);
        int *ret;
        if (pthread_join(tid, (void **)&ret)) {
            logerr("Error waiting for thread in join");
            goto exit;
        }
        sockdtp = *ret;
        free(ret);
    }

    /* Parse reply and collect data from DTP */
    while (ftp_pos_preliminary(reply)) {
        puts(reply_msg.arr);
        reply_msg.size = 0;
        ssize_t read;
        /* Read stream of bytes from server-DTP */
        while ((read=recv(sockdtp, databuf, BUFSIZ, 0))) {
            if (read < 0) {
                perror("recv");
                logerr("Error while reading from server-DTP");
                goto exit;
            } else {
                if (fwrite(databuf, sizeof *databuf, read, file) < (size_t)read) {
                    logwarn("May have failed to save all data to file");
                    goto exit;
                }
            }
        }
        reply = wait_for_reply(sockpi, &reply_msg);
    }
    puts(reply_msg.arr);

    exit:
    free(databuf);
    if (sockdtp >= 0)
        close(sockdtp);
    if (file)
        fclose(file);
    vector_free(&reply_msg);
}

/* Handle send repl command. path points to local file. */
static void handle_send(const char *localpath) {
    /* Validate args and init variables */
    if (!localpath) {
        puts("Path must not be NULL");
        return;
    }
    struct vector in, reply_msg;
    vector_create(&in, 128, 2);
    vector_create(&reply_msg, 128, 2);
    const bool rpassive = (delivery_option & FTPC_DO_PASV) == 1;
    int sockdtp = -1;
    uint8_t *databuf = calloc(BUFSIZ, sizeof *databuf);
    if (!databuf) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    FILE *in_file = fopen(localpath, "r");
    if (!in_file) {
        perror("fopen");
        goto exit;
    }

    /* Read save location */
    printf("Save location (on server): ");
    get_input_str(&in);

    /* Connect to or wait for reply server */
    enum reply_code reply;
    if (rpassive) {
        sockdtp = connect_to_dtp(sockpi, delivery_option, ripstr);
        if (sockdtp < 0) {
            goto exit;
        }
        reply = ftp_STOR(sockpi, in.arr, &reply_msg);
    } else {
        pthread_t tid;
        if (accept_server(sockpi, delivery_option, &tid) < 0) {
            logerr("Error during accept_server");
            goto exit;
        }
        reply = ftp_STOR(sockpi, in.arr, &reply_msg);
        int *ret;
        if (pthread_join(tid, (void **)&ret)) {
            logerr("Error waiting for thread in join");
            goto exit;
        }
        sockdtp = *ret;
        free(ret);
    }

    while (ftp_pos_preliminary(reply)) {
        puts(reply_msg.arr);
        reply_msg.size = 0;
        size_t read;
        while ((read=fread(databuf, sizeof *databuf, BUFSIZ, in_file)) > 0) {
            if (send(sockdtp, databuf, read, 0) <= 0) {
                perror("send");
                goto exit;
            }
        }
        close(sockdtp);
        reply = wait_for_reply(sockpi, &reply_msg);
    }
    puts(reply_msg.arr);

    exit:
    free(databuf);
    if (in_file)
        fclose(in_file);
    if (sockdtp >= 0)
        close(sockdtp);
    vector_free(&reply_msg);
    vector_free(&in);
}

/* Start the REPL for the user-PI. */
void repl(const int sockfd, const char *ipstr) {
    sockpi = sockfd;
    ripstr = ipstr;
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
        if (!token) goto end;
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
        } else if (strcmp(token, "extend") == 0) {
            handle_extend();
        } else if (strcmp(token, "get") == 0) {
            token = strtok(NULL, " \t");
            handle_get(token);
        } else if (strcmp(token, "send") == 0) {
            token = strtok(NULL, " \t");
            handle_send(token);
        } else {
            puts("Unknown command");
        }
        end:
        in.size = 0;
    }
    vector_free(&in);
}
