/*
 * CS 472 HW 3
 * Jason R. Carrete
 * auth.h
 *
 * Header for auth.c
 */

#ifndef FTPS_AUTH_H
#define FTPS_AUTH_H

#include <stdbool.h>

/* Read passwords from the password file into a hashtable. */
void auth_read_passwd(void);
/* Return true if the uname is in the passwd database. */
bool valid_user(const char *uname);
/* Return true if the passwd is correct for the given uname. */
bool valid_password(const char *uname, const char *passwd);

#endif /* FTPS_AUTH_H */
