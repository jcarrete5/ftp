/*
 * CS472 HW 2
 * Jason R. Carrete
 * ftp.h
 *
 * Header for ftp.c
 */

#ifndef FTPC_FTP_H
#define FTPC_FTP_H

/* Check if reply code x is a positive preliminary reply. */
#define ftp_pos_preliminary(x) ((x) >= 100 && (x) < 200)
/* Check if reply code x is a positive completion reply. */
#define ftp_pos_completion(x) ((x) >= 200 && (x) < 300)
/* Check if reply code x is a positive intermediate reply. */
#define ftp_pos_intermediate(x) ((x) >= 300 && (x) < 400)
/* Check if reply code x is a transient negative reply. */
#define ftp_trans_neg(x) ((x) >= 400 && (x) < 500)
/* Check if reply code x is a perminent negative reply. */
#define ftp_perm_neg(x) ((x) >= 500 && (x) < 600)

#include <stdint.h>
#include <stddef.h>
#include "vector.h"

struct sockbuf {
    char data[512];
    size_t i;
    size_t size;
};

/* FTP server reply codes. */
enum reply_code {
    FTP_SERVER_BUSY = 120,
    FTP_SUPERFLUOUS = 202,
    FTP_SERVER_READY = 220,
    FTP_USER_LOGGED_IN = 230,
    FTP_NEED_PASS = 331,
    FTP_NEED_ACCT = 332,
    FTP_SERVER_NA = 421,
    FTP_NO_DATA_CONN = 425,
    FTP_CONN_CLOSED = 426,
    FTP_NO_ACTION = 450,
    FTP_ACTION_ABORTED = 451,
    FTP_SYNTAX_ERR = 500,
    FTP_SYNTAX_ERR_ARGS = 501,
    FTP_CMD_NOT_IMPL = 502,
    FTP_USER_LOGIN_FAIL = 530,
};

int getchar_from_sock(int sockfd, struct sockbuf *buf);
int connect_to_dtp(int sockpi, bool rpassive);
enum reply_code wait_for_reply(const int sockfd, struct vector *out_msg);
enum reply_code ftp_USER(int sockfd, const char *username);
enum reply_code ftp_PASS(int sockfd, const char *password);
enum reply_code ftp_QUIT(int sockfd);
enum reply_code ftp_HELP(int sockfd, const char *cmd, struct vector *reply_msg);
enum reply_code ftp_PWD(int sockfd, struct vector *reply_msg);
enum reply_code ftp_SYST(int sockfd, struct vector *reply_msg);
enum reply_code ftp_LIST(int sockfd, const char *path, struct vector *reply_msg);
enum reply_code ftp_CWD(int sockfd, const char *path);
enum reply_code ftp_PORT(int sockfd, const char *ipstr, uint16_t port);
enum reply_code ftp_PASV(int sockfd, struct vector *reply_msg);
enum reply_code ftp_RETR(int sockfd, const char *path, struct vector *reply_msg);
enum reply_code ftp_STOR(int sockfd, const char *path);
enum reply_code ftp_EPRT(int sockfd, int family, const char *ipstr, const uint16_t port);
enum reply_code ftp_EPSV(int sockfd, const uint16_t port);

#endif /* FTPC_FTP_H */
