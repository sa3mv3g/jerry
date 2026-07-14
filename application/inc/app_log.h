#ifndef APP_LOG_H
#define APP_LOG_H

#include "log.h"
#include "log_cfg.h"

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

#endif  // APP_LOG_H