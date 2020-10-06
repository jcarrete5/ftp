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

#define FATAL_ERROR -1
#define ILLEGAL_ARG -2

/* FTP server reply codes. */
enum reply_code {
    FTP_SERVER_BUSY = 120,
    FTP_SUPERFLUOUS = 202,
    FTP_SERVER_READY = 220,
    FTP_USER_LOGGED_IN = 230,
    FTP_NEED_PASS = 331,
    FTP_NEED_ACCT = 332,
    FTP_SERVER_NA = 421,
    FTP_SYNTAX_ERR = 500,
    FTP_SYNTAX_ERR_ARGS = 501,
    FTP_USER_LOGIN_FAIL = 530,
};

enum reply_code wait_for_reply(const int sockfd);
enum reply_code ftp_USER(int sockfd, const char *username);
enum reply_code ftp_PASS(int sockfd, const char *password);

#endif /* FTPC_FTP_H */
