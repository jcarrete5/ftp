/*
 * CS 472 HW 3
 * Jason R. Carrete
 * auth.c
 *
 * This module serves as an interface for authenticating clients attempting
 * to connect to the FTP server.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <search.h>

#include "log.h"
#include "auth.h"

#define AUTH_FILE "out/etc/ftps_passwd"  /* This is pretty inflexible but ¯\_(ツ)_/¯ */

void auth_read_passwd(void) {
    FILE *passwd_file = fopen(AUTH_FILE, "r");
    if (!passwd_file) {
        logerr("Failed to open passwd file");
        exit(EXIT_FAILURE);
    }
    if (!hcreate(FTPS_MAX_USERS)) {
        logerr("Failed to create user hash table");
        exit(EXIT_FAILURE);
    }
    char *uname, *passwd;
    while (fscanf(passwd_file, " %m[^,],%ms", &uname, &passwd) != EOF) {
        ENTRY item = {.key=uname, .data=passwd};
        if (!hsearch(item, ENTER)) {
            logerr("Too many users in ftps_passwd");
            exit(EXIT_FAILURE);
        }
        /* Do not free uname and passwd. They need to exist for the
           duration of the program */
    }
    fclose(passwd_file);
}

bool valid_user(const char *uname) {
    char cpy[strlen(uname)+1];
    strcpy(cpy, uname);
    ENTRY sk = {.key=cpy};
    ENTRY *found = hsearch(sk, FIND);
    return (bool)found;
}

bool valid_password(const char *uname, const char *passwd) {
    char cpy[strlen(uname)+1];
    strcpy(cpy, uname);
    ENTRY sk = {.key=cpy};
    ENTRY *found = hsearch(sk, FIND);
    return found && strcmp(found->data, passwd) == 0;
}
