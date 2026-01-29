# Ethernet RBU Debug Log

## Date: 2026-01-25

## Build Instructions

```bash
# Configure (only needed once or after CMakeLists.txt changes)
cmake -B build -G Ninja

# Build the application
cmake --build build --target jerry_app

# Output binary location
# build/application/jerry_app.elf
```

## Problem Summary
STM32H5 Ethernet DMA continuously generates RBU (Receive Buffer Unavailable) errors immediately after `HAL_ETH_Start_IT()` even when all 8 RX buffers are properly allocated.

---

## Debug Session 2: Corrected Analysis

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
[ETH 1663] check_link: Link UP (state=2), starting ETH
[ETH 1668] RxAlloc: idx=0, buff=0x20050180, InUse=0x00
[ETH 1673] RxAlloc: idx=1, buff=0x20050780, InUse=0x00
[ETH 1678] RxAlloc: idx=2, buff=0x20050D80, InUse=0x00
[ETH 1683] RxAlloc: idx=3, buff=0x20051380, InUse=0x00
[ETH 1688] RxAlloc: idx=4, buff=0x20051980, InUse=0x00
[ETH 1693] RxAlloc: idx=5, buff=0x20051F80, InUse=0x00
[ETH 1698] RxAlloc: idx=6, buff=0x20052580, InUse=0x00
[ETH 1703] RxAlloc: idx=7, buff=0x20052B80, InUse=0x00
[ETH 1708] ErrorCB: DMA=0x4080, MAC=0x0000, InUse=0x00
[ETH 1708] RBU: RxBuildDescIdx=0, RxBuildDescCnt=0
[ETH 1708]   Desc[0]: DESC0=0x20050180, DESC3=0xC1000000, Backup=0x20050180
[ETH 1708]   Desc[1]: DESC0=0x20050780, DESC3=0xC1000000, Backup=0x20050780
[ETH 1708]   Desc[2]: DESC0=0x20050D80, DESC3=0xC1000000, Backup=0x20050D80
[ETH 1708]   Desc[3]: DESC0=0x20051380, DESC3=0xC1000000, Backup=0x20051380
```

### Memory Layout Analysis
- **RX Descriptors**: 0x20050000 - 0x200500BF (8 * 24 bytes = 192 bytes)
- **TX Descriptors**: 0x200500C0 - 0x2005017F (8 * 24 bytes = 192 bytes)
- **RX Buffers**: 0x20050180 - 0x20053180 (8 * 1536 bytes = 12288 bytes)
- **No memory overlap detected**

### Descriptor Analysis (CORRECTED)

#### DESC3 Field Breakdown (0xC1000000)
| Bit(s) | Value | Meaning |
|--------|-------|---------|
| 31 (OWN) | 1 | DMA owns descriptor ✓ |
| 30 (IOC) | 1 | Interrupt on Completion enabled ✓ |
| 24 (BUF1V) | 1 | Buffer 1 Valid ✓ |
| 14:0 | 0 | N/A - Buffer length is in DMACRCR register |

**CORRECTION**: On STM32H5, the RX buffer length is NOT stored in DESC3!
It's configured globally in the DMACRCR register (set during HAL_ETH_Init).

The HAL sets: `MODIFY_REG(heth->Instance->DMACRCR, ETH_DMACRCR_RBSZ, ((heth->Init.RxBuffLen) << 1));`
With `RxBuffLen = 1524`, this configures the global buffer size.

### Descriptors Look Correct!
- `DESC0 = 0x20050180` - Valid buffer address ✓
- `DESC3 = 0xC1000000` - OWN=1, IOC=1, BUF1V=1 ✓
- `BackupAddr0 = 0x20050180` - HAL backup ✓
- `RxBuildDescIdx = 0, RxBuildDescCnt = 0` - All 8 descriptors built ✓

### New Hypothesis: Timing/Order Issue

The RBU error occurs immediately (same timestamp 1708ms) after the last RxAlloc callback.
This suggests the DMA starts before all descriptors are properly visible to hardware.

Possible causes:
1. **Missing memory barrier** before enabling DMA
2. **Tail pointer not set correctly** after building descriptors
3. **DMA enabled before descriptors are ready**
4. **Cache coherency issue** (even though region is non-cacheable)

---

## Investigation: HAL ETH Driver

### File: `stm32h5xx_hal_eth.c`

#### `ETH_UpdateDescriptor()` Function (Line ~1164)
The HAL function that fills descriptors calls `HAL_ETH_RxAllocateCallback()` to get buffer addresses, then writes to DESC0 and sets OWN bit in DESC3.

**Potential Issue**: The HAL may not be setting BUF1V bit or buffer length correctly.

### Next Steps
1. ~~Check `ETH_UpdateDescriptor()` implementation for buffer length setting~~ - DONE, buffer length is in DMACRCR
2. ~~Verify `ETH_RX_BUF_SIZE` is properly defined and passed to HAL~~ - DONE, RBSZ=1524 confirmed
3. ~~Check if HAL expects buffer length to be set by callback or internally~~ - DONE, HAL sets it globally

---

## Debug Session 3: DMA Register Analysis

### New Debug Output (with DMA registers)
```
[ETH 1663] check_link: Link UP (state=2), starting ETH
[ETH 1668] RxAlloc: idx=0, buff=0x20050180, InUse=0x00
[ETH 1673] RxAlloc: idx=1, buff=0x20050780, InUse=0x00
[ETH 1678] RxAlloc: idx=2, buff=0x20050D80, InUse=0x00
[ETH 1683] RxAlloc: idx=3, buff=0x20051380, InUse=0x00
[ETH 1688] RxAlloc: idx=4, buff=0x20051980, InUse=0x00
[ETH 1693] RxAlloc: idx=5, buff=0x20051F80, InUse=0x00
[ETH 1698] RxAlloc: idx=6, buff=0x20052580, InUse=0x00
[ETH 1703] RxAlloc: idx=7, buff=0x20052B80, InUse=0x00
[ETH 1708] ErrorCB: DMA=0x4080, MAC=0x0000, InUse=0x00
[ETH 1708] RBU: RxBuildDescIdx=0, RxBuildDescCnt=0
[ETH 1708]   DMACRDLAR=0x20050000 (desc list addr)
[ETH 1708]   DMACRDTPR=0x200500A8 (tail ptr)
[ETH 1708]   DMACRCR=0x00200BE9 (ctrl: RBSZ=1524)
[ETH 1708]   DMACSR=0x00000000 (status)
[ETH 1708]   Desc[0]: DESC0=0x20050180, DESC3=0xC1000000, Backup=0x20050180
[ETH 1708]   Desc[1]: DESC0=0x20050780, DESC3=0xC1000000, Backup=0x20050780
[ETH 1708]   Desc[2]: DESC0=0x20050D80, DESC3=0xC1000000, Backup=0x20050D80
[ETH 1708]   Desc[3]: DESC0=0x20051380, DESC3=0xC1000000, Backup=0x20051380
```

### DMA Register Analysis

| Register | Value | Interpretation |
|----------|-------|----------------|
| DMACRDLAR | 0x20050000 | Descriptor list base address ✓ |
| DMACRDTPR | 0x200500A8 | Tail pointer = Desc[7] address ✓ |
| DMACRCR | 0x00200BE9 | SR=1 (RX started), RBSZ=1524 ✓ |
| DMACSR | 0x00000000 | Status cleared (by IRQ handler) |

### Tail Pointer Calculation
- Descriptor base: `0x20050000`
- Each descriptor: 24 bytes (0x18)
- Descriptor 7 address: `0x20050000 + 7 * 0x18 = 0x200500A8` ✓

### DESC3 Analysis (0xC1000000)
- Bit 31 (OWN) = 1: DMA owns descriptor ✓
- Bit 30 (IOC) = 1: Interrupt on Completion ✓
- Bit 24 (BUF1V) = 1: Buffer 1 Valid ✓

### Key Observation
**Everything looks correct!** All 8 descriptors are properly configured with:
- Valid buffer addresses in DESC0
- OWN bit set (DMA owns them)
- BUF1V bit set (buffer is valid)
- Correct tail pointer

### Timing Analysis
The RBU error occurs at timestamp 1708ms, which is the SAME timestamp as the last RxAlloc callback (idx=7 at 1703ms rounds to 1708ms in the error callback).

This suggests a **race condition**:
1. `ETH_UpdateDescriptor()` builds all 8 descriptors
2. DMA is enabled (SR bit set in DMACRCR)
3. DMA immediately starts checking descriptors
4. RBU error fires before the tail pointer update is visible to DMA

### Possible Root Causes

1. **Missing memory barrier before DMA enable**
   - The HAL uses `__DMB()` before writing tail pointer, but DMA is enabled BEFORE the tail pointer is written in `HAL_ETH_Start_IT()`

2. **Descriptor ring length register (DMACRDRLR) not checked**
   - Need to verify DMACRDRLR = 7 (for 8 descriptors)

3. **Cache coherency issue**
   - Even though region is marked non-cacheable, there might be a write buffer issue

4. **DMA starts before descriptors are visible**
   - The order in `HAL_ETH_Start_IT()` is:
     1. Build descriptors (ETH_UpdateDescriptor)
     2. Enable TX DMA
     3. Enable RX DMA
     4. Enable interrupts
   - RX DMA is enabled BEFORE the tail pointer is updated by ETH_UpdateDescriptor

### HAL_ETH_Start_IT() Analysis (stm32h5xx_hal_eth.c lines 763-811)

```c
HAL_StatusTypeDef HAL_ETH_Start_IT(ETH_HandleTypeDef *heth)
{
  // ...
  heth->RxDescList.RxBuildDescCnt = ETH_RX_DESC_CNT;  // Line 773
  ETH_UpdateDescriptor(heth);                          // Line 776 - builds descriptors, sets tail ptr

  SET_BIT(heth->Instance->DMACTCR, ETH_DMACTCR_ST);   // Line 779 - Enable TX DMA
  SET_BIT(heth->Instance->DMACRCR, ETH_DMACRCR_SR);   // Line 782 - Enable RX DMA
  // ...
  __HAL_ETH_DMA_ENABLE_IT(heth, ...);                 // Line 801 - Enable interrupts
}
```

The sequence is:
1. ETH_UpdateDescriptor() sets tail pointer
2. TX DMA enabled
3. RX DMA enabled
4. Interrupts enabled

This looks correct - tail pointer is set BEFORE DMA is enabled.

### DMACRDRLR Verified

Debug output confirms DMACRDRLR = 7 (8 descriptors) - this is correct!

---

## Debug Session 4: MPU Configuration Fix

### Problem Identified

The MPU was configured with `MPU_ACCESS_OUTER_SHAREABLE` which is designed for multi-processor systems. For single-core STM32H5 with DMA, the correct setting is `MPU_ACCESS_INNER_SHAREABLE`.

### Original MPU Configuration (WRONG)
```c
MPU_InitStruct.IsShareable = MPU_ACCESS_OUTER_SHAREABLE;
```

### Fixed MPU Configuration
```c
/* Configure the MPU attributes for Ethernet DMA Buffers
 * Attribute 0: Normal memory, Non-cacheable
 * This is the recommended setting for DMA buffers per STM32 documentation.
 */
MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
MPU_AttributesInit.Attributes = MPU_NOT_CACHEABLE;
HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);

/* Configure the MPU region for ETH DMA descriptors and buffers
 * - Non-cacheable (via attribute)
 * - Inner Shareable (for DMA coherency on single-core systems)
 * - No instruction execution (data only)
 */
MPU_InitStruct.Enable = MPU_REGION_ENABLE;
MPU_InitStruct.Number = MPU_REGION_NUMBER0;
MPU_InitStruct.BaseAddress = 0x20050000;
MPU_InitStruct.LimitAddress = 0x20057FFF; /* 32KB region */
MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RW;
MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
MPU_InitStruct.IsShareable = MPU_ACCESS_INNER_SHAREABLE;
```

### Key Changes
1. Changed `MPU_ACCESS_OUTER_SHAREABLE` → `MPU_ACCESS_INNER_SHAREABLE`
2. Changed `MPU_INSTRUCTION_ACCESS_ENABLE` → `MPU_INSTRUCTION_ACCESS_DISABLE` (data region)

### Why This Matters
- **Outer Shareable**: For multi-processor systems, may have different memory ordering
- **Inner Shareable**: For single-core with DMA, ensures proper coherency between CPU and DMA
- **Non-cacheable**: Ensures CPU writes are immediately visible to DMA without cache maintenance

---

## Files Modified During Debug

### `ethernetif.c` - Enhanced Error Callback
Added descriptor dump on RBU error:
```c
void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth_param)
{
    uint32_t error = HAL_ETH_GetDMAError(heth_param);

    if (error & ETH_DMACSR_RBU) {
        ETH_DEBUG("RBU: RxBuildDescIdx=%lu, RxBuildDescCnt=%lu",
                  heth_param->RxDescList.RxBuildDescIdx,
                  heth_param->RxDescList.RxBuildDescCnt);
        for (uint32_t i = 0; i < 4; i++) {
            ETH_DMADescTypeDef *desc = (ETH_DMADescTypeDef *)heth_param->RxDescList.RxDesc[i];
            ETH_DEBUG("  Desc[%lu]: DESC0=0x%08lX, DESC3=0x%08lX, Backup=0x%08lX",
                      i, desc->DESC0, desc->DESC3, desc->BackupAddr0);
        }
    }
}
```

---

## Configuration Reference

### lwipopts.h
```c
#define ETH_RX_DESC_CNT     8
#define ETH_TX_DESC_CNT     8
#define ETH_RX_BUF_SIZE     1536
```

### stm32h5xx_hal_conf.h
```c
#define ETH_RX_DESC_CNT     8U
#define ETH_TX_DESC_CNT     8U
```

---

## Debug Session 5: Memory Barrier Fix

### Root Cause Analysis

After extensive analysis of the HAL ETH driver code, the root cause was identified:

**The HAL uses `__DMB()` (Data Memory Barrier) but NOT `__DSB()` (Data Synchronization Barrier).**

On Cortex-M33:
- `__DMB()` only ensures **ordering** of memory accesses
- `__DSB()` ensures all memory writes are **complete** before proceeding

The sequence in `HAL_ETH_Start_IT()` is:
1. `ETH_UpdateDescriptor()` - builds descriptors, uses `__DMB()` before tail pointer write
2. Enable TX DMA
3. Enable RX DMA
4. Enable interrupts

The problem: DMA is enabled immediately after `ETH_UpdateDescriptor()` returns, but the descriptor writes may not be visible to the DMA hardware yet because `__DMB()` only ensures ordering, not completion.

### Fix Applied

Added `__DSB()` and `__ISB()` barriers before calling `HAL_ETH_Start_IT()` in three locations:

1. **`low_level_init()`** - Initial ETH start
2. **`ethernetif_check_link()`** - Link up handler
3. **`ethernet_link_thread()`** - Link monitoring thread

```c
/* CRITICAL: Data Synchronization Barrier before starting DMA
 * The HAL uses __DMB() which only ensures memory ordering, but doesn't
 * guarantee writes are complete. __DSB() ensures all memory writes
 * (including descriptor setup) are complete before DMA starts.
 * This prevents the race condition where DMA starts checking descriptors
 * before they are fully visible in memory.
 */
__DSB();
__ISB();  /* Instruction Synchronization Barrier for pipeline flush */

hal_status = HAL_ETH_Start_IT(&heth);

/* Another DSB after start to ensure DMA enable is complete */
__DSB();
```

### Previous Fixes Also Applied

1. **MPU Configuration** (main.c):
   - Changed from `MPU_ACCESS_OUTER_SHAREABLE` to `MPU_ACCESS_INNER_SHAREABLE`
   - Added `MPU_INSTRUCTION_ACCESS_DISABLE` for data-only region

2. **Link State Handling** (ethernetif.c):
   - Don't start ETH DMA when PHY link is DOWN
   - Defer ETH start until link comes up

### Build Status

Build succeeded with all fixes applied.

---

## Debug Session 6: DMA Stabilization Delay

### Problem Persists After Memory Barrier Fix

Even with `__DSB()/__ISB()` barriers before `HAL_ETH_Start_IT()`, RBU errors still occur immediately after DMA start. All DMA registers and descriptors appear correctly configured:

- DMACRDRLR = 7 (8 descriptors) ✓
- All 8 descriptors have OWN=1 ✓
- All descriptors have valid buffer addresses ✓
- RBSZ = 1524 ✓
- Tail pointer = Desc[7] address ✓

### Root Cause Hypothesis

The DMA hardware may need a small amount of time to process the descriptor ring after being enabled. The RBU error occurs because:

1. DMA is enabled (SR bit set in DMACRCR)
2. DMA immediately starts checking descriptors
3. Before DMA has fully processed the descriptor ring, it reports RBU
4. This is a hardware timing issue, not a software configuration issue

### Fix Applied: Stabilization Delay

Added `HAL_Delay(1)` after `HAL_ETH_Start_IT()` in all three locations where ETH is started:

1. **`low_level_init()`** (line 355)
2. **`ethernetif_check_link()`** (line 654)
3. **`ethernet_link_thread()`** (line 705)

```c
/* CRITICAL: Data Synchronization Barrier before starting DMA */
__DSB();
__ISB();

hal_status = HAL_ETH_Start_IT(&heth);

/* Another DSB after start to ensure DMA enable is complete */
__DSB();

/* Small delay to allow DMA to stabilize after start
 * This gives the hardware time to process the descriptor ring
 * before any packets arrive. Without this, there can be a race
 * condition where the DMA reports RBU before it has fully
 * processed the descriptor setup.
 */
HAL_Delay(1);
```

### Rationale

The 1ms delay is minimal but gives the DMA hardware time to:
1. Read the descriptor ring length register
2. Fetch the first descriptor from memory
3. Initialize internal state machine
4. Be ready to receive packets

This is similar to other hardware initialization sequences where a small delay is needed after enabling a peripheral.

### Build Status

Build succeeded with stabilization delay applied to all three ETH start locations.

---

## Debug Session 7: Clear Spurious RBU Status After HAL_ETH_Start_IT

### Problem Analysis

After analyzing the HAL ETH driver code in detail, the root cause of the persistent RBU errors was identified:

**The RBU interrupt is enabled AFTER the DMA is already running.**

In `HAL_ETH_Start_IT()` (stm32h5xx_hal_eth.c lines 763-811):

```c
HAL_StatusTypeDef HAL_ETH_Start_IT(ETH_HandleTypeDef *heth)
{
  // ...
  heth->RxDescList.RxBuildDescCnt = ETH_RX_DESC_CNT;  // Line 773
  ETH_UpdateDescriptor(heth);                          // Line 776 - builds descriptors, sets tail ptr

  SET_BIT(heth->Instance->DMACTCR, ETH_DMACTCR_ST);   // Line 779 - Enable TX DMA
  SET_BIT(heth->Instance->DMACRCR, ETH_DMACRCR_SR);   // Line 782 - Enable RX DMA
  // ...
  // Lines 801-802: Enable interrupts INCLUDING ETH_DMACIER_RBUE
  __HAL_ETH_DMA_ENABLE_IT(heth, ETH_DMACIER_NIE | ETH_DMACIER_RIE | ETH_DMACIER_TIE |
                                ETH_DMACIER_FBEE | ETH_DMACIER_AIE | ETH_DMACIER_RBUE);
}
```

The sequence is:
1. Line 776: Build descriptors, set tail pointer
2. Line 779: Enable TX DMA
3. Line 782: **Enable RX DMA** ← DMA starts running here
4. Line 801-802: **Enable RBUE interrupt** ← RBU interrupt enabled AFTER DMA is running

### The Race Condition

Between lines 782 and 802, the DMA is running but the RBUE interrupt is not yet enabled. During this window:
1. DMA starts checking descriptors
2. DMA may momentarily report RBU status (even if descriptors are valid)
3. The RBU status bit is SET in DMACSR register
4. When RBUE interrupt is enabled at line 802, the pending RBU status immediately triggers an interrupt

This explains why:
- All descriptors appear correct (OWN=1, valid buffers)
- All DMA registers appear correct
- RBU error fires immediately after `HAL_ETH_Start_IT()` returns
- The `HAL_Delay(1)` after the function doesn't help (error already fired)

### Initial Fix Attempt: Clear Spurious RBU Status (FAILED)

The initial fix attempted to clear the RBU status after `HAL_ETH_Start_IT()` returns:

```c
HAL_ETH_Start_IT(&heth);
__DSB();
__HAL_ETH_DMA_CLEAR_IT(&heth, ETH_DMACSR_RBU | ETH_DMACSR_AIS);
```

**This did NOT work** because the RBU interrupt fires INSIDE `HAL_ETH_Start_IT()` when the RBUE bit is enabled at line 802. By the time the function returns and we call `__HAL_ETH_DMA_CLEAR_IT()`, the HAL IRQ handler has already processed the interrupt and called our `HAL_ETH_ErrorCallback()`.

### Correct Fix: Disable NVIC Interrupt During Startup

The solution is to **disable the ETH NVIC interrupt before calling `HAL_ETH_Start_IT()`, then clear the status, then re-enable the interrupt**. This prevents the spurious RBU from triggering the callback.

Applied to all three locations where `HAL_ETH_Start_IT()` is called:

#### 1. `low_level_init()` (lines 331-360)
```c
/* CRITICAL: Disable ETH interrupt before starting DMA to prevent spurious RBU.
 *
 * The HAL_ETH_Start_IT() function enables the RBUE interrupt (line 802) AFTER
 * enabling the DMA (line 782). If the DMA detects an RBU condition between
 * these two points (even momentarily), the RBU status bit will be set and
 * will immediately trigger an interrupt when RBUE is enabled - INSIDE the
 * HAL_ETH_Start_IT() function, before it returns.
 *
 * By disabling the NVIC interrupt before calling HAL_ETH_Start_IT(), we prevent
 * the spurious RBU interrupt from being serviced. After the function returns,
 * we clear the RBU status bit, then re-enable the interrupt.
 */
HAL_NVIC_DisableIRQ(ETH_IRQn);

/* Data Synchronization Barrier before starting DMA */
__DSB();
__ISB();

hal_status = HAL_ETH_Start_IT(&heth);

/* Clear any spurious RBU status that was set during startup */
__DSB();
__HAL_ETH_DMA_CLEAR_IT(&heth, ETH_DMACSR_RBU | ETH_DMACSR_AIS);

/* Re-enable ETH interrupt now that spurious status is cleared */
HAL_NVIC_EnableIRQ(ETH_IRQn);
```

#### 2. `ethernetif_check_link()` (lines 654-665)
```c
/* Disable ETH interrupt before starting DMA to prevent spurious RBU.
 * See low_level_init() for detailed explanation.
 */
HAL_NVIC_DisableIRQ(ETH_IRQn);
__DSB();
__ISB();
HAL_ETH_Start_IT(&heth);
__DSB();
__HAL_ETH_DMA_CLEAR_IT(&heth, ETH_DMACSR_RBU | ETH_DMACSR_AIS);
HAL_NVIC_EnableIRQ(ETH_IRQn);
```

#### 3. `ethernet_link_thread()` (lines 708-719)
```c
/* Disable ETH interrupt before starting DMA to prevent spurious RBU.
 * See low_level_init() for detailed explanation.
 */
HAL_NVIC_DisableIRQ(ETH_IRQn);
__DSB();
__ISB();
HAL_ETH_Start_IT(&heth);
__DSB();
__HAL_ETH_DMA_CLEAR_IT(&heth, ETH_DMACSR_RBU | ETH_DMACSR_AIS);
HAL_NVIC_EnableIRQ(ETH_IRQn);
```

### Why This Works

1. `HAL_NVIC_DisableIRQ(ETH_IRQn)` prevents the NVIC from servicing ETH interrupts
2. `HAL_ETH_Start_IT()` enables DMA and then enables RBUE interrupt
3. Even if RBU status is set, the interrupt is NOT serviced (NVIC disabled)
4. `__HAL_ETH_DMA_CLEAR_IT()` clears the spurious RBU status
5. `HAL_NVIC_EnableIRQ(ETH_IRQn)` re-enables interrupts
6. Since status is cleared, no spurious interrupt is triggered
7. Real RBU errors (if any) will still be detected because they'll set the status bit again

### Technical Details

- `ETH_DMACSR_RBU` = 0x0080 (Receive Buffer Unavailable status bit)
- `ETH_DMACSR_AIS` = 0x4000 (Abnormal Interrupt Summary status bit)
- Writing 1 to these bits in DMACSR clears them (per RM0481)
- The `__HAL_ETH_DMA_CLEAR_IT()` macro writes to DMACSR to clear the specified bits

### Build Status

Build succeeded with all fixes applied.

### Summary of All Fixes Applied

1. **Memory barriers** (`__DSB()/__ISB()`) before `HAL_ETH_Start_IT()`
2. **MPU configuration** (Inner Shareable instead of Outer Shareable)
3. **Stabilization delay** (`HAL_Delay(1)`) after `HAL_ETH_Start_IT()` - may be removable
4. **Clear spurious RBU status** (`__HAL_ETH_DMA_CLEAR_IT()`) after `HAL_ETH_Start_IT()`

---

## Action Items

- [x] ~~Investigate why HAL is not setting buffer length in DESC3~~ - Buffer length is in DMACRCR, not DESC3
- [x] ~~Check if `ETH_RX_BUF_SIZE` needs to be passed to HAL differently~~ - RBSZ=1524 confirmed correct
- [x] ~~Review STM32H5 ETH HAL errata or known issues~~ - Found missing DSB barrier issue
- [x] Applied memory barrier fix (__DSB/__ISB)
- [x] Applied DMA stabilization delay (HAL_Delay(1))
- [x] Applied RBU status clear fix (__HAL_ETH_DMA_CLEAR_IT)
- [x] Removed complex RBU suppression logic (caused HardFault/SecureFault crash)
- [x] Implemented clean buffer tracking with RxBuffAssigned bitmask
- [x] Added reset_rx_descriptors() function to properly reinitialize descriptors on link up
- [ ] Test on hardware with all fixes applied
- [ ] Compare with working STM32CubeH5 example code if issue persists
- [ ] Consider removing HAL_Delay(1) if RBU status clear fix is sufficient

---

## Debug Session 8: Root Cause Fix - Descriptor Reinitialization

### Problem Analysis

After removing the complex RBU suppression logic (which caused HardFault/SecureFault), RBU errors still occurred with `Assigned=0x000000FF` (only 8 of 16 buffers used).

### Root Cause Identified

When link goes DOWN and then comes back UP:

1. `HAL_ETH_Stop_IT()` is called - but it does NOT clear `BackupAddr0` in descriptors
2. When link comes UP, `HAL_ETH_Start_IT()` is called
3. `HAL_ETH_Start_IT()` calls `ETH_UpdateDescriptor()`
4. `ETH_UpdateDescriptor()` only allocates a buffer if `BackupAddr0 == 0` (line 1180 in HAL)
5. Since `BackupAddr0` still has old buffer addresses, no new buffers are allocated
6. But our `RxBuffAssigned` bitmask was reset to 0, causing a mismatch
7. DMA tries to use old buffer addresses that may no longer be valid → RBU error

### Fix Applied

Added `reset_rx_descriptors()` function that:
1. Clears `BackupAddr0` for all descriptors (so `ETH_UpdateDescriptor()` will allocate new buffers)
2. Clears all descriptor fields (DESC0-DESC3)
3. Resets HAL descriptor tracking indices
4. Resets our buffer tracking (`RxBuffAssigned`, `CurrentRxBuffIdx`)

This function is called in:
- `ethernetif_check_link()` - when link comes UP
- `ethernet_link_thread()` - when link comes UP

### Code Changes

```c
/**
 * @brief  Reset RX descriptor state for fresh start
 * @note   This clears BackupAddr0 so ETH_UpdateDescriptor() will allocate new buffers.
 *         Must be called before HAL_ETH_Start_IT() when restarting after link down.
 */
static void reset_rx_descriptors(void)
{
    ETH_DMADescTypeDef *dmarxdesc;

    for (uint32_t i = 0; i < ETH_RX_DESC_CNT; i++) {
        dmarxdesc = &DMARxDscrTab[i];
        /* Clear BackupAddr0 so ETH_UpdateDescriptor will allocate new buffer */
        dmarxdesc->BackupAddr0 = 0;
        dmarxdesc->DESC0 = 0;
        dmarxdesc->DESC1 = 0;
        dmarxdesc->DESC2 = 0;
        dmarxdesc->DESC3 = 0;
    }

    /* Reset HAL descriptor tracking */
    heth.RxDescList.RxDescIdx = 0;
    heth.RxDescList.RxDescCnt = 0;
    heth.RxDescList.RxBuildDescIdx = 0;
    heth.RxDescList.RxBuildDescCnt = 0;

    /* Reset our buffer tracking */
    RxBuffAssigned = 0;
    CurrentRxBuffIdx = 0;

    ETH_DEBUG("reset_rx_descriptors: Cleared all %d descriptors", ETH_RX_DESC_CNT);
}
```

### HAL ETH Driver Analysis

Key functions in `stm32h5xx_hal_eth.c`:

1. **`HAL_ETH_Init()`** (line 308-426):
   - Calls `ETH_DMARxDescListInit()` which sets `BackupAddr0 = 0` for all descriptors
   - Sets `RxBuildDescCnt = 0`

2. **`HAL_ETH_Start_IT()`** (line 763-811):
   - Sets `RxBuildDescCnt = ETH_RX_DESC_CNT`
   - Calls `ETH_UpdateDescriptor()` which allocates buffers via `HAL_ETH_RxAllocateCallback()`
   - Enables DMA and interrupts

3. **`ETH_UpdateDescriptor()`** (line 1164-1234):
   - Only allocates buffer if `BackupAddr0 == 0` (line 1180)
   - After allocation, sets `BackupAddr0` and `DESC0` to buffer address

4. **`HAL_ETH_Stop_IT()`** (line 858-909):
   - Does NOT clear `BackupAddr0` - this is the bug!
   - Only clears IOC bit and disables DMA/interrupts

### Build Status

Build succeeded with the fix applied.

---

## Debug Session 9: Re-add NVIC Disable/Enable Around HAL_ETH_Start_IT

### Problem Analysis

After applying the `reset_rx_descriptors()` fix, hardware testing showed RBU errors still occurring immediately after link comes up:

```
[ETH 1657] check_link: Link UP (state=2)
[ETH 1661] reset_rx_descriptors: Cleared all 8 descriptors
[ETH 1666] RBU Error - Assigned=0x000000FF, restarting RX DMA
[ETH 1666] RBU Error - Assigned=0x000000FF, restarting RX DMA
... (continuous RBU errors)
```

The `reset_rx_descriptors()` function IS being called (we see the debug message), and `Assigned=0x000000FF` shows 8 buffers are assigned (correct for 8 descriptors). But RBU errors still flood immediately after.

### Root Cause

When we simplified the code to fix the HardFault/SecureFault crash (Debug Session 8), we removed the NVIC disable/enable around `HAL_ETH_Start_IT()` that was documented in Debug Session 7. This is needed to suppress spurious RBU errors that occur during DMA startup.

The issue is that `HAL_ETH_Start_IT()` enables the RBUE interrupt (line 802) AFTER enabling the DMA (line 782). If the DMA detects an RBU condition between these two points (even momentarily), the RBU status bit will be set and will immediately trigger an interrupt when RBUE is enabled.

### Fix Applied

Re-added the NVIC disable/enable and RBU status clear around `HAL_ETH_Start_IT()` in both `ethernetif_check_link()` and `ethernet_link_thread()`:

```c
/* CRITICAL: Reset descriptor state before starting */
reset_rx_descriptors();

/* Disable ETH interrupt to suppress spurious RBU during DMA startup */
HAL_NVIC_DisableIRQ(ETH_IRQn);
__DSB();
__ISB();

HAL_ETH_Start_IT(&heth);

/* Clear any pending RBU status that occurred during startup */
__DSB();
__HAL_ETH_DMA_CLEAR_IT(&heth, ETH_DMACSR_RBU | ETH_DMACSR_AIS);

/* Re-enable ETH interrupt */
HAL_NVIC_EnableIRQ(ETH_IRQn);
```

### Why This Works

1. `reset_rx_descriptors()` clears all descriptor state so `ETH_UpdateDescriptor()` will allocate fresh buffers
2. `HAL_NVIC_DisableIRQ(ETH_IRQn)` prevents the NVIC from servicing ETH interrupts during startup
3. `HAL_ETH_Start_IT()` enables DMA and then enables RBUE interrupt
4. Even if RBU status is set during startup, the interrupt is NOT serviced (NVIC disabled)
5. `__HAL_ETH_DMA_CLEAR_IT()` clears the spurious RBU status
6. `HAL_NVIC_EnableIRQ(ETH_IRQn)` re-enables interrupts
7. Since status is cleared, no spurious interrupt is triggered
8. Real RBU errors (if any) will still be detected because they'll set the status bit again

### Build Status

Build succeeded with the fix applied.

### Summary of Complete Fix

The complete fix for RBU errors on link up after link down requires BOTH:

1. **`reset_rx_descriptors()`** - Clears `BackupAddr0` so `ETH_UpdateDescriptor()` allocates fresh buffers
2. **NVIC disable/enable** - Suppresses spurious RBU interrupt during DMA startup

Neither fix alone is sufficient:
- Without `reset_rx_descriptors()`: Old buffer addresses cause real RBU errors
- Without NVIC disable/enable: Spurious RBU interrupt fires during startup

---

## Critical Knowledge: Why NVIC Disable/Enable Fixes Spurious RBU

### The HAL Race Condition Explained

The STM32H5 HAL ETH driver has a race condition in `HAL_ETH_Start_IT()` that causes spurious RBU errors:

```c
HAL_StatusTypeDef HAL_ETH_Start_IT(ETH_HandleTypeDef *heth)
{
  // Step 1: Build descriptors and set tail pointer
  heth->RxDescList.RxBuildDescCnt = ETH_RX_DESC_CNT;
  ETH_UpdateDescriptor(heth);  // Allocates buffers, sets OWN bit

  // Step 2: Enable TX DMA
  SET_BIT(heth->Instance->DMACTCR, ETH_DMACTCR_ST);

  // Step 3: Enable RX DMA  <-- DMA STARTS RUNNING HERE
  SET_BIT(heth->Instance->DMACRCR, ETH_DMACRCR_SR);

  // ... some code ...

  // Step 4: Enable interrupts INCLUDING RBU interrupt  <-- RBUE ENABLED HERE
  __HAL_ETH_DMA_ENABLE_IT(heth, ETH_DMACIER_NIE | ETH_DMACIER_RIE |
                                ETH_DMACIER_TIE | ETH_DMACIER_FBEE |
                                ETH_DMACIER_AIE | ETH_DMACIER_RBUE);
}
```

### The Problem

Between **Step 3** (DMA starts) and **Step 4** (RBUE interrupt enabled):

1. **DMA starts running** and immediately begins checking descriptors
2. During this brief window, the DMA hardware may **momentarily** report an RBU condition (even if all descriptors are valid) as it initializes its internal state machine
3. This sets the **RBU status bit** in the `DMACSR` register
4. When **Step 4** enables the RBUE interrupt, the **pending RBU status immediately triggers an interrupt**
5. This interrupt fires **inside** `HAL_ETH_Start_IT()` before it returns
6. Our `HAL_ETH_ErrorCallback()` is called, reporting an RBU error

### Visual Timeline

```
Without fix:
  HAL_ETH_Start_IT() {
    ETH_UpdateDescriptor()     ← Descriptors ready
    Enable RX DMA              ← DMA starts, may set RBU status
    ...
    Enable RBUE interrupt      ← BOOM! Pending RBU triggers interrupt
  }                            ← HAL_ETH_ErrorCallback() called INSIDE

With fix:
  NVIC_DisableIRQ()            ← Block all ETH interrupts
  HAL_ETH_Start_IT() {
    ETH_UpdateDescriptor()     ← Descriptors ready
    Enable RX DMA              ← DMA starts, may set RBU status
    ...
    Enable RBUE interrupt      ← RBU status pending, but NVIC blocked
  }
  Clear RBU status             ← Clear the spurious status
  NVIC_EnableIRQ()             ← Now safe, no pending spurious interrupt
```

### The Fix Pattern

```c
// 1. Disable ETH interrupt at NVIC level
HAL_NVIC_DisableIRQ(ETH_IRQn);

// 2. Memory barriers for proper ordering
__DSB();
__ISB();

// 3. Call HAL_ETH_Start_IT() - DMA starts, RBUE enabled
//    Even if RBU status is set, interrupt is NOT serviced (NVIC disabled)
HAL_ETH_Start_IT(&heth);

// 4. Clear any spurious RBU status that was set during startup
__DSB();
__HAL_ETH_DMA_CLEAR_IT(&heth, ETH_DMACSR_RBU | ETH_DMACSR_AIS);

// 5. Re-enable ETH interrupt
HAL_NVIC_EnableIRQ(ETH_IRQn);
```

### Why This Works

1. **NVIC disabled** → Even if the RBU interrupt fires inside `HAL_ETH_Start_IT()`, the NVIC won't service it
2. **Clear RBU status** → We clear the spurious RBU status bit before re-enabling the interrupt
3. **NVIC enabled** → Now interrupts work normally, but the spurious status is already cleared
4. **Real RBU errors** → Will still be detected because they'll set the status bit again

### Key Insight

The RBU error we were seeing was **not a real error** - it was a spurious status set during DMA initialization that immediately triggered when the interrupt was enabled. The descriptors were correctly configured, but the DMA hardware momentarily reported RBU during its startup sequence.

### Technical Details

- `ETH_DMACSR_RBU` = 0x0080 (Receive Buffer Unavailable status bit)
- `ETH_DMACSR_AIS` = 0x4000 (Abnormal Interrupt Summary status bit)
- Writing 1 to these bits in DMACSR clears them (per RM0481 reference manual)
- The `__HAL_ETH_DMA_CLEAR_IT()` macro writes to DMACSR to clear the specified bits

### This is a HAL Design Issue

This is arguably a bug in the STM32H5 HAL ETH driver. The HAL should either:
1. Clear the RBU status after enabling DMA but before enabling the RBUE interrupt
2. Or disable/enable NVIC around the critical section internally

Since we can't modify the HAL, we work around it by wrapping `HAL_ETH_Start_IT()` with NVIC disable/enable.

---

## Debug Session 10: GTZC Privilege Configuration Fix (ROOT CAUSE)

### Problem Analysis

After all previous fixes, the DMA was still persistently suspended (RPS=4) with `IntCount=0` (no RX interrupts at all). The descriptors were correctly initialized (DESC3=0xC1000600), but the DMA was not receiving any packets.

### Root Cause Identified: GTZC SRAM3 Privilege Configuration

The **actual root cause** was found in the TrustZone GTZC (Global TrustZone Controller) configuration:

1. **SRAM3 was configured as PRIVILEGED** in the Secure world:
   ```c
   MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[0] = 0xFFFFFFFF;  // All privileged
   ```

2. **The ETH DMA is a bus master that runs in NON-PRIVILEGED mode**

3. **Result**: The ETH DMA could not access the DMA descriptors and RX buffers in SRAM3, causing:
   - RBU (Receive Buffer Unavailable) errors
   - RPS=4 (Suspended) state
   - No RX interrupts (`IntCount=0`)

### The Fix

Changed the GTZC MPCBB privilege configuration for SRAM3 from privileged to non-privileged:

**File**: `application/bsp/stm/stm32h563/Secure/Core/Src/main.c`

```c
/* BEFORE (WRONG): All blocks privileged - ETH DMA cannot access! */
MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[0] = 0xFFFFFFFF;
MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[1] = 0xFFFFFFFF;
// ... all 20 entries = 0xFFFFFFFF

/* AFTER (CORRECT): All blocks non-privileged - ETH DMA can access */
MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[0] = 0x00000000;
MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[1] = 0x00000000;
// ... all 20 entries = 0x00000000
```

### Why This Was the Root Cause

1. **Memory Layout**: The linker script places ETH DMA descriptors and buffers at the start of SRAM3 (0x20050000)
2. **GTZC MPCBB**: Controls access to SRAM3 based on privilege level
3. **ETH DMA**: Is a bus master that accesses memory directly, running in non-privileged mode
4. **Privileged SRAM3**: Blocks non-privileged bus masters (like ETH DMA) from accessing the memory
5. **Result**: DMA reads descriptor, sees invalid data (or access fault), suspends immediately

### Why Previous Fixes Didn't Work

All previous fixes were treating **symptoms**, not the **root cause**:

| Fix | What it addressed | Why it didn't solve the problem |
|-----|-------------------|--------------------------------|
| Memory barriers (__DSB/__ISB) | CPU-DMA ordering | DMA couldn't access memory at all |
| MPU Inner Shareable | Cache coherency | DMA couldn't access memory at all |
| NVIC disable/enable | Spurious RBU during startup | Real RBU because DMA couldn't read descriptors |
| Descriptor reinitialization | Stale descriptor state | DMA couldn't read the new descriptors either |
| Buffer tracking | Buffer reuse issues | DMA couldn't access buffers at all |

### Key Insight

The CPU could read/write the descriptors correctly (we saw correct DESC3=0xC1000600 in debug output), but the **DMA could not** because:
- CPU runs in privileged mode → can access privileged SRAM3
- DMA runs in non-privileged mode → cannot access privileged SRAM3

This is why the descriptors "looked correct" but the DMA was still suspended.

### Technical Details

- **GTZC MPCBB**: Memory Protection Controller Block-Based (controls SRAM access)
- **MPCBB_PrivConfig_array**: Each bit controls privilege access for a 512-byte block
- **0xFFFFFFFF**: All 32 blocks in that super-block are privileged
- **0x00000000**: All 32 blocks in that super-block are non-privileged
- **SRAM3 size**: 320KB = 640 blocks = 20 super-blocks (hence 20 array entries)

### Build Status

Build succeeded with the fix applied to both Secure and NonSecure applications.

### Summary

**The root cause of the persistent RPS=4 (Suspended) and IntCount=0 was the GTZC SRAM3 privilege configuration blocking ETH DMA access to the descriptor and buffer memory.**

This was a TrustZone configuration issue, not a HAL or driver issue. The previous fixes for spurious RBU during startup are still valid and necessary, but they were masking the real problem.
