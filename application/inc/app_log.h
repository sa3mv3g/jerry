#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdarg.h>

#define LOG_LEVEL_VERBOSE 0
#define LOG_LEVEL_DEBUG   1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_ERROR   4
#define LOG_LEVEL_NONE    5

/* Syslog severities (RFC 5424) */
#define SYSLOG_SEV_EMERG  0
#define SYSLOG_SEV_ALERT  1
#define SYSLOG_SEV_CRIT   2
#define SYSLOG_SEV_ERR    3
#define SYSLOG_SEV_WARN   4
#define SYSLOG_SEV_NOTICE 5
#define SYSLOG_SEV_INFO   6
#define SYSLOG_SEV_DEBUG  7

#define LOG_MAX_MSG_LEN (256U)

#define LOG_ERR(fmt, ...) \
    AppLog_Message(LOG_LEVEL_ERROR, SYSLOG_SEV_ERR, "E", fmt, ##__VA_ARGS__)

#define LOG_WRN(fmt, ...) \
    AppLog_Message(LOG_LEVEL_WARNING, SYSLOG_SEV_WARN, "W", fmt, ##__VA_ARGS__)

#define LOG_INF(fmt, ...) \
    AppLog_Message(LOG_LEVEL_INFO, SYSLOG_SEV_INFO, "I", fmt, ##__VA_ARGS__)

#define LOG_DBG(fmt, ...) \
    AppLog_Message(LOG_LEVEL_DEBUG, SYSLOG_SEV_DEBUG, "D", fmt, ##__VA_ARGS__)

#define LOG_VERBOSE(fmt, ...) \
    AppLog_Message(LOG_LEVEL_VERBOSE, SYSLOG_SEV_DEBUG, "V", fmt, ##__VA_ARGS__)

void AppLog_Init(void);

/**
 * @brief Sends a syslog message over UDP using RFC 5424 format.
 *
 * Transmits a fully-formatted syslog message to the configured remote syslog
 * server via UDP. The message should already be constructed according to
 * RFC 5424.
 *
 * @param[in] msg       Null-terminated string containing the fully formatted
 *                      log message (including syslog header). Must not be NULL.
 *
 * @note This function must only be called after the network interface is up
 *       and @ref Syslog_Init() has been successfully called.
 * @note This function is not ISR-safe. Call only from task context.
 * @note If the network is unavailable, the message is silently dropped.
 *
 * @see Syslog_Init()
 */
void Applog_Syslog(const char* msg);

void AppLog_Message(int level, int syslog_severity, const char* level_str,
                    const char* fmt, ...);

void AppLog_SetLevel(int level);
int  AppLog_GetLevel(void);

#endif  // APP_LOG_H
