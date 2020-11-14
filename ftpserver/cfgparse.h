/*
 * CS472 HW 4
 * Jason R. Carrete
 * cfgparse.h
 *
 * Header for cfgparse.c
 */

#ifndef FTPS_CFGPARSE_H
#define FTPS_CFGPARSE_H

struct config {
    bool port_mode_enabled;
    bool pasv_mode_enabled;
};

/* Contains configuration information read from the ftps config. */
extern struct config ftps_config;

/* Read the ftps config file and set ftps_config appropriately. */
void read_cfg(void);

#endif /* FTPS_CFGPARSE_H */
