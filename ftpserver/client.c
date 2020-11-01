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
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <unistd.h>

#include "log.h"
#include "client.h"
#include "vector.h"
#include "auth.h"
#include "misc.h"

#define DATA_ROOT_PREFIX "./out/srv/ftps"
#define MAX_ARG_LEN 2048U

/* Sorted list of supported commands. */
const char *supported_cmds[] = {
    "CDUP",
    "CWD",
    "EPRT",
    "EPSV",
    "LIST",
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
    SERVER_BUSY     = 120,
    STARTING_TX     = 125,
    OPENING_CONN    = 150,
    COMMAND_OK      = 200,
    SUPERFLUOUS     = 202,
    SYST_TYPE       = 215,
    SERVER_READY    = 220,
    CLOSING_CONN    = 221,
    TX_COMPLETE     = 226,
    USER_LOGGED_IN  = 230,
    FILE_ACT_OK     = 250,
    PATH_CREATED    = 257,
    NEED_PASS       = 331,
    SERVER_NA       = 421,
    NO_DATA_CONN    = 425,
    CONN_CLOSED     = 426,
    NO_ACTION       = 450,
    ACTION_ABORTED  = 451,
    SYNTAX_ERR      = 500,
    SYNTAX_ERR_ARGS = 501,
    CMD_NOT_IMPL    = 502,
    BAD_SEQ         = 503,
    USER_LOGIN_FAIL = 530,
    NO_ACTION_PERM  = 550,
};

/* Buffer for data received from server-PI. */
static struct sockbuf pi_buf = {0};

static char root[PATH_MAX];
/* State for the client connection. */
static struct {
    struct sockaddr_storage port_addr;
    int id;
    int sockpi;
    bool dtp_ready;           /* True if the DTP is configured and ready for communication */
    bool passive;             /* True for passive mode, false for port mode */
    bool extended;            /* True for extended mode, false for normal mode */
    bool auth;                /* True when the client is authenticated */
    char cwd[PATH_MAX];       /* Current working directory */
    char uname[MAX_ARG_LEN];
} state;

/* Return a default reply string for a given reply code. */
static char *default_reply_str(enum reply_code code) {
    switch(code) {
        case SERVER_READY:
            return "Service ready for new user.";
        case SYNTAX_ERR:
            return "Syntax error, command unrecognized.";
        case NEED_PASS:
            return "User name okay, need password.";
        case USER_LOGIN_FAIL:
            return "Not logged in.";
        case SUPERFLUOUS:
            return "Command not implemented, superfluous at this site.";
        case USER_LOGGED_IN:
            return "User logged in, proceed.";
        case BAD_SEQ:
            return "Bad sequence of commands.";
        case CLOSING_CONN:
            return "Service closing control connection.";
        case SYNTAX_ERR_ARGS:
            return "Syntax error in parameters or arguments.";
        case PATH_CREATED:
            return "\"PATHNAME\" created.";
        case FILE_ACT_OK:
            return "Requested file action okay, completed.";
        case STARTING_TX:
            return "Data connection already open; transfer starting.";
        case TX_COMPLETE:
            return "Closing data connection.";
        case OPENING_CONN:
            return "File status okay; about to open data connection.";
        case NO_DATA_CONN:
            return "Can’t open data connection.";
        case ACTION_ABORTED:
            return "Requested action aborted: local error in processing.";
        case CMD_NOT_IMPL:
            return "Command not implemented.";
        case COMMAND_OK:
            return "Command okay.";
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
 * Construct a valid path from the specified path into dst. Return 0 if path
 * is valid, otherwise return -1.
 */
static int construct_valid_path(char dst[PATH_MAX], const char path[MAX_ARG_LEN]) {
    strcpy(dst, root);
    if (path[0] != '/') {
        strcat(dst, state.cwd);
    }
    strcat(dst, path);
    char buf[PATH_MAX];
    if (!realpath(dst, buf)) {
        logwarn("Conn %d: failed canonicalizing '%s' (realpath: %s)",
                state.id, dst, strerror(errno));
        return -1;
    }
    strcpy(dst, buf);

    /* Make sure path doesn't go below root */
    char *common = strstr(dst, root);
    if (common && strncmp(common, root, strlen(root)) == 0) {
        return 0;   /* Valid path */
    } else {
        return -1;  /* Invalid path */
    }
}

static int connect_to_dtp() {
    int sockdtp = -1;
    socklen_t addrlen = state.port_addr.ss_family == AF_INET ?
                        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    if ((sockdtp=socket(state.port_addr.ss_family, SOCK_STREAM, 0)) == -1) {
        logerr("Conn %d: failed to create socket (socket: %s)", state.id, strerror(errno));
        sockdtp = -1;
    }
    if (connect(sockdtp, (struct sockaddr *)&state.port_addr, addrlen) == -1) {
        logerr("Conn %d: failed to connect to client-dtp (connect: %s)",
               state.id, strerror(errno));
        close(sockdtp);
        sockdtp = -1;
    }
    return sockdtp;
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
            logwarn("Conn %d: unsupported command '%s'; resetting pi_buf", state.id, cmd);
            pi_buf.i = 0;
            pi_buf.size = 0;
            reply_with(CMD_NOT_IMPL, NULL, false);
            return -1;
        }
    }
    char ch = pi_getchar();
    if (ch == '\r') {
        if (pi_getchar() == '\n') {
            arg[0] = '\0';
            return 0;
        } else {
            goto err_syntax;
        }
    }
    if (strcmp(cmd, "ACCT") == 0) {
        reply_with(SUPERFLUOUS, NULL, false);
        pi_buf.i = 0;
        pi_buf.size = 0;
        return -2;
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
    /* TODO Instead of resetting the buffer, consider reading until a \r\n */
    pi_buf.i = 0;
    pi_buf.size = 0;
    reply_with(SYNTAX_ERR, NULL, false);
    return -1;
}

static void handle_USER(const char uname[MAX_ARG_LEN]) {
    state.auth = false;
    if (valid_user(uname)) {
        strcpy(state.uname, uname);
        reply_with(NEED_PASS, NULL, false);
    } else {
        reply_with(USER_LOGIN_FAIL, NULL, false);
    }
}

static void handle_PASS(const char passwd[MAX_ARG_LEN]) {
    if (strlen(state.uname) > 0) {
        if (valid_password(state.uname, passwd)) {
            state.auth = true;
            reply_with(USER_LOGGED_IN, NULL, false);
        } else {
            state.auth = false;
            state.uname[0] = '\0';
            reply_with(USER_LOGIN_FAIL, NULL, false);
        }
    } else {
        state.auth = false;
        state.uname[0] = '\0';
        reply_with(BAD_SEQ, NULL, false);
    }
}

static void handle_QUIT(void) {
    reply_with(CLOSING_CONN, NULL, false);
}

static void handle_PWD(void) {
    if (!state.auth) {
        reply_with(USER_LOGIN_FAIL, "Must be authenticated to run this command", false);
        return;
    }
    reply_with(PATH_CREATED, state.cwd, false);
}

static void handle_CWD(const char path[MAX_ARG_LEN]) {
    if (!state.auth) {
        reply_with(USER_LOGIN_FAIL, "Must be authenticated to run this command", false);
        return;
    }

    char dst[PATH_MAX];
    if (!construct_valid_path(dst, path)) {
        if (chdir(dst) == 0) {
            strcpy(state.cwd, dst+strlen(root));
            strcat(state.cwd, "/");
            loginfo("Conn %d: changed dir to '%s'; cwd=%s", state.id, dst, state.cwd);
            reply_with(FILE_ACT_OK, NULL, false);
        } else {
            logwarn("Conn %d: failed to change directory (chdir %s)",
                    state.id, strerror(errno));
            reply_with(NO_ACTION_PERM, "Cannot change to that path", false);
        }
    } else {
        logwarn("Conn %d: failed to change directory; illegal path '%s'", state.id, dst);
        reply_with(NO_ACTION_PERM, "Cannot change to that path", false);
    }
}

static void handle_PORT(char *arg) {
    if (!state.auth) {
        reply_with(USER_LOGIN_FAIL, "Must be authenticated to run this command", false);
        return;
    }

    char ipv4[INET_ADDRSTRLEN];
    ipv4[0] = '\0';
    in_port_t port;

    /* Parse ip and port from arg */
    char *tok = strtok(arg, ",");
    for (size_t i = 0; i < 4; i++, tok = strtok(NULL, ",")) {
        if (!tok || strlen(tok) > 3) {
            reply_with(SYNTAX_ERR_ARGS, "Malformed port command", false);
            return;
        }
        strncat(ipv4, tok, 3);
        if (i < 3)
            strcat(ipv4, ".");
    }
    if (!tok) {
        reply_with(SYNTAX_ERR_ARGS, "Malformed port command", false);
        return;
    }
    unsigned msb = atoi(tok);
    tok = strtok(NULL, ",");
    if (!tok) {
        reply_with(SYNTAX_ERR_ARGS, "Malformed port command", false);
        return;
    }
    unsigned lsb = atoi(tok);
    port = (msb << 8) | lsb;
    loginfo("Conn %d: PORT address is %s:%"PRIu16, state.id, ipv4, port);

    /* Update state */
    state.port_addr.ss_family = AF_INET;
    inet_pton(AF_INET, ipv4, &((struct sockaddr_in *)&state.port_addr)->sin_addr);
    ((struct sockaddr_in *)&state.port_addr)->sin_port = htons(port);
    state.dtp_ready = true;
    state.passive = false;
    state.extended = false;
    reply_with(COMMAND_OK, NULL, false);
}

static void handle_LIST(const char *path) {
    if (!state.auth) {
        reply_with(USER_LOGIN_FAIL, "Must be authenticated to run this command", false);
        return;
    }
    if (!state.dtp_ready) {
        reply_with(NO_ACTION, "Specify data transfer control command first "
                              "(i.e. PORT, PASV, EPRT, EPSV)", false);
        return;
    }
    char canon[PATH_MAX];
    if (construct_valid_path(canon, path) == -1) {
        reply_with(SYNTAX_ERR_ARGS, "Illegal path", false);
        return;
    }
    reply_with(OPENING_CONN, "Here comes the directory listing", false);

    /* Establish data connection */
    int sockdtp;
    if (state.passive) {
        /* TODO Handle PASV mode */
    } else {
        sockdtp = connect_to_dtp();
        if (sockdtp == -1) {
            reply_with(NO_DATA_CONN, NULL, false);
            return;
        }
    }

    /* Send directory listing */
    DIR *dir = opendir(canon);
    if (!dir) {
        logerr("Conn %d: error opening directory for listing (opendir: %s)",
               state.id, strerror(errno));
        reply_with(ACTION_ABORTED, NULL, false);
        close(sockdtp);
        return;
    }
    for (struct dirent *ent = readdir(dir); ent; ent = readdir(dir)) {
        char sndbuf[300];
        strcpy(sndbuf, ent->d_name);
        strcat(sndbuf, "\r\n");
        if (send(sockdtp, sndbuf, strlen(sndbuf), 0) == -1) {
            logwarn("Conn %d: error sending a directory listing (send: %s)",
                    state.id, strerror(errno));
        }
    }
    closedir(dir);
    close(sockdtp);
    reply_with(TX_COMPLETE, "Directory send OK", false);
}

/* Start handling commands from a newly connected client. */
void handle_new_client(const int id, const int sockpi,
                       struct sockaddr_storage *peeraddr,
                       socklen_t peeraddrlen) {
    /* Initialize state */
    state.id = id;
    state.sockpi = sockpi;
    strcpy(state.cwd, "/");
    if (!realpath(DATA_ROOT_PREFIX, root)) {
        logerr("Conn %d: cannot get canonical path for data root (realpath: %s)",
               state.id, strerror(errno));
        _exit(EXIT_FAILURE);
    }
    if (chdir(root) == -1) {
        logerr("Conn %d: failed to change directory to data root (chdir: %s)",
               state.id, strerror(errno));
        _exit(EXIT_FAILURE);
    }
    reply_with(SERVER_READY, NULL, false);

    /* Start response loop */
    char cmd[5], arg[MAX_ARG_LEN];
    while (true) {
        if (get_next_cmd(cmd, arg, MAX_ARG_LEN) < 0) continue;
        loginfo("Conn %d: received %s", state.id, cmd);
        if (strcmp(cmd, "USER") == 0) {
            handle_USER(arg);
        } else if (strcmp(cmd, "PASS") == 0) {
            handle_PASS(arg);
        } else if (strcmp(cmd, "QUIT") == 0) {
            if (strlen(arg) > 0) {
                /* QUIT should take no arguments */
                reply_with(SYNTAX_ERR_ARGS, NULL, false);
            } else {
                handle_QUIT();
                break;  /* Break out of loop; we are quitting */
            }
        } else if (strcmp(cmd, "PWD") == 0) {
            if (strlen(arg) > 0) {
                /* PWD should take no arguments */
                reply_with(SYNTAX_ERR_ARGS, NULL, false);
            } else {
                handle_PWD();
            }
        } else if (strcmp(cmd, "CWD") == 0) {
            handle_CWD(arg);
        } else if (strcmp(cmd, "CDUP") == 0) {
            if (strlen(arg) > 0) {
                /* PWD should take no arguments */
                reply_with(SYNTAX_ERR_ARGS, NULL, false);
            } else {
                handle_CWD("..");
            }
        } else if (strcmp(cmd, "PORT") == 0) {
            handle_PORT(arg);
        } else if (strcmp(cmd, "LIST") == 0) {
            handle_LIST(arg);
        } else {
            logwarn("Conn %d: unknown command '%s'", state.id, cmd);
            reply_with(CMD_NOT_IMPL, NULL, false);
        }
        /* Reset arg and cmd */
        cmd[0] = '\0';
        arg[0] = '\0';
    }

    close(state.sockpi);
    _exit(EXIT_SUCCESS);
}
