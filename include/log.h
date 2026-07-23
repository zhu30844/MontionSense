#ifndef MOTIONSENSE_LOG_H
#define MOTIONSENSE_LOG_H

#include <syslog.h>

#ifndef LOG_TAG
#define LOG_TAG "MotionSense"
#endif

/*
 * Logs go to syslog (facility LOG_DAEMON, see openlog() in main.c) and land in
 * /var/log/messages on the device. Live-tail while debugging:
 *     adb shell 'tail -f /var/log/messages | grep MotionSense'
 * Filter by level via the syslog priority prefix, e.g. daemon.err / daemon.warning.
 */
/* The "MotionSense" tag comes from openlog()'s ident, so it is not repeated
 * here -- only the function name is prefixed: "MotionSense[pid]: [func] msg". */
#define MS_LOG_INFO(fmt, ...)  syslog(LOG_INFO,    "[%s] " fmt, __func__, ##__VA_ARGS__)
#define MS_LOG_WARN(fmt, ...)  syslog(LOG_WARNING, "[%s] " fmt, __func__, ##__VA_ARGS__)
#define MS_LOG_ERROR(fmt, ...) syslog(LOG_ERR,     "[%s] " fmt, __func__, ##__VA_ARGS__)
#define MS_LOG_DEBUG(fmt, ...) syslog(LOG_DEBUG,   "[%s] " fmt, __func__, ##__VA_ARGS__)

#endif
