#ifndef FTPC_REPL_H
#define FTPC_REPL_H

#include <setjmp.h>

extern jmp_buf err_jmp_buf;

void repl(int sockfd);

#endif  /* FTPC_REPL_H */
