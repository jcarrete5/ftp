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

void auth_read_passwd(void);
bool valid_user(const char *uname);
bool valid_password(const char *uname, const char *passwd);

#endif /* FTPS_AUTH_H */
