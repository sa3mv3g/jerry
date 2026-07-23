# [BUG-20] Firmware Modbus Server Missing FIN / Half-Open TCP Socket Leak on New Connections

**Severity:** 🔴 Critical
**Component:** Modbus TCP Server / Network Task
**File:** [`application/src/modbus_task.c`](application/src/modbus_task.c)

---

## Executive Summary

When a Modbus TCP connection stalls or becomes abandoned without sending a `FIN` (a common occurrence with network instability or browser tab closures), the firmware's sequential server architecture blocked all subsequent client connection attempts due to a 30-second absolute idle timeout (`MODBUS_RECV_TIMEOUT_MS = 30000ms`). 

While the background TCP stack (lwIP) successfully completes the 3-way handshake with new clients, the application layer was stuck handling the old stalled connection and could not accept the new ones. When the new client timed out and attempted to close the connection by sending a `FIN`, the device acknowledged it at the TCP level (moving to `CLOSE_WAIT`), but **never sent its own `FIN` back** because the application had not accepted the connection and could not call `netconn_close()`. This leaked the socket in a permanent half-open state, causing dashboard disconnect promises to hang indefinitely.

Under high-throughput burst testing (e.g., `test_modbus_performance.py`), the sequential model suffers from a critical **TCP Delayed-ACK Memory Deadlock**, which completely exhausts the `PBUF_POOL` and locks up the entire Ethernet MAC/DMA interface, freezing all network traffic (including ARP and UDP Syslogs) without resetting the device.

---

## Step-by-Step Packet Analysis & Root Cause

### Scenario A: Stalled Dashboard Connections (`yello.pcapng` / `bug.pcapng`)

1. **Stalled Connections & TCP Keep-Alives:**
   Older connections (e.g., source ports `57464` and `63876`) have stalled. The dashboard sends TCP Keep-Alives to verify if the device is alive. Because the device's TCP stack is running, it automatically sends ACKs, but the application remains unaware and has not closed these old sockets.

2. **The "Hostage Connection" Deadlock (Sequential Server Loop):**
   The server task is implemented as a sequential blocking loop in [`application/src/modbus_task.c`](application/src/modbus_task.c):
   ```c
   while (1)
   {
       struct netconn *new_conn;
       err = netconn_accept(listen_conn, &new_conn);

       if (err == ERR_OK)
       {
           LOG_INF("[Modbus] New connection accepted");
           netconn_set_recvtimeout(new_conn, 1000U);
           modbus_handle_connection(new_conn); // BLOCKS HERE FOR UP TO 30s
           netconn_close(new_conn);
           netconn_delete(new_conn);
           LOG_INF("[Modbus] Connection closed");
       }
   }
   ```
   If Client A (stalled) doesn't send any data, the thread is blocked inside `modbus_handle_connection()` calling `netconn_recv(conn_A)`. It remains blocked there for up to **30 seconds** of absolute inactivity before hitting the idle timeout.

3. **New Connection Attempt (59.3s):**
   The dashboard attempts to open a new connection on port `56572`.
   - **TCP Layer:** lwIP successfully completes the SYN, SYN-ACK, ACK handshake in the background. The connection enters `ESTABLISHED` and is placed into `listen_conn`'s backlog queue.
   - **Application Layer:** The Modbus thread is **still blocked** inside `modbus_handle_connection()` for Client A, so it does NOT call `netconn_accept()` to accept Client B.

4. **No Modbus Responses & Retransmissions:**
   The dashboard sends a `Read Input Registers` (TID 1) request over port `56572`.
   - lwIP's TCP layer ACKs the packet and buffers the data.
   - But since the application hasn't accepted Client B, no one reads from the socket.
   - The dashboard receives no Modbus payload, assumes packet loss, and retransmits. The device ACKs the retransmission but still provides no application-level response.

5. **The Missing FIN (60.06s):**
   The dashboard gives up after timeouts (0.76 seconds later) and sends a `FIN, ACK` packet to close port `56572`.
   - lwIP's TCP stack receives the `FIN` and automatically responds with an `ACK`.
   - This moves Client B's socket to the `CLOSE_WAIT` state on the device.
   - In TCP, a socket in `CLOSE_WAIT` represents a **half-closed** connection. The device's TCP stack is waiting for the *application* to call `netconn_close()` to signal that it is finished.
   - Because the application is still blocked handling Client A, it has **never accepted** Client B, does not have a reference to Client B's `netconn`, and **cannot** call `netconn_close()`.
   - The device never sends its own `FIN` back, leaving Client B's socket leaked in `CLOSE_WAIT` forever.

---

### Scenario B: High-Throughput Delayed-ACK Deadlock (`perf.pcapng`)

When running a sustained burst performance test, the device successfully processes about 1600+ requests back-to-back before **completely freezing network communications** (ignoring ARP requests and stopping UDP Syslogs):

1. **TCP Delayed ACKs:**
   Standard TCP clients do not acknowledge every tiny packet immediately; they wait up to 200ms or until 2 segments are received before sending a TCP ACK.
2. **Send Buffer Accumulation:**
   The client polls every 2ms. Within the 200ms Delayed-ACK window, the client sends dozens of requests. The device processes them and calls `netconn_write(..., NETCONN_COPY)` for each.
   - `NETCONN_COPY` copies the data by allocating a small segment from lwIP's static memory heap `MEM_SIZE` (which is configured at a very small **8 KB** in `lwipopts.h`).
   - Because the client has not ACKed them yet, lwIP must buffer these outstanding segments in its send queue.
3. **PBUF Pool Exhaustion:**
   If more than 48 segments accumulate in the send queue, they consume all available `pbuf` structures in the static `PBUF_POOL` (configured at **48 buffers** in `lwipopts.h`).
4. **The Deadlock:**
   Once `PBUF_POOL` is empty, the Ethernet input task (`ethernetif_input_task` running `pbuf_alloc(PBUF_RAW, Length, PBUF_POOL)`) can **no longer allocate any `pbuf` to read incoming packets from the DMA**.
   - Consequently, the client's subsequent TCP `ACK` packets are silently dropped at the Ethernet driver level.
   - Because the `ACK`s are dropped, the TCP stack can never free the buffered segments in its send queue.
   - The device network stack enters an unrecoverable **deadlock**. Since the watchdog is refreshed inside `modbus_handle_connection()`, which blocks on the dead socket, the hardware watchdog never resets the device!

---

## Parallel Multi-Task Pitfalls (The Thread-Safety Risk)

Attempting to spawn multiple worker tasks (parallel execution) to process connections concurrently is highly dangerous in this codebase due to the design of the core Modbus parsing layer:

* **Static Variable Data Corruption:**
  The request parsing function `modbus_process_request()` uses `static` structures to avoid large stack allocations (which is required by the strict stack size limits in FreeRTOS):
  ```c
  static modbus_adu_t request_adu;
  static modbus_adu_t response_adu;
  static modbus_pdu_t response_pdu;
  ```
  If multiple worker tasks run in parallel, they concurrently modify these static buffers. A request processed by Worker Task 1 will overwrite and corrupt the ADU/PDU data being processed by Worker Task 0, causing **severe transaction corruption, incorrect register operations, and sudden connection drops**.
* **Stack Size Limits:**
  To make `modbus_process_request()` thread-safe, these large structs (totalling over 770 bytes) would have to be moved to the task stacks. This would exceed the tight, statically allocated FreeRTOS stack limits (`MODBUS_TASK_STACK_SIZE`), triggering stack overflows.

---

## Recommended Solutions for the Firmware Team

To maintain 100% thread-safety, avoid dynamic memory allocation, and completely eliminate both the half-open socket bug and high-throughput deadlocks, the firmware team must implement the following:

### 1. Shorten the Modbus Idle Timeout
Shorten `MODBUS_RECV_TIMEOUT_MS` from `30000ms` to **`3000ms` (3 seconds)**. If a client stalls, the sequential server closes and discards it within 3 seconds, quickly returning to accept queue and processing any new connection waiting in the backlog with zero noticeable delay.

### 2. Enable TCP Keep-Alives on Sockets
Globally enable keep-alives via `LWIP_TCP_KEEPALIVE` in `lwipopts.h`, and configure keep-alives on accepted connection PCBs:
```c
#if LWIP_TCP_KEEPALIVE
if (new_conn->pcb.tcp != NULL)
{
    ip_set_option(new_conn->pcb.ip, SOF_KEEPALIVE);
    new_conn->pcb.tcp->keep_idle  = 3000U; /* 3 seconds */
    new_conn->pcb.tcp->keep_intvl = 1000U; /* 1 second */
    new_conn->pcb.tcp->keep_cnt   = 3U;    /* 3 tries */
}
#endif
```
This ensures that the transport layer automatically actively probes and recycles stalled connections with no application-level blockages.

### 3. Tune LwIP Sizing to Prevent Delayed-ACK Deadlocks
Under `lwipopts.h`, adjust the memory configurations to withstand tight performance bursts:
* **Increase LwIP Heap size (`MEM_SIZE`):** Increase the heap from `8 * 1024` to **`16 * 1024` (16 KB)** to give the segment allocator more headroom.
* **Increase PBUF Pool size (`PBUF_POOL_SIZE`):** Increase the pool size from `48` to **`64` or `96`** to ensure plenty of packet buffers remain available for the RX driver during heavy TX bursts.
* **Enable TCP_NODELAY on the socket:** Disable Nagle's algorithm on the Modbus server socket in `modbus_task.c` to force immediate segment transmission and minimize buffered queues.
