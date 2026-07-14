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
#define SYSLOG_SEV_EMERG   0
#define SYSLOG_SEV_ALERT   1
#define SYSLOG_SEV_CRIT    2
#define SYSLOG_SEV_ERR     3
#define SYSLOG_SEV_WARN    4
#define SYSLOG_SEV_NOTICE  5
#define SYSLOG_SEV_INFO    6
#define SYSLOG_SEV_DEBUG   7

void AppLog_Init(void);

/**
 * @brief Sends a syslog message over UDP using RFC 5424 format.
 *
 * Formats and transmits a syslog message to the configured remote syslog
 * server via UDP. The message is constructed according to RFC 5424, with
 * the priority value derived from the facility and severity parameters.
 *
 * @param[in] facility  Syslog facility code indicating the source of the
 * message. Valid range: 0–23. Common values:
 *                      - 0  : Kernel messages
 *                      - 1  : User-level messages
 *                      - 16 : Local use 0 (local0)
 *                      - 23 : Local use 7 (local7)
 *
 * @param[in] severity  Syslog severity level of the message.
 *                      Valid range: 0–7.
 *                      Common values:
 *                      - 0 : Emergency
 *                      - 1 : Alert
 *                      - 2 : Critical
 *                      - 3 : Error
 *                      - 4 : Warning
 *                      - 5 : Notice
 *                      - 6 : Informational
 *                      - 7 : Debug
 *
 * @param[in] msg       Null-terminated string containing the log message body.
 *                      Must not be NULL.
 *
 * @note This function must only be called after the network interface is up
 *       and @ref Syslog_Init() has been successfully called.
 * @note This function is not ISR-safe. Call only from task context.
 * @note If the network is unavailable, the message is silently dropped.
 *
 * @par Example
 * @code
 *     Applog_Syslog(1, 6, "System initialized successfully");
 *     Applog_Syslog(1, 3, "ADC read failed");
 * @endcode
 *
 * @see Syslog_Init()
 */
void Applog_Syslog(int facility, int severity, const char* msg);

void AppLog_Message(int level, int syslog_severity, const char* level_str, const char* fmt, ...);

void log_set_level(int level);
int log_get_level(void);

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

#endif  // APP_LOG_H
