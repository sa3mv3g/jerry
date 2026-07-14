import socket
import datetime

# ── Configuration ──────────────────────────────────────────────
HOST = "0.0.0.0"   # Listen on all interfaces
PORT = 514          # Standard syslog port (use 5140 if non-root)
BUFFER_SIZE = 4096
LOG_FILE = "syslog.log"

# ── Severity & Facility Maps ────────────────────────────────────
SEVERITY = {
    0: "EMERGENCY", 1: "ALERT",    2: "CRITICAL", 3: "ERROR",
    4: "WARNING",   5: "NOTICE",   6: "INFO",      7: "DEBUG"
}

FACILITY = {
    0: "KERN",   1: "USER",   2: "MAIL",   3: "DAEMON",
    4: "AUTH",   5: "SYSLOG", 6: "LPR",    7: "NEWS",
    8: "UUCP",   9: "CRON",  10: "AUTHPRIV", 16: "LOCAL0",
    17: "LOCAL1", 18: "LOCAL2", 19: "LOCAL3", 20: "LOCAL4",
    21: "LOCAL5", 22: "LOCAL6", 23: "LOCAL7"
}

# ── Parse RFC 5424 / RFC 3164 ───────────────────────────────────
def parse_syslog(data: str) -> dict:
    result = {"raw": data}

    try:
        if data.startswith("<"):
            pri_end = data.index(">")
            pri = int(data[1:pri_end])
            result["facility"]  = FACILITY.get(pri // 8, str(pri // 8))
            result["severity"]  = SEVERITY.get(pri % 8,  str(pri % 8))
            remainder = data[pri_end + 1:]

            # RFC 5424: <PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID [SD] MSG
            parts = remainder.split(" ", 7)
            if parts[0].isdigit():  # RFC 5424
                result["version"]   = parts[0]
                result["timestamp"] = parts[1] if len(parts) > 1 else "-"
                result["hostname"]  = parts[2] if len(parts) > 2 else "-"
                result["appname"]   = parts[3] if len(parts) > 3 else "-"
                result["procid"]    = parts[4] if len(parts) > 4 else "-"
                result["msgid"]     = parts[5] if len(parts) > 5 else "-"
                result["msg"]       = parts[7] if len(parts) > 7 else parts[6] if len(parts) > 6 else ""
            else:  # RFC 3164
                result["version"]   = "3164"
                result["msg"]       = remainder
    except Exception as e:
        result["parse_error"] = str(e)

    return result

# ── Format for File ─────────────────────────────────────────────
def format_log(parsed: dict, sender_ip: str) -> str:
    ts       = parsed.get("timestamp", datetime.datetime.utcnow().isoformat() + "Z")
    facility = parsed.get("facility", "?")
    severity = parsed.get("severity", "?")
    hostname = parsed.get("hostname", sender_ip)
    appname  = parsed.get("appname",  "-")
    procid   = parsed.get("procid",   "-")
    msgid    = parsed.get("msgid",    "-")
    msg      = parsed.get("msg",      parsed.get("raw", ""))

    return (f"[{ts}] [{facility}.{severity}] "
            f"HOST={hostname} APP={appname} PID={procid} MSGID={msgid} | {msg}")

# ── Main Server ─────────────────────────────────────────────────
def start_server():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((HOST, PORT))

    with open(LOG_FILE, "a") as log_file:
        while True:
            try:
                data, addr = sock.recvfrom(BUFFER_SIZE)
                sender_ip  = addr[0]
                message    = data.decode("utf-8", errors="replace").strip()

                parsed     = parse_syslog(message)
                log_line   = format_log(parsed, sender_ip)

                log_file.write(log_line + "\n")
                log_file.flush()

            except KeyboardInterrupt:
                break
            except Exception as e:
                log_file.write(f"[ERROR] {datetime.datetime.utcnow().isoformat()}Z - {e}\n")
                log_file.flush()

if __name__ == "__main__":
    start_server()