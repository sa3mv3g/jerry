# STM32H5 Ethernet Debug Guide

This document provides comprehensive debugging information for the STM32H5 Ethernet implementation with LwIP and TrustZone.

## Build Instructions

```bash
# Configure (only needed once or after CMakeLists.txt changes)
cmake -B build -G Ninja

# Build the application
cmake --build build --target jerry_app

# Output binary location
# build/application/jerry_app.elf
```

## Debug Output Format

The Ethernet driver outputs debug messages in the following format:
```
[ETH <tick>] <message>
```

Where `<tick>` is the HAL tick count in milliseconds.

### Key Debug Messages

| Message | Description |
|---------|-------------|
| `low_level_init: Starting` | Ethernet initialization starting |
| `MAC: XX:XX:XX:XX:XX:XX` | MAC address configured |
| `Link UP` | PHY link detected |
| `Link DOWN` | PHY link lost |
| `ETH started, Link UP` | Ethernet DMA started successfully |
| `HAL_ETH_Start_IT FAILED!` | Failed to start Ethernet DMA |
| `RxAlloc: idx=N, buff=0xXXXX` | Buffer N allocated for RX DMA |
| `RxAlloc FAILED!` | No free buffers available (will cause RBU) |
| `RxLink: buff=0xXXXX, len=N` | Received packet linked to pbuf |
| `RX: len=N, status=OK` | Packet received and processed |
| `TX: len=N` | Packet being transmitted |
| `RBU Error` | Receive Buffer Unavailable error |
| `TBU Error` | Transmit Buffer Unavailable error |
| `HAL_ETH_RxCpltCallback` | RX complete interrupt fired |

### DMA State Dump Format

```
[ETH xxxx] <context>: RPS=N TPS=N SR=N RE=N TE=N FBE=N RBU=N DMACSR=0xXXXXXXXX
[ETH xxxx]   RDLAR=0xXXXXXXXX RDRLR=N RDTPR=0xXXXXXXXX CARDR=0xXXXXXXXX
[ETH xxxx]   RxDesc[0]: DES0=0xXXXXXXXX DES3=0xXXXXXXXX (OWN=N IOC=N BUF1V=N)
```

#### Register Meanings

| Field | Description | Expected Value |
|-------|-------------|----------------|
| RPS | Receive Process State | 3 = Running (waiting for packets) |
| TPS | Transmit Process State | 3 = Running |
| SR | Start Receive bit | 1 = RX DMA enabled |
| RE | Receiver Enable bit | 1 = MAC receiver enabled |
| TE | Transmitter Enable bit | 1 = MAC transmitter enabled |
| FBE | Fatal Bus Error | 0 = No error |
| RBU | Receive Buffer Unavailable | 0 = No error |
| RDLAR | RX Descriptor List Address | Base address of RX descriptors |
| RDRLR | RX Descriptor Ring Length | Number of descriptors - 1 |
| RDTPR | RX Descriptor Tail Pointer | Address of last valid descriptor |
| CARDR | Current Application RX Descriptor | Current descriptor being processed |

#### RPS (Receive Process State) Values

| Value | State | Description |
|-------|-------|-------------|
| 0 | Stopped | RX DMA is stopped |
| 1 | Running (Fetching) | Fetching RX descriptor |
| 2 | Reserved | - |
| 3 | Running (Waiting) | Waiting for RX packet |
| 4 | Suspended | RX descriptor unavailable |
| 5 | Running (Closing) | Closing RX descriptor |
| 6 | Reserved | - |
| 7 | Running (Transferring) | Transferring RX packet to memory |

#### Descriptor Fields

| Field | Description | Expected Value |
|-------|-------------|----------------|
| DES0 | Buffer 1 Address | Valid buffer address in SRAM3 |
| DES3 | Control/Status | See below |
| OWN | Ownership bit | 1 = DMA owns descriptor |
| IOC | Interrupt on Completion | 1 = Generate interrupt |
| BUF1V | Buffer 1 Valid | 1 = Buffer address valid |

## Common Issues and Solutions

### Issue: RPS=0 (RX DMA Stopped)

**Symptoms:**
- `RPS=0` in debug output
- No packets received
- ARP may work (TX only) but ping fails

**Possible Causes:**
1. **GTZC Configuration**: SRAM3 not configured as non-secure/non-privileged
2. **Memory Protection**: MPU not configured correctly for DMA buffers
3. **Descriptor Initialization**: Descriptors not properly initialized with OWN bit

**Solution:**
Check GTZC configuration in Secure main.c:
```c
// SRAM3 must be non-secure AND non-privileged for ETH DMA
MPCBB_ConfigTypeDef MPCBB_NonSecureArea_Desc = {0};
MPCBB_NonSecureArea_Desc.SecureRWIllegalMode = GTZC_MPCBB_SRWILADIS_ENABLE;
MPCBB_NonSecureArea_Desc.InvertSecureState = GTZC_MPCBB_INVSECSTATE_NOT_INVERTED;
MPCBB_NonSecureArea_Desc.AttributeConfig.MPCBB_SecConfig_array[0] = 0x00000000;  // Non-secure
MPCBB_NonSecureArea_Desc.AttributeConfig.MPCBB_PrivConfig_array[0] = 0x00000000; // Non-privileged
MPCBB_NonSecureArea_Desc.AttributeConfig.MPCBB_LockConfig_array[0] = 0x00000000;
HAL_GTZC_MPCBB_ConfigMem(SRAM3_BASE, &MPCBB_NonSecureArea_Desc);
```

### Issue: RBU (Receive Buffer Unavailable) Errors

**Symptoms:**
- `RBU Error` messages in debug output
- Intermittent packet loss
- High interrupt rate

**Possible Causes:**
1. **Buffer Exhaustion**: All RX buffers assigned to DMA
2. **Slow Processing**: Application not processing packets fast enough
3. **Memory Corruption**: Buffer tracking corrupted

**Solution:**
1. Increase buffer count: `#define RX_BUFFER_COUNT (ETH_RX_DESC_CNT * 2)`
2. Check `RxBuffAssigned` bitmask in debug output
3. Ensure `HAL_ETH_RxLinkCallback` frees buffers properly

### Issue: No RX Interrupts

**Symptoms:**
- No `HAL_ETH_RxCpltCallback` messages
- `RxPktSemaphore received` never appears
- Packets visible on wire but not received

**Possible Causes:**
1. **NVIC Configuration**: ETH interrupt not enabled
2. **Interrupt Priority**: Priority too low
3. **TrustZone**: Interrupt routed to wrong world

**Solution:**
Check NVIC configuration:
```c
HAL_NVIC_SetPriority(ETH_IRQn, 5, 0);
HAL_NVIC_EnableIRQ(ETH_IRQn);
```

### Issue: TX Works but RX Doesn't

**Symptoms:**
- `TX: len=N` messages appear
- ARP requests sent (visible in Wireshark)
- No `RX: len=N` messages
- ARP table on PC shows MAC address

**Possible Causes:**
1. **RX DMA Not Running**: Check RPS value
2. **MAC Filter**: MAC address filter rejecting packets
3. **Descriptor Issue**: RX descriptors not owned by DMA

**Solution:**
1. Check `dump_rx_dma_state` output
2. Verify OWN bit is set in RX descriptors
3. Check MAC address matches expected value

## Memory Layout

### SRAM3 Usage (0x20050000 - 0x2005FFFF, 64KB)

| Section | Size | Description |
|---------|------|-------------|
| `.driver.eth_mac0_rx_desc` | 128 bytes | RX DMA Descriptors (8 × 16 bytes) |
| `.driver.eth_mac0_tx_desc` | 64 bytes | TX DMA Descriptors (4 × 16 bytes) |
| `.driver.eth_mac0_rx_buf` | 24KB | RX Buffers (16 × 1536 bytes) |

### Linker Script Configuration

```ld
.driver.eth_mac0_rx_desc (NOLOAD) : {
    . = ALIGN(4);
    *(.driver.eth_mac0_rx_desc)
    . = ALIGN(4);
} >RAM3

.driver.eth_mac0_tx_desc (NOLOAD) : {
    . = ALIGN(4);
    *(.driver.eth_mac0_tx_desc)
    . = ALIGN(4);
} >RAM3

.driver.eth_mac0_rx_buf (NOLOAD) : {
    . = ALIGN(32);
    *(.driver.eth_mac0_rx_buf)
    . = ALIGN(32);
} >RAM3
```

## Debug Checklist

When debugging Ethernet issues, check the following in order:

1. **PHY Link Status**
   - [ ] `Link UP` message appears
   - [ ] PHY LED indicates link

2. **DMA State**
   - [ ] RPS=3 (Running, waiting for packets)
   - [ ] SR=1 (Start Receive enabled)
   - [ ] RE=1 (Receiver enabled)
   - [ ] TE=1 (Transmitter enabled)
   - [ ] FBE=0 (No fatal bus error)
   - [ ] RBU=0 (No buffer unavailable)

3. **Descriptors**
   - [ ] OWN=1 for all RX descriptors
   - [ ] DES0 contains valid buffer addresses
   - [ ] BUF1V=1 for all descriptors

4. **Memory Configuration**
   - [ ] SRAM3 non-secure (GTZC)
   - [ ] SRAM3 non-privileged (GTZC)
   - [ ] MPU configured for DMA buffers

5. **Interrupts**
   - [ ] ETH_IRQn enabled
   - [ ] `HAL_ETH_RxCpltCallback` called
   - [ ] Semaphore signaled

6. **Network**
   - [ ] IP address configured (check DHCP or static)
   - [ ] Subnet mask correct
   - [ ] Gateway reachable (if needed)

## Enabling/Disabling Debug Output

In `ethernetif.c`:
```c
/* Enable debug logging - set to 1 to enable, 0 to disable */
#define ETH_DEBUG_ENABLE  1
```

Set to `0` for production builds to reduce overhead.

## Related Documentation

- [STM32H5 Reference Manual (RM0481)](../refs/rm0481-stm32h52333xx-stm32h56263xx-and-stm32h573xx-armbased-32bit-mcus-stmicroelectronics.pdf) - Chapter 61: Ethernet
- [LwIP Application Note (UM1713)](../refs/um1713-developing-applications-on-stm32cube-with-lwip-tcpip-stack-stmicroelectronics.pdf)
- [Ethernet DMA Explained](stm32h5_ethernet_dma_explained.md)
- [RBU Debug Log](ethernet_rbu_debug_log.md)
- [RBU Diagnostic Plan](ethernet_rbu_diagnostic_plan.md)

---

## Debug Session: Ping Not Working (ARP Works)

### Date: 2026-01-26

### Problem Description
- ARP is working: Board responds to ARP requests (TX: len=42)
- Ping is NOT working: No ICMP echo reply sent
- PC can see board's MAC address in ARP table
- Board receives ARP broadcasts (Type=0x0806)

### Test Configuration
- Static IP: 169.254.4.100
- Subnet: 255.255.0.0
- Gateway: 169.254.4.1
- TCP Echo Server on port 7

### Hardware Test Output

```
TCP Echo Task Started
Initializing LwIP...
Adding Network Interface...
[ETH 10] low_level_init: Starting...
[ETH 14] low_level_init: RxBuff at 0x20050180, size=8*1536
[ETH 19] low_level_init: DMARxDscrTab at 0x20050000
[ETH 24] low_level_init: DMATxDscrTab at 0x200500C0
[ETH 29] low_level_init: Initializing PHY...
[ETH 33] low_level_init: PHY link state = 1
[ETH 37] low_level_init: Link DOWN, deferring ETH start until link up
Initial Link status: DOWN
Static IP address set: 169.254.4.100
TCP Echo Server listening on port 7
[ETH 1746] check_link: Link UP (state=2)
[ETH 1750] reset_rx_descriptors: Cleared all 8 descriptors
[ETH 1755] RxAlloc: idx=0, buff=0x20050180, assigned=0x01
[ETH 1760] RxAlloc: idx=1, buff=0x20050780, assigned=0x03
[ETH 1765] RxAlloc: idx=2, buff=0x20050D80, assigned=0x07
[ETH 1770] RxAlloc: idx=3, buff=0x20051380, assigned=0x0F
[ETH 1775] RxAlloc: idx=4, buff=0x20051980, assigned=0x1F
[ETH 1780] RxAlloc: idx=5, buff=0x20051F80, assigned=0x3F
[ETH 1785] RxAlloc: idx=6, buff=0x20052580, assigned=0x7F
[ETH 1790] RxAlloc: idx=7, buff=0x20052B80, assigned=0xFF
[ETH 1795] HAL_ETH_Start_IT returned 0
[ETH 1795] DMA State after start:
[ETH 1795]   DMACSR=0x00000000
[ETH 1795]   DMACRCR=0x00200BE9 (SR=1, RE=1, TE=1)
[ETH 1795]   RPS=0 (Stopped)
[ETH 1795] WARNING: RPS=0 after HAL_ETH_Start_IT, attempting manual start
[ETH 1795] After manual start: RPS=0
[ETH 3480] HAL_ETH_RxCpltCallback
[ETH 3480] ethernetif_input_task: semaphore received
[ETH 3480] RxLink: buff=0x20050180, len=60
[ETH 3480]   Freed buffer idx=0, assigned=0xFE
[ETH 3480]   pbuf allocated: p=0x20000A28
[ETH 3480] RX: len=60, status=OK
[ETH 3480]   DST=FF:FF:FF:FF:FF:FF SRC=00:E0:4C:68:01:11 Type=0806
[ETH 3480] RxAlloc: idx=0, buff=0x20050180, assigned=0xFF
... (more ARP packets received)
[ETH 10107] TX: len=42
Stats - RX: 0, TX: 0, DROP: 0
```

### Analysis

#### What's Working
1. **Link UP** - PHY link established at tick 1746
2. **RX DMA** - Packets being received (HAL_ETH_RxCpltCallback fires)
3. **ARP RX** - ARP broadcast packets received (Type=0x0806)
4. **ARP TX** - ARP response sent (TX: len=42)
5. **Buffer management** - Buffers allocated, freed, and recycled correctly

#### What's NOT Working
1. **Ping (ICMP)** - No ICMP echo reply observed
2. **LwIP Stats** - Show RX: 0, TX: 0 (stats not incrementing)

### Packet Type Analysis

| Type | Hex | Description | Status |
|------|-----|-------------|--------|
| ARP | 0x0806 | Address Resolution Protocol | ✓ Working |
| IPv4 | 0x0800 | Internet Protocol v4 | ? Not tested |
| ICMP | (inside IPv4) | Ping request/reply | ✗ Not working |

### Possible Causes

1. **ICMP not enabled in LwIP**
   - Check `LWIP_ICMP` in lwipopts.h
   - Should be `#define LWIP_ICMP 1`

2. **IP packet not reaching LwIP**
   - ARP works at Layer 2
   - ICMP requires Layer 3 (IP) processing
   - Check if IPv4 packets (Type=0x0800) are received

3. **Packet not passed to LwIP stack**
   - `low_level_input()` returns pbuf
   - `ethernetif_input()` should call `netif->input()`
   - Check if `ethernet_input()` is being called

4. **IP address mismatch**
   - Board IP: 169.254.4.100
   - PC must be on same subnet (169.254.x.x)
   - Check PC IP configuration

5. **MAC filter issue**
   - Board may be filtering unicast packets
   - ARP broadcasts (FF:FF:FF:FF:FF:FF) pass
   - Unicast ICMP may be filtered

### Debug Steps

1. **Check lwipopts.h for ICMP**
   ```c
   #define LWIP_ICMP 1
   ```

2. **Add debug for IPv4 packets**
   - Look for Type=0x0800 in RX debug output
   - If not seen, MAC filter may be blocking

3. **Check netif->input callback**
   - Should be `ethernet_input` for Ethernet
   - Verify in `netif_add()` call

4. **Test with Wireshark**
   - Capture on PC side
   - Verify ICMP request is sent
   - Check if ICMP reply is received

### Root Cause Identified: MAC Address Mismatch

**The MAC address in HAL ETH Init did not match the MAC address in ethernetif.c!**

**In `MX_ETH_Init()` (NonSecure/Core/Src/main.c):**
```c
MACAddr[5] = 0x00;  // <-- WRONG: Last byte was 0x00
```

**In `ethernetif.c`:**
```c
#define ETH_MAC_ADDR5   0x01U  // <-- Last byte is 0x01
```

### Why This Caused Ping to Fail

1. PC sends ARP request: "Who has 169.254.4.100?"
2. Board responds with ARP reply containing MAC `00:80:E1:00:00:01` (from netif)
3. PC updates ARP table: 169.254.4.100 → 00:80:E1:00:00:01
4. PC sends ICMP ping to MAC `00:80:E1:00:00:01`
5. **ETH hardware filter rejects the packet** because it's configured for `00:80:E1:00:00:00`
6. Ping fails - no ICMP packets reach LwIP

ARP broadcasts worked because they're sent to `FF:FF:FF:FF:FF:FF` which passes the MAC filter.

### Fix Applied

Changed `MACAddr[5]` from `0x00` to `0x01` in `application/bsp/stm32h563/NonSecure/Core/Src/main.c`:

```c
MACAddr[5] = 0x01;  /* Must match ETH_MAC_ADDR5 in ethernetif.c */
```

### Lesson Learned

**Always ensure the MAC address is consistent between:**
1. HAL ETH Init (`heth.Init.MACAddr`) - configures hardware MAC filter
2. LwIP netif (`netif->hwaddr`) - used in ARP responses

If they don't match, the hardware will filter out unicast packets addressed to the netif MAC.

### Build Status

Build succeeded with the fix applied.

### Next Steps

1. [x] Verify `LWIP_ICMP` is enabled in lwipopts.h - YES, enabled
2. [x] Identified root cause - MAC address mismatch
3. [x] Applied fix - MAC addresses now match
4. [ ] Test ping from PC to board (169.254.4.100)
5. [ ] Test TCP echo server on port 7
