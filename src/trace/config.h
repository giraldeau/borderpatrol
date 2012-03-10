//
// $Id: config.h,v 1.21 2009-02-02 05:06:07 ning Exp $
//
// configuration
//

#ifndef CONFIG_H
#define CONFIG_H

#include "syscalls.h"

// #################################################################

#define MAX_LOG_LINE             1023
#define EPOCH_SECONDS      1144765785
#define DEBUG_STRACE_KIDS           0

#define LOG_DIR     "/tmp"
#define RELAY_ON    "/mnt/relay/pfdura/on"
#define RELAY_OFF   "/mnt/relay/pfdura/off"
#define RELAY_FEED  "/mnt/relay/pfdura/cpu0"
#define LOGD_IP     "127.0.0.1"
#define LOGD_PORT   7070
#define LOGD_PIPE   "/tmp/fifo"
#define LOGD_BUF    (PIPE_BUF - 2)
#define LOGD_USE_SOCK

#define LOGD_BUF_MAX ((int)((PIPE_BUF - 2) / sizeof(logentry_t)))

#define DEBUG       0

// #################################################################

#define PP_NULL_ID 0
#define PP_PERLS_ID 1
#define PP_PERLC_ID 2
#define PP_HTTPS_ID 3
#define PP_HTTPC_ID 4
#define PP_PSQLS_ID 5
#define PP_PSQLC_ID 6
#define PP_FCGIS_ID 7
#define PP_FCGIC_ID 8
#define PP_XS_ID 9              /* Unused unless we trace X server */
#define PP_XC_ID 10
#define PP_MYSQLS_ID 11
#define PP_MYSQLC_ID 12
#define PP_TRIVIAL_ID 99

// #################################################################

#endif
