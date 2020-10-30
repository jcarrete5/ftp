/*
 * CS472 HW 3
 * Jason R. Carrete
 * client.c
 *
 * This module handles communication with a single connected client.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "client.h"
#include "vector.h"
#include "misc.h"

#define READY_BIT  0x01U
#define PASV_BIT   0x02U
#define EXT_BIT    0x04U

/* Sorted list of supported commands. */
const char *supported_cmds[] = {
    "CDUP",
    "CWD",
    "EPRT",
    "EPSV",
    "PASS",
    "PASV",
    "PORT",
    "PWD",
    "QUIT",
    "RETR",
    "STOR",
    "USER",
};

/* Server reply codes. */
enum reply_code {
    SERVER_BUSY = 120,
    SUPERFLUOUS = 202,
    SYST_TYPE = 215,
    SERVER_READY = 220,
    USER_LOGGED_IN = 230,
    NEED_PASS = 331,
    NEED_ACCT = 332,
    SERVER_NA = 421,
    NO_DATA_CONN = 425,
    CONN_CLOSED = 426,
    NO_ACTION = 450,
    ACTION_ABORTED = 451,
    SYNTAX_ERR = 500,
    SYNTAX_ERR_ARGS = 501,
    CMD_NOT_IMPL = 502,
    USER_LOGIN_FAIL = 530,
};

/* Buffer for data received from server-PI. */
static struct sockbuf pi_buf = {0};

/* State for the client connection. */
static struct {
    int id;
    int sockpi;
    bool got_user;
    bool got_pass;
    bool auth;      /* True when the client is authenticated */
    unsigned to;    /* Transmission Options */
} state;

/* Return a default reply string for a given reply code. */
static char *default_reply_str(enum reply_code code) {
    switch(code) {
        case SERVER_READY:
            return "Service ready for new user.";
        case SYNTAX_ERR:
            return "Syntax error, command unrecognized.";
        default:
            return "Acknowledged";
    }
}

/*
 * Send a reply to the client with the given reply code and optional single or
 * multi line message.
 */
static void reply_with(enum reply_code code, const char *msg, bool multiline) {
    const char *text = msg ? msg : default_reply_str(code);
    char code_str[4];
    sprintf(code_str, "%d", code);
    struct vector buf;
    vector_create(&buf, 128, 2);
    vector_append_str(&buf, code_str);
    if (multiline) {
        vector_append(&buf, '-');
    } else {
        vector_append(&buf, ' ');
    }
    vector_append_str(&buf, text);
    vector_append_str(&buf, "\r\n");
    if (multiline) {
        vector_append_str(&buf, code_str);
        vector_append(&buf, ' ');
        vector_append_str(&buf, text);
        vector_append_str(&buf, "\r\n");
    }
    if (send(state.sockpi, buf.arr, buf.size, 0) == -1) {
        logerr("Conn %d: failed to send reply (send: %s)", state.id, strerror(errno));
    } else {
        loginfo("Conn %d: sent reply code %d", state.id, code);
    }
    vector_free(&buf);
}

/* Wrapper for getchar_from_sock(.) that handles errors. */
static char pi_getchar() {
    int ch = getchar_from_sock(state.sockpi, &pi_buf);
    if (ch == -1 && errno != EINTR) {
        logerr("Conn %d: error in getchar_from_sock(.) (recv: %s)",
               state.id, strerror(errno));
        _exit(EXIT_FAILURE);
    } else if (ch == 0) {
        loginfo("Conn %d: connection closed by client", state.id);
        _exit(EXIT_SUCCESS);
    }
    return ch;
}

/*
 * Attempt to parse the next command sent from the client. Return -1 if the
 * command is malformed, otherwise return 0 and set cmd, and arg appropriately.
 */
static int get_next_cmd(char cmd[5], char arg[], size_t arglen) {
    assert(cmd);
    assert(arg);

    /* Parse command string */
    cmd[0] = toupper(pi_getchar());
    cmd[1] = toupper(pi_getchar());
    cmd[2] = toupper(pi_getchar());
    cmd[3] = '\0';
    if (!bsearch(cmd, supported_cmds, sizeof supported_cmds / sizeof *supported_cmds,
                 sizeof *supported_cmds, bsearch_strcmp))
    {
        cmd[3] = toupper(pi_getchar());
        cmd[4] = '\0';
        if (!bsearch(cmd, supported_cmds, sizeof supported_cmds / sizeof *supported_cmds,
                     sizeof *supported_cmds, bsearch_strcmp))
        {
            logwarn("Conn %d: unsupported command; resetting pi_buf", state.id);
            pi_buf.i = 0;
            pi_buf.size = 0;
            reply_with(CMD_NOT_IMPL, NULL, false);
            return -1;
        }
    }
    char ch = pi_getchar();
    if (ch == '\r') {
        if (pi_getchar() == '\n') {
            return 0;
        } else {
            goto err_syntax;
        }
    }

    /* Parse optional argument */
    if (ch != ' ') {
        goto err_syntax;
    }
    size_t i;
    for (i = 0; i < arglen; i++) {
        char ch = pi_getchar();
        if (ch == '\n' && arg[i-1] == '\r') {
            arg[i-1] = '\0';
            return 0;
        }
        arg[i] = ch;
    }

    err_syntax:
    logwarn("Conn %d: invalid command or too long; resetting pi_buf", state.id);
    pi_buf.i = 0;
    pi_buf.size = 0;
    reply_with(SYNTAX_ERR, NULL, false);
    return -1;
}

/* Start handling commands from a newly connected client. */
void handle_new_client(const int id, const int sockpi,
                       struct sockaddr_storage *peeraddr,
                       socklen_t peeraddrlen) {
    state.id = id;
    state.sockpi = sockpi;
    reply_with(SERVER_READY, NULL, false);

    char cmd[5], arg[2048];
    while (true) {
        if (get_next_cmd(cmd, arg, 2048) == -1) continue;
        loginfo("Conn %d: received %s", state.id, cmd);
        if (strcmp(cmd, "SYST") == 0) {
            reply_with(SYST_TYPE, "UNIX system type", false);
        } else {
            logwarn("Conn %d: unknown command '%s'", state.id, cmd);
            reply_with(CMD_NOT_IMPL, NULL, false);
        }
    }

    close(state.sockpi);
    _exit(EXIT_SUCCESS);
}
