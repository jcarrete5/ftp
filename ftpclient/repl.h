/*
 * CS472 HW 2
 * Jason R. Carrete
 * repl.h
 *
 * Header for repl.c
 */

#ifndef FTPC_REPL_H
#define FTPC_REPL_H

#include <setjmp.h>

/* Error jump buffer for error handling. */
extern jmp_buf err_jmp_buf;

/* Start the REPL for the user-PI. */
void repl(int sockfd);

#endif  /* FTPC_REPL_H */
