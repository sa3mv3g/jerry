# STM32H5 Ethernet DMA Architecture Explained

This document explains the Ethernet DMA architecture on STM32H5 microcontrollers and the concepts involved in debugging RBU (Receive Buffer Unavailable) errors.

## Table of Contents

1. [Overview](#1-overview)
2. [DMA (Direct Memory Access) Concept](#2-dma-direct-memory-access-concept)
3. [DMA Descriptors](#3-dma-descriptors)
4. [OWN Bit - The Key Concept](#4-own-bit---the-key-concept)
5. [RBU (Receive Buffer Unavailable) Error](#5-rbu-receive-buffer-unavailable-error)
6. [Key DMA Registers](#6-key-dma-registers)
7. [Memory Barriers](#7-memory-barriers-__dsb-__dmb-__isb)
8. [MPU Configuration](#8-mpu-memory-protection-unit-configuration)
9. [NVIC (Interrupt Controller)](#9-nvic-nested-vectored-interrupt-controller)
10. [Complete RX Data Flow](#10-complete-rx-data-flow)
11. [Code Locations: CPU Descriptor Operations](#11-code-locations-cpu-descriptor-operations)
12. [Memory Layout](#12-our-memory-layout)
13. [The RBU Fix](#13-summary-of-our-fix)

---

## 1. Overview

The STM32H5 Ethernet peripheral consists of three main components:

```
┌─────────────────────────────────────────────────────────────────┐
│                        STM32H5 MCU                              │
│  ┌─────────┐    ┌─────────────┐    ┌─────────────────────────┐  │
│  │  CPU    │◄──►│   AHB Bus   │◄──►│    Ethernet MAC + DMA   │  │
│  │(Cortex  │    │             │    │  ┌─────────┐ ┌───────┐  │  │
│  │  M33)   │    │             │    │  │   MAC   │ │  DMA  │  │  │
│  └─────────┘    └─────────────┘    │  └────┬────┘ └───┬───┘  │  │
│       │                            │       │          │      │  │
│       ▼                            │       ▼          ▼      │  │
│  ┌─────────┐                       │  ┌─────────────────┐    │  │
│  │  SRAM   │◄──────────────────────┼──│  Descriptors &  │    │  │
│  │(Buffers)│                       │  │    Buffers      │    │  │
│  └─────────┘                       │  └─────────────────┘    │  │
│                                    └──────────┬──────────────┘  │
└───────────────────────────────────────────────┼─────────────────┘
                                                │ RMII Interface
                                                ▼
                                    ┌───────────────────┐
                                    │   PHY (LAN8742)   │
                                    └─────────┬─────────┘
                                              │
                                              ▼
                                    ┌───────────────────┐
                                    │  Ethernet Cable   │
                                    └───────────────────┘
```

### Components:

- **MAC (Media Access Controller)**: Handles Ethernet frame encoding/decoding, CRC, addressing
- **DMA (Direct Memory Access)**: Transfers data between MAC and memory without CPU
- **PHY (Physical Layer)**: Converts digital signals to/from electrical signals on the wire
- **RMII (Reduced Media Independent Interface)**: Interface between MAC and PHY

---

## 2. DMA (Direct Memory Access) Concept

### What is DMA?

DMA allows hardware peripherals to transfer data directly to/from memory without CPU involvement. This is crucial for high-speed networking because:

- **Without DMA**: CPU must copy every byte from the Ethernet peripheral to memory (very slow, wastes CPU cycles)
- **With DMA**: Hardware automatically transfers data while CPU does other work

### How Ethernet DMA Works:

```
┌───────────────────────────────────────────────────────────────────┐
│                    DMA Data Flow                                  │
│                                                                   │
│  RECEIVE (RX):                                                    │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌──────────┐        │
│  │ Network │───►│   PHY   │───►│   MAC   │───►│   DMA    │        │
│  │ Packet  │    │(LAN8742)│    │(Decode) │    │(Transfer)│        │
│  └─────────┘    └─────────┘    └─────────┘    └─────┬────┘        │
│                                                     │             │
│                                                     ▼             │
│                                              ┌─────────────┐      │
│                                              │ RX Buffer   │      │
│                                              │ (in SRAM)   │      │
│                                              └─────────────┘      │
│                                                                   │
│  TRANSMIT (TX):                                                   │
│  ┌─────────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐     │
│  │ TX Buffer   │───►│   DMA   │───►│   MAC   │───►│   PHY   │───► │
│  │ (in SRAM)   │    │(Transfer)│   │(Encode) │    │(LAN8742)│     │
│  └─────────────┘    └─────────┘    └─────────┘    └─────────┘     │
└───────────────────────────────────────────────────────────────────┘
```

### Benefits of DMA:

1. **CPU Offloading**: CPU is free to do other tasks while DMA handles data transfer
2. **High Throughput**: DMA can sustain 100 Mbps without CPU bottleneck
3. **Low Latency**: No software overhead for each byte transferred
4. **Deterministic**: Hardware timing is predictable

---

## 3. DMA Descriptors

### What are Descriptors?

Descriptors are small data structures in memory that tell the DMA where to find/put data. Think of them as "work orders" for the DMA.

### Descriptor Structure (24 bytes each):

```c
typedef struct {
    uint32_t DESC0;      // Buffer address (where data goes/comes from)
    uint32_t DESC1;      // Reserved
    uint32_t DESC2;      // Buffer 2 address (for scatter-gather)
    uint32_t DESC3;      // Control/Status bits (OWN, IOC, BUF1V, etc.)
    uint32_t BackupAddr0; // HAL uses this to track buffer address
    uint32_t BackupAddr1; // HAL uses this for buffer 2
} ETH_DMADescTypeDef;
```

### DESC3 Bit Fields (for RX):

| Bit | Name | Description |
|-----|------|-------------|
| 31 | OWN | Ownership - 1=DMA owns, 0=CPU owns |
| 30 | IOC | Interrupt on Completion |
| 24 | BUF1V | Buffer 1 Valid |
| 14:0 | - | Not used for RX (buffer size is in DMACRCR register) |

### Descriptor Ring:

The descriptors form a circular ring. When DMA reaches the last descriptor, it wraps around to the first.

```
┌──────────────────────────────────────────────────────────────────┐
│                    Descriptor Ring (8 descriptors)               │
│                                                                  │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐       ┌──────────┐   │
│  │ Desc[0]  │──►│ Desc[1]  │──►│ Desc[2]  │──►....│ Desc[7]  │──┐│
│  │ OWN=1    │   │ OWN=1    │   │ OWN=1    │       │ OWN=1    │  ││
│  │ Buff=A   │   │ Buff=B   │   │ Buff=C   │       │ Buff=H   │  ││
│  └──────────┘   └──────────┘   └──────────┘       └──────────┘  ││
│       ▲                                                         ││
│       └─────────────────────────────────────────────────────────┘│
│                         (wraps around)                           │
└──────────────────────────────────────────────────────────────────┘
```

---

## 4. OWN Bit - The Key Concept

The **OWN bit** (bit 31 of DESC3) is the most important concept in DMA operation. It determines who "owns" the descriptor and can access it.

### OWN Bit State Machine:

```
┌─────────────────────────────────────────────────────────────────┐
│                    OWN Bit State Machine                        │
│                                                                 │
│  ┌─────────────────┐                    ┌─────────────────┐     │
│  │   CPU Owns      │                    │   DMA Owns      │     │
│  │   (OWN = 0)     │                    │   (OWN = 1)     │     │
│  │                 │                    │                 │     │
│  │ CPU can:        │                    │ DMA can:        │     │
│  │ - Read data     │                    │ - Write data    │     │
│  │ - Modify desc   │                    │ - Read buffer   │     │
│  │ - Set OWN=1     │                    │ - Clear OWN     │     │
│  └────────┬────────┘                    └────────┬────────┘     │
│           │                                      │              │
│           │  CPU sets OWN=1                      │              │
│           │  (gives to DMA)                      │              │
│           └──────────────────►───────────────────┘              │
│                                                                 │
│           ┌──────────────────◄───────────────────┐              │
│           │                                      │              │
│           │  DMA clears OWN=0                    │              │
│           │  (returns to CPU)                    │              │
│           ▼                                      │              │
│  ┌─────────────────┐                    ┌────────┴────────┐     │
│  │   CPU Owns      │                    │   DMA Owns      │     │
│  └─────────────────┘                    └─────────────────┘     │
└─────────────────────────────────────────────────────────────────┘
```

### RX Flow with OWN Bit:

1. **Initialization**: CPU sets OWN=1, gives descriptor to DMA
2. **Waiting**: DMA waits for packet from network
3. **Reception**: DMA writes packet to buffer, clears OWN=0
4. **Interrupt**: DMA triggers interrupt to notify CPU
5. **Processing**: CPU reads packet, processes it
6. **Recycling**: CPU sets OWN=1 again (recycles descriptor)

### Critical Rule:

> **Never access a descriptor while DMA owns it (OWN=1)!**
>
> This can cause data corruption or undefined behavior.

---

## 5. RBU (Receive Buffer Unavailable) Error

### What is RBU?

RBU occurs when the DMA needs to receive a packet but has no available descriptors. This happens when:

1. All descriptors have OWN=0 (CPU owns them all)
2. Descriptors have no valid buffer address
3. CPU is too slow processing packets

### RBU Error Scenario:

```
┌─────────────────────────────────────────────────────────────────┐
│                    RBU Error Scenario                           │
│                                                                 │
│  Packet arrives from network...                                 │
│                                                                 │
│  DMA checks descriptors:                                        │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │ Desc[0]  │   │ Desc[1]  │   │ Desc[2]  │   │ Desc[3]  │      │
│  │ OWN=0 ✗  │    OWN=0 ✗  │   │ OWN=0 ✗  │   │ OWN=0 ✗  │     │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘      │
│                                                                 │
│  All descriptors owned by CPU!                                  │
│  DMA cannot store packet → RBU ERROR!                           │
│  Packet is DROPPED!                                             │
└─────────────────────────────────────────────────────────────────┘
```

### Our Spurious RBU Problem:

In our case, all descriptors HAD OWN=1 (DMA owned them), but RBU still fired. This was a **spurious** RBU caused by a race condition:

1. DMA was enabled at line 782 of `HAL_ETH_Start_IT()`
2. DMA immediately started checking descriptors
3. Before DMA fully processed the descriptor ring, it reported RBU
4. RBUE interrupt was enabled at line 802
5. The pending RBU status immediately triggered an interrupt

---

## 6. Key DMA Registers

### Register Summary:

| Register | Address | Description |
|----------|---------|-------------|
| DMACRDLAR | 0x20050000 | Descriptor List Address Register |
| DMACRDRLR | 0x00000007 | Descriptor Ring Length Register |
| DMACRDTPR | 0x200500A8 | Descriptor Tail Pointer Register |
| DMACRCR | 0x00200BE9 | Receive Control Register |
| DMACSR | 0x00004080 | Status Register |

### Detailed Register Descriptions:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Key DMA Registers                             │
│                                                                  │
│  DMACRDLAR (0x20050000):                                         │
│  ├── Descriptor List Address Register                            │
│  └── Points to first descriptor in ring                          │
│                                                                  │
│  DMACRDRLR (0x00000007):                                         │
│  ├── Descriptor Ring Length Register                             │
│  └── Value = N-1 where N = number of descriptors (8-1=7)         │
│                                                                  │
│  DMACRDTPR (0x200500A8):                                         │
│  ├── Descriptor Tail Pointer Register                            │
│  └── Points to last valid descriptor (Desc[7])                   │
│  └── Writing to this "kicks" DMA to check for new descriptors    │
│                                                                  │
│  DMACRCR (0x00200BE9):                                           │
│  ├── Receive Control Register                                    │
│  ├── Bit 0 (SR): Start Receive - 1=DMA running                   │
│  └── Bits 14:1 (RBSZ): Receive Buffer Size = 1524 bytes          │
│                                                                  │
│  DMACSR (0x00004080):                                            │
│  ├── Status Register                                             │
│  ├── Bit 7 (RBU): Receive Buffer Unavailable                     │
│  └── Bit 14 (AIS): Abnormal Interrupt Summary                    │
└─────────────────────────────────────────────────────────────────┘
```

### Tail Pointer Calculation:

The tail pointer points to the last valid descriptor:
- Descriptor base: `0x20050000`
- Each descriptor: 24 bytes (0x18)
- Descriptor 7 address: `0x20050000 + 7 * 0x18 = 0x200500A8`

---

## 7. Memory Barriers (__DSB, __DMB, __ISB)

### Why Memory Barriers?

Modern CPUs (including Cortex-M33) can reorder memory operations for performance. This can cause problems with DMA:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Memory Barrier Problem                        │
│                                                                  │
│  Without barriers, CPU might execute:                            │
│                                                                  │
│  1. Write to DESC0 (buffer address)                              │
│  2. Write to DESC3 (set OWN=1)                                   │
│  3. Enable DMA                                                   │
│                                                                  │
│  But CPU might REORDER to:                                       │
│                                                                  │
│  1. Enable DMA          ← DMA starts before descriptors ready!   │
│  2. Write to DESC0                                               │
│  3. Write to DESC3                                               │
│                                                                  │
│  Result: DMA sees invalid descriptors → RBU error!               │
└─────────────────────────────────────────────────────────────────┘
```

### Types of Barriers:

| Barrier | Name | Purpose |
|---------|------|---------|
| `__DMB()` | Data Memory Barrier | Ensures memory operations complete **in order** |
| `__DSB()` | Data Synchronization Barrier | Ensures all memory writes are **complete** before proceeding |
| `__ISB()` | Instruction Synchronization Barrier | Flushes CPU pipeline, ensures subsequent instructions see memory changes |

### When to Use Each:

- **`__DMB()`**: Before writing to peripheral registers after memory writes
- **`__DSB()`**: Before enabling DMA, after configuring descriptors
- **`__ISB()`**: After modifying system control registers, after `__DSB()`

### Example Usage:

```c
/* Configure descriptors */
desc->DESC0 = buffer_address;
desc->DESC3 = OWN_BIT | IOC_BIT | BUF1V_BIT;

/* Ensure all writes are complete before enabling DMA */
__DSB();
__ISB();

/* Now safe to enable DMA */
SET_BIT(ETH->DMACRCR, ETH_DMACRCR_SR);
```

---

## 8. MPU (Memory Protection Unit) Configuration

### Why MPU Matters for DMA:

The MPU controls memory attributes like cacheability and shareability. For DMA to work correctly, the descriptor and buffer memory must be configured properly.

### MPU Configuration for DMA:

```
┌─────────────────────────────────────────────────────────────────┐
│                    MPU Configuration for DMA                     │
│                                                                  │
│  Region: 0x20050000 - 0x20057FFF (32KB in SRAM3)                 │
│                                                                  │
│  Attributes:                                                     │
│  ├── Non-cacheable: CPU writes go directly to memory             │
│  │   (DMA sees changes immediately, no cache flush needed)       │
│  │                                                               │
│  ├── Inner Shareable: For single-core with DMA                   │
│  │   (Ensures coherency between CPU and DMA)                     │
│  │                                                               │
│  └── No Execute: Data region only                                │
│      (Security - prevents code execution from this region)       │
└─────────────────────────────────────────────────────────────────┘
```

### Inner vs Outer Shareable:

| Attribute | Use Case |
|-----------|----------|
| **Outer Shareable** | Multi-processor systems (multiple CPUs) |
| **Inner Shareable** | Single-core with DMA (our case) |

### Why Non-Cacheable?

If the region is cacheable:
1. CPU writes to cache, not memory
2. DMA reads from memory, sees stale data
3. Data corruption!

With non-cacheable:
1. CPU writes directly to memory
2. DMA reads from memory, sees correct data
3. No cache maintenance needed

---

## 9. NVIC (Nested Vectored Interrupt Controller)

### What is NVIC?

NVIC manages all interrupts on Cortex-M processors. It:
- Prioritizes interrupts
- Enables/disables individual interrupts
- Routes interrupts to handlers

### NVIC Interrupt Flow:

```
┌─────────────────────────────────────────────────────────────────┐
│                    NVIC Interrupt Flow                           │
│                                                                  │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐       │
│  │ ETH DMA │───►│ DMACSR  │───►│  NVIC   │───►│   CPU   │       │
│  │ Event   │    │ Status  │    │         │    │ Handler │       │
│  └─────────┘    └─────────┘    └─────────┘    └─────────┘       │
│                                                                  │
│  1. DMA detects RBU condition                                    │
│  2. Sets RBU bit in DMACSR register                              │
│  3. If RBUE enabled in DMACIER, signals NVIC                     │
│  4. NVIC checks if ETH_IRQn is enabled                           │
│  5. If enabled, CPU jumps to ETH_IRQHandler                      │
│  6. Handler calls HAL_ETH_IRQHandler                             │
│  7. HAL calls HAL_ETH_ErrorCallback                              │
└─────────────────────────────────────────────────────────────────┘
```

### Our Fix Using NVIC:

```c
/* Disable NVIC - Step 4 blocked! */
HAL_NVIC_DisableIRQ(ETH_IRQn);

/* Start DMA (RBU status may be set) */
HAL_ETH_Start_IT(&heth);

/* Clear RBU status */
__HAL_ETH_DMA_CLEAR_IT(&heth, ETH_DMACSR_RBU | ETH_DMACSR_AIS);

/* Re-enable NVIC - Now safe */
HAL_NVIC_EnableIRQ(ETH_IRQn);
```

---

## 10. Complete RX Data Flow

### Initialization Phase:

```
┌─────────────────────────────────────────────────────────────────┐
│                    INITIALIZATION                                │
│                                                                  │
│  HAL_ETH_Start_IT():                                             │
│  ├── Set RxBuildDescCnt = 8                                      │
│  ├── Call ETH_UpdateDescriptor() for each descriptor             │
│  │   ├── Calls HAL_ETH_RxAllocateCallback() to get buffer        │
│  │   ├── Sets DESC0 = buffer address                             │
│  │   └── Sets DESC3 = OWN=1, IOC=1, BUF1V=1                      │
│  ├── Enable TX DMA (DMACTCR.ST = 1)                              │
│  ├── Enable RX DMA (DMACRCR.SR = 1)                              │
│  └── Enable interrupts (DMACIER)                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Packet Reception Phase:

```
┌─────────────────────────────────────────────────────────────────┐
│                    PACKET RECEPTION                              │
│                                                                  │
│  Network → PHY → MAC → DMA:                                      │
│  ├── DMA reads current descriptor                                │
│  ├── Checks OWN bit (must be 1)                                  │
│  ├── Writes packet to buffer address in DESC0                    │
│  ├── Updates DESC3 with packet length, status                    │
│  ├── Clears OWN bit (OWN = 0)                                    │
│  ├── Moves to next descriptor                                    │
│  └── Triggers RX Complete interrupt                              │
└─────────────────────────────────────────────────────────────────┘
```

### Interrupt Handling Phase:

```
┌─────────────────────────────────────────────────────────────────┐
│                    INTERRUPT HANDLING                            │
│                                                                  │
│  ETH_IRQHandler → HAL_ETH_IRQHandler:                            │
│  ├── Checks DMACSR for interrupt source                          │
│  ├── If RX complete: calls HAL_ETH_RxCpltCallback()              │
│  └── Callback signals RxPktSemaphore                             │
└─────────────────────────────────────────────────────────────────┘
```

### Packet Processing Phase:

```
┌─────────────────────────────────────────────────────────────────┐
│                    PACKET PROCESSING                             │
│                                                                  │
│  ethernetif_input_task (FreeRTOS task):                          │
│  ├── Waits on RxPktSemaphore                                     │
│  ├── Calls HAL_ETH_ReadData()                                    │
│  │   ├── Finds descriptors with OWN=0 (received data)            │
│  │   ├── Calls HAL_ETH_RxLinkCallback() to build pbuf            │
│  │   └── Calls ETH_UpdateDescriptor() to refill                  │
│  │       ├── Calls HAL_ETH_RxAllocateCallback() for new buffer   │
│  │       └── Sets OWN=1 (gives back to DMA)                      │
│  └── Passes pbuf to LwIP stack                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Code Locations: CPU Descriptor Operations

This section shows exactly where in the code the CPU reads descriptors and sets the OWN bit.

### 11.1 Where CPU Sets OWN Bit

The CPU sets the OWN bit in **`ETH_UpdateDescriptor()`** in the HAL driver:

**File:** `stm32h5xx_hal_eth.c` (lines 1164-1234)

```c
static void ETH_UpdateDescriptor(ETH_HandleTypeDef *heth)
{
    // ... buffer allocation code ...

    // Line 1206 - With interrupt on completion (IOC):
    WRITE_REG(dmarxdesc->DESC3, ETH_DMARXNDESCRF_OWN | ETH_DMARXNDESCRF_BUF1V | ETH_DMARXNDESCRF_IOC);

    // Line 1210 - Without interrupt:
    WRITE_REG(dmarxdesc->DESC3, ETH_DMARXNDESCRF_OWN | ETH_DMARXNDESCRF_BUF1V);
}
```

Where:
- `ETH_DMARXNDESCRF_OWN = 0x80000000U` (bit 31)
- `ETH_DMARXNDESCRF_BUF1V = 0x01000000U` (bit 24 - Buffer 1 Valid)
- `ETH_DMARXNDESCRF_IOC = 0x40000000U` (bit 30 - Interrupt on Completion)

### 11.2 Where CPU Reads/Checks OWN Bit

The CPU checks the OWN bit in **`HAL_ETH_ReadData()`**:

**File:** `stm32h5xx_hal_eth.c` (lines 1040-1154)

```c
HAL_StatusTypeDef HAL_ETH_ReadData(ETH_HandleTypeDef *heth, void **pAppBuff)
{
    // Line 1067 - Check if descriptor is NOT owned by DMA (OWN=0 means CPU owns it)
    while ((READ_BIT(dmarxdesc->DESC3, ETH_DMARXNDESCWBF_OWN) == (uint32_t)RESET) &&
           (descidx < (uint32_t)ETH_RX_DESC_CNT))
    {
        // Process received data...
    }
}
```

Where:
- `ETH_DMARXNDESCWBF_OWN = 0x80000000U` (bit 31 in writeback format)
- `RESET = 0` - So the condition checks if OWN bit is 0 (CPU owns)

### 11.3 Application Call Flow

Your application calls `HAL_ETH_ReadData()` through this chain:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Call Flow                         │
│                                                                  │
│  ETH_IRQHandler()                    ← Interrupt fires           │
│      └── HAL_ETH_IRQHandler()        ← HAL processes interrupt   │
│          └── HAL_ETH_RxCpltCallback() ← Callback in ethernetif.c │
│              └── osSemaphoreRelease(RxPktSemaphore)              │
│                                                                  │
│  ethernetif_input_task()             ← FreeRTOS task (line 445)  │
│      └── osSemaphoreAcquire()        ← Waits for semaphore       │
│      └── low_level_input()           ← Line 414                  │
│          └── HAL_ETH_ReadData()      ← Line 425 - YOUR CALL      │
│              ├── Checks OWN bit (line 1067 in HAL)               │
│              ├── HAL_ETH_RxLinkCallback()  ← Builds pbuf chain   │
│              └── ETH_UpdateDescriptor()    ← Sets OWN=1          │
│                  └── HAL_ETH_RxAllocateCallback() ← New buffer   │
└─────────────────────────────────────────────────────────────────┘
```

### 11.4 Key Code Locations in Your Application

| Function | File | Line | Purpose |
|----------|------|------|---------|
| `ethernetif_input_task()` | ethernetif.c | 445 | FreeRTOS task waiting for RX |
| `low_level_input()` | ethernetif.c | 414 | Wrapper that calls HAL |
| `HAL_ETH_ReadData()` call | ethernetif.c | 425 | Actual HAL call |
| `HAL_ETH_RxAllocateCallback()` | ethernetif.c | 126 | Called by `ETH_UpdateDescriptor()` |
| `HAL_ETH_RxLinkCallback()` | ethernetif.c | 172 | Called by `HAL_ETH_ReadData()` |
| Task creation | ethernetif.c | 285 | `xTaskCreateStatic(ethernetif_input_task, "EthIf", ...)` |

### 11.5 Key Code Locations in HAL Driver

| Function | File | Line | Purpose |
|----------|------|------|---------|
| `HAL_ETH_ReadData()` | stm32h5xx_hal_eth.c | 1040 | Reads received data |
| OWN bit check | stm32h5xx_hal_eth.c | 1067 | `READ_BIT(dmarxdesc->DESC3, ETH_DMARXNDESCWBF_OWN)` |
| `ETH_UpdateDescriptor()` | stm32h5xx_hal_eth.c | 1164 | Refills descriptors |
| OWN bit set (with IOC) | stm32h5xx_hal_eth.c | 1206 | `WRITE_REG(dmarxdesc->DESC3, OWN\|BUF1V\|IOC)` |
| OWN bit set (no IOC) | stm32h5xx_hal_eth.c | 1210 | `WRITE_REG(dmarxdesc->DESC3, OWN\|BUF1V)` |
| `HAL_ETH_Start_IT()` | stm32h5xx_hal_eth.c | 763 | Initializes and starts DMA |
| Initial descriptor setup | stm32h5xx_hal_eth.c | 776 | Calls `ETH_UpdateDescriptor()` |

### 11.6 Complete Cycle

```
┌─────────────────────────────────────────────────────────────────┐
│                    Complete OWN Bit Cycle                        │
│                                                                  │
│  1. INITIALIZATION (HAL_ETH_Start_IT):                           │
│     └── ETH_UpdateDescriptor() sets OWN=1 for all descriptors    │
│         (Gives all descriptors to DMA)                           │
│                                                                  │
│  2. PACKET RECEPTION (DMA hardware):                             │
│     └── DMA writes packet to buffer                              │
│     └── DMA clears OWN=0 (Returns descriptor to CPU)             │
│     └── DMA triggers RX complete interrupt                       │
│                                                                  │
│  3. PACKET PROCESSING (HAL_ETH_ReadData):                        │
│     └── CPU checks OWN=0 (Confirms CPU owns descriptor)          │
│     └── CPU reads packet data from buffer                        │
│     └── HAL_ETH_RxLinkCallback() builds pbuf                     │
│                                                                  │
│  4. DESCRIPTOR RECYCLING (ETH_UpdateDescriptor):                 │
│     └── HAL_ETH_RxAllocateCallback() provides new buffer         │
│     └── CPU sets OWN=1 (Gives descriptor back to DMA)            │
│     └── Cycle repeats from step 2                                │
└─────────────────────────────────────────────────────────────────┘
```

---

## 12. Memory Layout

### SRAM3 Memory Map:

```
┌─────────────────────────────────────────────────────────────────┐
│                    SRAM3 Memory Layout                           │
│                    (Non-cacheable region)                        │
│                                                                  │
│  0x20050000 ┌─────────────────────────────────────┐              │
│             │ RX Descriptors (8 × 24 = 192 bytes) │              │
│             │ DMARxDscrTab[0..7]                  │              │
│  0x200500C0 ├─────────────────────────────────────┤              │
│             │ TX Descriptors (8 × 24 = 192 bytes) │              │
│             │ DMATxDscrTab[0..7]                  │              │
│  0x20050180 ├─────────────────────────────────────┤              │
│             │ RX Buffer 0 (1536 bytes)            │              │
│  0x20050780 ├─────────────────────────────────────┤              │
│             │ RX Buffer 1 (1536 bytes)            │              │
│  0x20050D80 ├─────────────────────────────────────┤              │
│             │ RX Buffer 2 (1536 bytes)            │              │
│  0x20051380 ├─────────────────────────────────────┤              │
│             │ RX Buffer 3 (1536 bytes)            │              │
│  0x20051980 ├─────────────────────────────────────┤              │
│             │ RX Buffer 4 (1536 bytes)            │              │
│  0x20051F80 ├─────────────────────────────────────┤              │
│             │ RX Buffer 5 (1536 bytes)            │              │
│  0x20052580 ├─────────────────────────────────────┤              │
│             │ RX Buffer 6 (1536 bytes)            │              │
│  0x20052B80 ├─────────────────────────────────────┤              │
│             │ RX Buffer 7 (1536 bytes)            │              │
│  0x20053180 └─────────────────────────────────────┘              │
│                                                                  │
│  Total: 192 + 192 + (8 × 1536) = 12,672 bytes                    │
└─────────────────────────────────────────────────────────────────┘
```

### Why 1536 Bytes Per Buffer?

- Maximum Ethernet frame: 1518 bytes (1500 payload + 14 header + 4 CRC)
- Rounded up to 1536 for alignment (multiple of 32)
- Allows for VLAN tags (4 extra bytes)

---

## 13. Summary of Our Fix

### The Race Condition:

The spurious RBU error occurred because of a race condition in `HAL_ETH_Start_IT()`:

```
┌─────────────────────────────────────────────────────────────────┐
│                    The Race Condition                            │
│                                                                  │
│  HAL_ETH_Start_IT() sequence:                                    │
│                                                                  │
│  Line 776: ETH_UpdateDescriptor()  ← Descriptors ready           │
│  Line 779: Enable TX DMA                                         │
│  Line 782: Enable RX DMA           ← DMA starts running!         │
│            │                                                     │
│            │  ┌─────────────────────────────────────────────┐    │
│            │  │ RACE WINDOW: DMA running, RBUE not enabled  │    │
│            │  │ DMA may set RBU status during this window   │    │
│            │  └─────────────────────────────────────────────┘    │
│            │                                                     │
│  Line 802: Enable RBUE interrupt   ← Pending RBU triggers IRQ!   │
└─────────────────────────────────────────────────────────────────┘
```

### Our Fix:

```c
/* 1. Disable NVIC - Block IRQ delivery */
HAL_NVIC_DisableIRQ(ETH_IRQn);

/* 2. Memory barriers */
__DSB();
__ISB();

/* 3. Start DMA - RBU may be set during this call */
HAL_ETH_Start_IT(&heth);

/* 4. Clear spurious RBU status */
__DSB();
__HAL_ETH_DMA_CLEAR_IT(&heth, ETH_DMACSR_RBU | ETH_DMACSR_AIS);

/* 5. Re-enable NVIC - Safe to enable now */
HAL_NVIC_EnableIRQ(ETH_IRQn);
```

### Why This Works:

1. `HAL_NVIC_DisableIRQ(ETH_IRQn)` prevents the NVIC from servicing ETH interrupts
2. `HAL_ETH_Start_IT()` enables DMA and then enables RBUE interrupt
3. Even if RBU status is set, the interrupt is NOT serviced (NVIC disabled)
4. `__HAL_ETH_DMA_CLEAR_IT()` clears the spurious RBU status
5. `HAL_NVIC_EnableIRQ(ETH_IRQn)` re-enables interrupts
6. Since status is cleared, no spurious interrupt is triggered
7. Real RBU errors (if any) will still be detected because they'll set the status bit again

---

## References

- STM32H5 Reference Manual (RM0481)
- STM32H5 HAL Driver Source Code
- ARM Cortex-M33 Technical Reference Manual
- LwIP Documentation

---

*Document created: 2026-01-26*
*Last updated: 2026-01-26*
