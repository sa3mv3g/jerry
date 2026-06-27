# Jerry — Bug Issue Files

One Markdown file per bug, ready to copy-paste into a GitHub issue
(`https://github.com/sa3mv3g/jerry`). Each file's H1 is the suggested issue **title**;
everything below it is the issue **body**.

Source reviews:
- [`plans/codebase_bug_review.md`](../codebase_bug_review.md) — original review (BUG-*)
- [`plans/codebase_bug_review_2.md`](../codebase_bug_review_2.md) — fresh independent review (N-*)

> **False positives (no issue file):** BUG-01 and BUG-06 from the first review were re-validated
> against the actually-compiled `application/src/generated/` files and found to be artifacts of a
> stale, non-compiled repo-root `generated/` copy (since deleted). They are intentionally omitted.

## Original review (BUG-*)

| GH | Issue | Severity | Summary |
|----|-------|----------|---------|
| #23 | [BUG-02](BUG-02-hr-float-indexing.md) | 🔴 Critical | HR float reads return garbage (absolute vs relative index) |
| #24 | [BUG-03](BUG-03-ir-calibrated-indexing.md) | 🔴 Critical | Calibrated ADC input-register reads return garbage |
| #25 | [BUG-04](BUG-04-register-thread-safety.md) | 🔴 Critical | Shared register data accessed from multiple tasks, no mutex |
| #26 | [BUG-05](BUG-05-eeprom-always-write.md) | 🟠 High | EEPROM float `==` skip — replace with always-write |
| #27 | [BUG-07](BUG-07-stale-coil-struct.md) | 🟠 High | `read_coils` doesn't sync `digital_output_x` struct fields |
| #28 | [BUG-08](BUG-08-stack-overflow-led-blink.md) | 🟠 High | Stack-overflow hook → characteristic RED-LED blink |
| #29 | [BUG-09](BUG-09-lcd-semaphore-loss.md) | 🟠 High | LCD binary semaphore can lose updates |
| #39 | [BUG-19](BUG-19-non-atomic-float-reads.md) | 🟠 High | Non-atomic 32-bit float reads (tearing) + doc contract |
| #30 | [BUG-10](BUG-10-atoi-usage.md) | 🟡 Medium | `atoi()` usage + `stdlib.h` include |
| #31 | [BUG-11](BUG-11-null-deref-netconn-stats.md) | 🟡 Medium | NULL deref of `lwip_stats.memp[MEMP_NETCONN]` |
| #32 | [BUG-12](BUG-12-static-reentrancy.md) | 🟡 Medium | `static` locals not reentrant (latent) |
| #33 | [BUG-13](BUG-13-quantity-zero-underflow.md) | 🟡 Medium | `quantity=0` end_address underflow |
| #34 | [BUG-14](BUG-14-inverted-rtos-guard.md) | 🟡 Medium | Inverted `#ifndef BSP_USING_RTOS` guard |
| #35 | [BUG-15](BUG-15-fota-dead-define.md) | 🔵 Low | Unused `#define CONST 10000` |
| #36 | [BUG-16](BUG-16-unused-modbusdata-struct.md) | 🔵 Low | Unused `ModbusData_t` struct |
| #37 | [BUG-17](BUG-17-islcdready-pointer-bug.md) | 🔵 Low* | `IsLcdReady` passes pointer-to-handle (potential crash) |
| #38 | [BUG-18](BUG-18-semaphore-typo.md) | 🔵 Low | Typo `gUdpateLcdSem` → `gUpdateLcdSem` |

\* BUG-17 is filed Low per the original report but is a potential crash — consider prioritizing.

## Fresh review (N-*)

| GH | Issue | Severity | Summary |
|----|-------|----------|---------|
| #40 | [N-01](N-01-racing-digital-input-updates.md) | 🔴 Critical | Racing DI/LCD updates — Main task owns ALL LCD writes |
| #41 | [N-02](N-02-coil-restore-endianness.md) | 🔴 Critical | Boot coil restore: `uint16_t`→`uint8_t*` endianness/aliasing |
| #42 | [N-03](N-03-modbus-tcp-no-reassembly.md) | 🔴 Critical | Modbus TCP truncation / no frame reassembly |
| #43 | [N-04](N-04-modbus-task-stack-pressure.md) | 🟠 High | Large per-case stack buffers pressure Modbus task stack |
| #44 | [N-05](N-05-ethernet-link-double-handling.md) | 🟠 High | Ethernet link handled twice (poll + callback) |
| #45 | [N-06](N-06-tcp-echo-write-error-ignored.md) | 🟠 High | TCP echo ignores `netconn_write` errors / spins |
| #46 | [N-07](N-07-rtc-ms-underflow.md) | 🟠 High | RTC millisecond unsigned underflow |
| #47 | [N-08](N-08-lcd-float-format-overflow.md) | 🟠 High | LCD float formatting overflow (neg/NaN/Inf) |
| #48 | [N-09](N-09-main-di-read-unchecked.md) | 🟡 Medium | Main DI loop ignores `BSP_GPIODI_Read` return |
| #49 | [N-10](N-10-di-error-sentinel-logging.md) | 🟡 Medium | DI error sentinel `0xFF` logged as `255` |
| #50 | [N-11](N-11-shared-i2c-bus-mutex.md) | 🟡 Medium | Shared I2C bus needs BSP-level mutex |
| #51 | [N-12](N-12-rtc-modulo-masks-invalid.md) | 🟡 Medium | `% 100` masks invalid RTC values |
| #52 | [N-13](N-13-modbus-server-dead-after-startup-failure.md) | 🟡 Medium | Modbus server permanently dead after startup failure |
| #53 | [N-14](N-14-i2c-scanner-read-probe.md) | 🟡 Medium | I2C scanner read probe misses devices / glitches outputs |
| #54 | [N-15](N-15-netconn-error-log-level.md) | 🔵 Low | NETCONN pool error logged at INFO not ERR |
| #55 | [N-16](N-16-priority-cast-signedness.md) | 🔵 Low | Inconsistent priority cast signedness |
| #56 | [N-17](N-17-i2c-scanner-embedded-newlines.md) | 🔵 Low | Scanner embeds `\n` vs per-call timestamps |
| #57 | [N-18](N-18-format-ai-magic-numbers.md) | 🔵 Low | `format_ai()` magic numbers |
| #58 | [N-19](N-19-empty-task-stacks.md) | 🔵 Low | Empty stub tasks reserve full stacks |

## Creating the GitHub issues with `gh` (optional)

After `gh auth login` (GitHub.com → HTTPS → browser), from the repo root:

```powershell
# one issue from one file (title = first line stripped of leading "# ")
$f = "plans/issues/BUG-02-hr-float-indexing.md"
$title = (Get-Content $f -TotalCount 1) -replace '^#\s*',''
& "C:\Program Files\GitHub CLI\gh.exe" issue create --title "$title" --body-file $f

# all of them
Get-ChildItem plans/issues -Filter "*.md" | Where-Object { $_.Name -ne 'README.md' } | ForEach-Object {
    $title = (Get-Content $_.FullName -TotalCount 1) -replace '^#\s*',''
    & "C:\Program Files\GitHub CLI\gh.exe" issue create --title "$title" --body-file $_.FullName
}
```

> Optional: create severity labels first, e.g.
> `gh label create critical --color B60205`, `high --color D93F0B`,
> `medium --color FBCA04`, `low --color 0E8A16`, then add `--label <sev>` to each `issue create`.
