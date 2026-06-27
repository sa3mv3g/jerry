# [N-06] `tcp_echo_thread()` ignores `netconn_write()` errors and can spin on a broken connection

**Severity:** 🟠 High
**Component:** TCP echo server
**File:** `application/src/tcp_echo_task.c` (~139-148)

## Description

```c
while ((err = netconn_recv(newconn, &buf)) == ERR_OK)
{
    do {
        netbuf_data(buf, &data, &len);
        err = netconn_write(newconn, data, len, NETCONN_COPY);
    } while (netbuf_next(buf) >= 0);
    netbuf_delete(buf);
}
```

The `netconn_write()` result is overwritten by the loop and never checked, so a failed write on a half-open/errored connection is ignored and the loop keeps spinning. The `netbuf_next()` return-value handling should also be verified against the lwIP contract.

## Impact

On write errors the echo loop spins; partial data may be echoed with no backpressure handling.

## Fix

Check `netconn_write()` and break on error. Verify `netbuf_next()` handling.
