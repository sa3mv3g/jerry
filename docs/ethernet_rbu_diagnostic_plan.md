# STM32H5 Ethernet RBU Root Cause Diagnostic Plan

## Problem Statement
RBU (Receive Buffer Unavailable) errors flood continuously when Ethernet cable is connected, even though:
- All 8 descriptors show OWN=1 (DMA owns them)
- All descriptors have valid buffer addresses (BUF1V=1)
- Buffers are in non-cacheable SRAM2 region

This suggests the DMA is NOT actually seeing valid descriptors, despite what the CPU reads.

---

## Diagnostic Action Plan

### Phase 1: Hardware-Level Verification

#### 1.1 Verify DMA is Actually Reading Descriptors
**Goal**: Confirm DMA sees the same descriptor values as CPU

**Action**: Add a "canary" pattern to unused descriptor fields
```c
// In ETH_UpdateDescriptor or HAL_ETH_RxAllocateCallback
dmarxdesc->DESC1 = 0xDEADBEEF;  // Unused in normal RX
dmarxdesc->DESC2 = 0xCAFEBABE;  // Should be preserved
```

**Check in ErrorCallback**:
- If DESC1/DESC2 are corrupted → Memory coherency issue
- If DESC1/DESC2 are intact → DMA is reading correctly

#### 1.2 Check DMA Current Descriptor Pointer
**Goal**: See which descriptor DMA is stuck on

**Action**: Read DMACCATDR (Current Application Transmit Descriptor) and DMACCARDR (Current Application Receive Descriptor) registers
```c
// In ErrorCallback
ETH_DEBUG("DMACCARDR=0x%08lX (current RX desc)",
          READ_REG(heth_param->Instance->DMACCARDR));
ETH_DEBUG("DMACCATDR=0x%08lX (current TX desc)",
          READ_REG(heth_param->Instance->DMACCATDR));
```

**Analysis**:
- Compare DMACCARDR with descriptor list addresses
- If stuck on same descriptor → That specific descriptor has an issue
- If cycling through descriptors → DMA is working but something else triggers RBU

#### 1.3 Check RX DMA State Machine
**Goal**: Understand what state the RX DMA is in

**Action**: Read DMADSR (DMA Debug Status Register) if available, or check DMACSR bits
```c
// In ErrorCallback
uint32_t dmacsr = READ_REG(heth_param->Instance->DMACSR);
ETH_DEBUG("RPS (RX Process State) = %lu", (dmacsr >> 8) & 0x7);
// 0=Stopped, 1=Running(fetch), 2=Reserved, 3=Running(wait),
// 4=Suspended, 5=Running(close), 6=Timestamp, 7=Running(transfer)
```

**Analysis**:
- RPS=4 (Suspended) → DMA stopped due to RBU, needs tail pointer write
- RPS=1 (Fetching) → DMA is trying to fetch descriptors
- RPS=7 (Transferring) → DMA is actively receiving

---

### Phase 2: Timing Analysis

#### 2.1 Capture Exact Timing of RBU
**Goal**: Understand when RBU occurs relative to other events

**Action**: Use GPIO toggling for oscilloscope/logic analyzer
```c
// Define a debug GPIO (e.g., unused LED pin)
#define DEBUG_GPIO_PORT GPIOB
#define DEBUG_GPIO_PIN  GPIO_PIN_0

// In HAL_ETH_IRQHandler (modify stm32h5xx_it.c)
HAL_GPIO_WritePin(DEBUG_GPIO_PORT, DEBUG_GPIO_PIN, GPIO_PIN_SET);
HAL_ETH_IRQHandler(&heth);
HAL_GPIO_WritePin(DEBUG_GPIO_PORT, DEBUG_GPIO_PIN, GPIO_PIN_RESET);

// In HAL_ETH_RxCpltCallback
HAL_GPIO_TogglePin(DEBUG_GPIO_PORT, DEBUG_GPIO_PIN);
```

**Analysis with Logic Analyzer**:
- Measure time between RBU interrupts
- Correlate with packet arrival (if you have access to network tap)
- Check if RBU happens before any RxCplt

#### 2.2 Check if RBU Happens Before First Packet
**Goal**: Determine if RBU is triggered by packet arrival or something else

**Action**: Add sequence tracking
```c
static volatile uint32_t RxCpltCount = 0;
static volatile uint32_t FirstRbuAfterRxCplt = 0;

void HAL_ETH_RxCpltCallback(...) {
    RxCpltCount++;
}

void HAL_ETH_ErrorCallback(...) {
    if (error & ETH_DMACSR_RBU) {
        if (RxCpltCount == 0) {
            ETH_DEBUG("RBU before ANY RxCplt!");
        } else if (FirstRbuAfterRxCplt == 0) {
            FirstRbuAfterRxCplt = RxCpltCount;
            ETH_DEBUG("First RBU after %lu RxCplt", RxCpltCount);
        }
    }
}
```

---

### Phase 3: Memory and Cache Analysis

#### 3.1 Verify MPU Configuration
**Goal**: Confirm descriptors and buffers are truly non-cacheable

**Action**: Dump MPU region configuration at runtime
```c
void dump_mpu_config(void) {
    for (int i = 0; i < 8; i++) {
        MPU->RNR = i;
        uint32_t rbar = MPU->RBAR;
        uint32_t rlar = MPU->RLAR;
        if (rlar & 1) {  // Region enabled
            ETH_DEBUG("MPU[%d]: RBAR=0x%08lX, RLAR=0x%08lX", i, rbar, rlar);
            ETH_DEBUG("  Base=0x%08lX, Limit=0x%08lX, AttrIdx=%lu",
                      rbar & 0xFFFFFFE0, rlar & 0xFFFFFFE0, (rlar >> 1) & 0x7);
        }
    }
}
```

**Check**:
- Descriptor region (0x20050000) should have Device or Non-cacheable attribute
- Buffer region should also be non-cacheable
- No overlapping regions with different attributes

#### 3.2 Test with Cache Disabled Globally
**Goal**: Rule out cache as the root cause

**Action**: Temporarily disable D-Cache
```c
// In main() before any ETH init
SCB_DisableDCache();
// Or just don't enable it: comment out SCB_EnableDCache();
```

**Analysis**:
- If RBU stops → Cache coherency is the root cause
- If RBU continues → Cache is not the issue

#### 3.3 Manual Cache Invalidation Test
**Goal**: Force cache coherency before DMA operations

**Action**: Add explicit cache operations
```c
// Before reading descriptors in ErrorCallback
SCB_InvalidateDCache_by_Addr((void*)DMARxDscrTab, sizeof(DMARxDscrTab));

// After writing descriptors in ETH_UpdateDescriptor
SCB_CleanDCache_by_Addr((void*)dmarxdesc, sizeof(ETH_DMADescTypeDef));
```

---

### Phase 4: Network Traffic Analysis

#### 4.1 Test with Controlled Traffic
**Goal**: Determine if specific packet types trigger RBU

**Action**: Use different traffic patterns
1. **No traffic**: Just cable connected, no ping/data
2. **Single ping**: `ping -c 1 <device_ip>`
3. **Continuous ping**: `ping <device_ip>`
4. **Broadcast storm**: Connect to busy network
5. **Specific packet size**: `ping -s 64 <device_ip>` vs `ping -s 1400 <device_ip>`

**Log for each test**:
- Time to first RBU
- RBU rate (errors per second)
- Whether any packets are successfully received

#### 4.2 Test with Different Network Equipment
**Goal**: Rule out USB-Ethernet adapter issues

**Action**:
1. Connect directly to a managed switch (not USB adapter)
2. Connect to a different computer's Ethernet port
3. Use a crossover cable for direct connection
4. Try a different USB-Ethernet adapter

**Analysis**:
- If RBU only with USB adapter → Adapter sends malformed packets or unusual traffic
- If RBU with all equipment → Issue is on STM32 side

#### 4.3 Capture Network Traffic
**Goal**: See what packets are being sent to the device

**Action**: Use Wireshark on the connected computer
```bash
# Filter for traffic to/from the STM32 MAC address
eth.addr == 00:80:e1:00:00:01
```

**Look for**:
- Broadcast storms (ARP, DHCP, mDNS)
- Malformed packets
- Jumbo frames (if MTU mismatch)
- VLAN tagged frames (if unexpected)

---

### Phase 5: HAL Driver Analysis

#### 5.1 Trace HAL_ETH_ReadData Execution
**Goal**: Understand if ReadData is being called and what it does

**Action**: Add detailed tracing
```c
HAL_StatusTypeDef HAL_ETH_ReadData_Traced(ETH_HandleTypeDef *heth, void **pAppBuff)
{
    uint32_t descidx = heth->RxDescList.RxDescIdx;
    ETH_DMADescTypeDef *dmarxdesc = (ETH_DMADescTypeDef *)heth->RxDescList.RxDesc[descidx];

    ETH_DEBUG("ReadData: idx=%lu, DESC3=0x%08lX, OWN=%lu",
              descidx, dmarxdesc->DESC3, (dmarxdesc->DESC3 >> 31) & 1);

    HAL_StatusTypeDef result = HAL_ETH_ReadData(heth, pAppBuff);

    ETH_DEBUG("ReadData: result=%d, pAppBuff=%p", result, *pAppBuff);
    return result;
}
```

#### 5.2 Check Descriptor Ring Consistency
**Goal**: Verify descriptor ring is properly formed

**Action**: Validate descriptor addresses
```c
void validate_descriptor_ring(void) {
    ETH_DEBUG("Validating RX descriptor ring:");
    ETH_DEBUG("  DMACRDLAR=0x%08lX (should be 0x%08lX)",
              READ_REG(heth.Instance->DMACRDLAR), (uint32_t)DMARxDscrTab);
    ETH_DEBUG("  DMACRDRLR=%lu (should be %d)",
              READ_REG(heth.Instance->DMACRDRLR) + 1, ETH_RX_DESC_CNT);

    for (int i = 0; i < ETH_RX_DESC_CNT; i++) {
        ETH_DMADescTypeDef *desc = &DMARxDscrTab[i];
        ETH_DEBUG("  Desc[%d] @ 0x%08lX: DESC0=0x%08lX, DESC3=0x%08lX",
                  i, (uint32_t)desc, desc->DESC0, desc->DESC3);

        // Check DESC0 points to valid buffer
        if (desc->DESC0 < 0x20000000 || desc->DESC0 > 0x20100000) {
            ETH_DEBUG("    WARNING: DESC0 outside SRAM range!");
        }

        // Check OWN bit
        if ((desc->DESC3 & 0x80000000) == 0) {
            ETH_DEBUG("    WARNING: OWN=0, CPU owns this descriptor!");
        }

        // Check BUF1V bit
        if ((desc->DESC3 & 0x01000000) == 0) {
            ETH_DEBUG("    WARNING: BUF1V=0, buffer not valid!");
        }
    }
}
```

#### 5.3 Check for HAL State Machine Issues
**Goal**: Verify HAL state is consistent

**Action**: Log HAL state
```c
ETH_DEBUG("HAL State: gState=%d, RxState=%d, ErrorCode=0x%08lX",
          heth.gState, heth.RxState, heth.ErrorCode);
ETH_DEBUG("RxDescList: RxDescIdx=%lu, RxBuildDescIdx=%lu, RxBuildDescCnt=%lu",
          heth.RxDescList.RxDescIdx,
          heth.RxDescList.RxBuildDescIdx,
          heth.RxDescList.RxBuildDescCnt);
```

---

### Phase 6: Alternative Approaches

#### 6.1 Test with Polling Mode (No Interrupts)
**Goal**: Determine if interrupt handling is the issue

**Action**: Use HAL_ETH_Start() instead of HAL_ETH_Start_IT()
```c
// In low_level_init
HAL_ETH_Start(&heth);  // Polling mode, no interrupts

// In ethernetif_input_task, poll continuously
for (;;) {
    p = low_level_input(netif);
    if (p != NULL) {
        netif->input(p, netif);
    }
    vTaskDelay(1);  // Small delay to prevent CPU hogging
}
```

**Analysis**:
- If works in polling mode → Interrupt handling issue
- If still fails → Fundamental DMA/descriptor issue

#### 6.2 Test with Minimal Descriptor Count
**Goal**: Simplify the problem

**Action**: Reduce to 2 descriptors
```c
#define ETH_RX_DESC_CNT 2
```

**Analysis**:
- Easier to trace with fewer descriptors
- May reveal race conditions

#### 6.3 Test with ST's CubeMX Generated Code
**Goal**: Compare with known-working reference

**Action**:
1. Create new CubeMX project for same board
2. Enable ETH + LwIP with default settings
3. Generate code and test
4. Compare register configurations

---

### Phase 7: Deep Dive Experiments

#### 7.1 Descriptor Memory Location Test
**Goal**: Test if descriptor location matters

**Action**: Try different memory regions
```c
// Option A: Descriptors in DTCM (if available)
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".dtcm")));

// Option B: Descriptors in regular SRAM1
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".data")));

// Option C: Descriptors at fixed address
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".eth_desc"), aligned(32)));
```

#### 7.2 Buffer Alignment Test
**Goal**: Test if buffer alignment matters

**Action**: Try different alignments
```c
// 4-byte aligned (minimum)
static uint8_t RxBuff[ETH_RX_DESC_CNT][ETH_RX_BUFFER_SIZE] __attribute__((aligned(4)));

// 32-byte aligned (cache line)
static uint8_t RxBuff[ETH_RX_DESC_CNT][ETH_RX_BUFFER_SIZE] __attribute__((aligned(32)));

// 64-byte aligned
static uint8_t RxBuff[ETH_RX_DESC_CNT][ETH_RX_BUFFER_SIZE] __attribute__((aligned(64)));
```

#### 7.3 DMA Burst Length Test
**Goal**: Test if DMA burst configuration matters

**Action**: Modify DMA configuration
```c
ETH_DMAConfigTypeDef dmaconf;
HAL_ETH_GetDMAConfig(&heth, &dmaconf);

// Try different burst lengths
dmaconf.RxDMABurstLength = ETH_RXDMABURSTLENGTH_1BEAT;  // Minimum
// or
dmaconf.RxDMABurstLength = ETH_RXDMABURSTLENGTH_4BEAT;
// or
dmaconf.RxDMABurstLength = ETH_RXDMABURSTLENGTH_32BEAT; // Maximum

HAL_ETH_SetDMAConfig(&heth, &dmaconf);
```

---

## Diagnostic Code Template

Add this diagnostic function to ethernetif.c:

```c
void eth_deep_diagnostic(ETH_HandleTypeDef *heth_param)
{
    ETH_DEBUG("========== ETH DEEP DIAGNOSTIC ==========");

    // 1. HAL State
    ETH_DEBUG("HAL: gState=%d, ErrorCode=0x%08lX",
              heth_param->gState, heth_param->ErrorCode);

    // 2. DMA Registers
    ETH_DEBUG("DMA Registers:");
    ETH_DEBUG("  DMAMR=0x%08lX", READ_REG(heth_param->Instance->DMAMR));
    ETH_DEBUG("  DMASBMR=0x%08lX", READ_REG(heth_param->Instance->DMASBMR));
    ETH_DEBUG("  DMACSR=0x%08lX", READ_REG(heth_param->Instance->DMACSR));
    ETH_DEBUG("  DMACIER=0x%08lX", READ_REG(heth_param->Instance->DMACIER));

    // 3. RX DMA Registers
    ETH_DEBUG("RX DMA:");
    ETH_DEBUG("  DMACRCR=0x%08lX (SR=%lu)",
              READ_REG(heth_param->Instance->DMACRCR),
              READ_REG(heth_param->Instance->DMACRCR) & 1);
    ETH_DEBUG("  DMACRDLAR=0x%08lX", READ_REG(heth_param->Instance->DMACRDLAR));
    ETH_DEBUG("  DMACRDRLR=%lu", READ_REG(heth_param->Instance->DMACRDRLR) + 1);
    ETH_DEBUG("  DMACRDTPR=0x%08lX", READ_REG(heth_param->Instance->DMACRDTPR));

    // 4. RX Process State
    uint32_t dmacsr = READ_REG(heth_param->Instance->DMACSR);
    uint32_t rps = (dmacsr >> 8) & 0x7;
    const char* rps_names[] = {"Stopped", "Fetching", "Reserved", "Waiting",
                               "Suspended", "Closing", "Timestamp", "Transferring"};
    ETH_DEBUG("  RX Process State: %lu (%s)", rps, rps_names[rps]);

    // 5. MAC Registers
    ETH_DEBUG("MAC:");
    ETH_DEBUG("  MACCR=0x%08lX", READ_REG(heth_param->Instance->MACCR));
    ETH_DEBUG("  MACECR=0x%08lX", READ_REG(heth_param->Instance->MACECR));

    // 6. All Descriptors
    ETH_DEBUG("Descriptors (expected at 0x%08lX):", (uint32_t)heth_param->Init.RxDesc);
    for (int i = 0; i < ETH_RX_DESC_CNT; i++) {
        ETH_DMADescTypeDef *desc = (ETH_DMADescTypeDef *)heth_param->RxDescList.RxDesc[i];
        ETH_DEBUG("  [%d] @0x%08lX: D0=0x%08lX D1=0x%08lX D2=0x%08lX D3=0x%08lX Bk=0x%08lX",
                  i, (uint32_t)desc,
                  desc->DESC0, desc->DESC1, desc->DESC2, desc->DESC3, desc->BackupAddr0);
    }

    // 7. Memory regions
    ETH_DEBUG("Memory:");
    ETH_DEBUG("  Descriptors: 0x%08lX - 0x%08lX",
              (uint32_t)DMARxDscrTab,
              (uint32_t)DMARxDscrTab + sizeof(DMARxDscrTab));
    ETH_DEBUG("  RX Buffers:  0x%08lX - 0x%08lX",
              (uint32_t)RxBuff,
              (uint32_t)RxBuff + sizeof(RxBuff));

    ETH_DEBUG("==========================================");
}
```

---

## Priority Order for Investigation

1. **Phase 4.1** - Test with no traffic (just cable connected)
2. **Phase 6.1** - Test polling mode
3. **Phase 3.2** - Disable D-Cache globally
4. **Phase 1.2** - Check DMACCARDR (current descriptor)
5. **Phase 1.3** - Check RX Process State
6. **Phase 5.2** - Validate descriptor ring
7. **Phase 4.2** - Test with different network equipment

Start with the simplest tests that can quickly rule out major categories of issues.

---

## Expected Outcomes

| Test | If RBU Stops | If RBU Continues |
|------|--------------|------------------|
| No traffic | Network traffic is trigger | Hardware/init issue |
| Polling mode | Interrupt handling bug | DMA/descriptor issue |
| D-Cache disabled | Cache coherency problem | Not cache related |
| Different equipment | USB adapter issue | STM32 side issue |
| 2 descriptors | Race condition | Fundamental issue |
| CubeMX reference | Custom code bug | Hardware/silicon issue |
