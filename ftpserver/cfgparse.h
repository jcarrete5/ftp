/*
 * CS472 HW 4
 * Jason R. Carrete
 * cfgparse.h
 *
 * Header for cfgparse.c
 */

#ifndef FTPS_CFGPARSE_H
#define FTPS_CFGPARSE_H

/* Contains configuration information read from the ftps config. */
struct {
    bool port_mode_enabled;
    bool pasv_mode_enabled;
} ftps_config;

/* Read the ftps config file and set ftps_config appropriately. */
void read_cfg(void);

#endif /* FTPS_CFGPARSE_H */
