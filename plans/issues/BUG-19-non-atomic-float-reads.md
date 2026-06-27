# [BUG-19] Non-atomic 32-bit float reads — value can "tear" within and across Modbus transactions

**Severity:** 🟠 High
**Component:** Modbus / device callbacks + docs
**File:** `application/src/modbus_device_callbacks.c` (lines ~1004-1073 holding regs, ~1473-1503 input regs)

## Description

A 32-bit float spans **two** Modbus registers (a convention; Modbus has no native 32-bit type). The read callbacks recompute/copy the value **per register case** rather than taking a single snapshot. For input registers it is worse: each calibrated-value case calls `update_calibrated_adcval()`, which **re-reads the ADC and recomputes the float during the read callback**.

Two tearing scenarios:

1. **Within a single 2-register transaction** — the float is produced once for `addr == base` and again for `addr == base + 1`, so the two halves can come from different computations/ADC acquisitions.
2. **Across two separate transactions** — if a master reads the high word in one request and the low word in another, the input is re-sampled in between, so the reassembled float is a corrupt bit pattern.

This is masked today by BUG-02/03 (data never reaches the master) but becomes live once those are fixed.

## Fix

- **Snapshot once at callback entry:** compute every multi-register value a single time into the backing struct before the per-register loop; the loop then only copies frozen words by relative index `i`. Fixes the within-transaction tear and the duplicate computation.
- **Document the master-side contract:** a master must read both words of a float in **one** transaction. Cross-transaction tearing cannot be solved on the slave alone.
- **Optional:** a "latch/freeze" coil to snapshot all volatile values before a multi-transaction read.

## Documentation targets (master-side contract)

1. `config/jerry_registers.json` — device-level note on 32-bit-pair atomicity (type info already present: `float32`/`uint32`, `Size 2`).
2. `tools/modbus_codegen/` template — emit an atomicity legend/footnote for `Size 2` registers in the generated `jerry_device_register_map.txt` (do not hand-edit the generated file).
3. `README.md` — Modbus section integrator note (two-register convention, word order, single-transaction requirement).
4. (Optional) `docs/FIRMWARE_README.md` cross-reference.
5. (Optional) integration test in `tests/integration/test_holding_registers.py` reading each `Size 2` register in one transaction.

## Related

- BUG-02, BUG-03 (wrong index; fix together)
- BUG-04 (snapshot should occur under the register mutex)
