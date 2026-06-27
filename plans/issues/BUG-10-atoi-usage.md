# [BUG-10] `LcdManager_UpdateIpv4Address()` uses `atoi()` (MISRA-banned, no error handling) and pulls in `stdlib.h`

**Severity:** 🟡 Medium
**Component:** LCD manager
**File:** `application/src/lcd_manager.c` (lines ~193-208, include at line 4)

## Description

```c
gIpLastOctet = (uint8_t)atoi(last_dot + 1);
```

`atoi()` has undefined behavior for out-of-range input and reports no error. The `#include <stdlib.h>` may pull in dynamic-allocation symbols, conflicting with the project's no-dynamic-allocation requirement.

## Impact

Malformed IP strings silently parse to 0; potential linker concerns from `stdlib.h`.

## Fix

- Replace `atoi()` with `strtoul()` + error checking, or a small manual digit parser.
- Remove the `#include <stdlib.h>` if no longer needed.
