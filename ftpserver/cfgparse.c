/*
 * CS472 HW 4
 * Jason R. Carrete
 * cfgparse.c
 *
 * This module handles parsing the ftps configuration file and storing the
 * settings to be read later.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "log.h"
#include "cfgparse.h"

#define CFG_FILE "out/etc/ftps.conf"
#define MAX_LINE_LEN (128U)

struct config ftps_config = {
    .port_mode_enabled=true,
    .pasv_mode_enabled=true
};

/* Return true if line is blank, otherwise false. */
static bool is_blank(const char *line) {
    for (const char *p = line; *p; p++) {
        if (!isspace(*p)) {
            return false;
        }
    }
    return true;
}

/*
 * Parse a value argument as true or false. YES is true, NO is false
 * (ignores case).
 */
static bool parse_value(const char *value, unsigned lineno) {
    if (strcasecmp(value, "YES") == 0) {
        return true;
    } else if (strcasecmp(value, "NO") == 0) {
        return false;
    } else {
        logerr("Main: config file line %u invalid value '%s'", lineno, value);
        exit(EXIT_FAILURE);
    }
}

/* Update ftps_config key with value. */
static void update_cfg(const char *key, const char *value, unsigned lineno) {
    if (!(key && value)) {
        logerr("Main: config file line %u invalid", lineno);
        exit(EXIT_FAILURE);
    }
    if (strcmp(key, "port_mode") == 0) {
        ftps_config.port_mode_enabled = parse_value(value, lineno);
    } else if (strcmp(key, "pasv_mode") == 0) {
        ftps_config.pasv_mode_enabled = parse_value(value, lineno);
    }
}

/* Read the ftps config file and set ftps_config appropriately. */
void read_cfg(void) {
    FILE *conffile = fopen(CFG_FILE, "r");
    if (!conffile) {
        logerr("Main: failed to open configuration file (fopen: %s)", strerror(errno));
        exit(EXIT_FAILURE);
    }
    char line[MAX_LINE_LEN];
    unsigned lineno = 1;
    while (fgets(line, sizeof line, conffile)) {
        if (line[0] == '#' || is_blank(line)) {
            lineno++;
            continue;
        }
        char *key = strtok(line, " =\t");
        char *value = strtok(NULL, " =\t\n");
        update_cfg(key, value, lineno++);
    }
    fclose(conffile);
    if (!ftps_config.pasv_mode_enabled && !ftps_config.port_mode_enabled) {
        logerr("Main: at least one of port_mode or pasv_mode must be enabled in configuration");
        exit(EXIT_FAILURE);
    }
    loginfo("Main: configuration file loaded");
}
