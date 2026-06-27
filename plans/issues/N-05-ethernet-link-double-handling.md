# [N-05] Ethernet link handled twice — 10 ms polling plus a registered link callback

**Severity:** 🟠 High
**Component:** Ethernet / lwIP port
**File:** `application/src/tcp_echo_task.c` (~63-89, 91-104, 220-227)

## Description

`vEthernetTask` calls `ethernetif_check_link(netif)` every 10 ms while `link_callback()` is also registered via `netif_set_link_callback()`. Depending on the port, `ethernetif_check_link()` may itself invoke the callback, so link up/down is handled from two contexts. The 10 ms cadence at priority `tskIDLE_PRIORITY + 2` (same as Modbus) also adds needless MDIO traffic and priority contention.

## Impact

Redundant link processing, possible log spam, CPU/MDIO overhead, contention with the Modbus server.

## Fix

Pick one mechanism: rely on the event-driven link callback, or poll at a slower cadence (e.g., 500 ms-1 s). Do not run both at 10 ms.
