# Engineering Requirements: `embedded-log` Integration and Precision Profiling

## 1. Overview
The system currently relies on raw `printf` calls for logging throughout the application and BSP layers. This approach lacks structured severity levels, precise timestamping, and is difficult to parse or filter. 
This document outlines the requirements to integrate the `to9/embedded-log` library, replacing all existing `printf` calls. Additionally, it details the implementation of a precise timestamping mechanism using the RTC SubSecond Register (SSR) for logging, and a separate high-resolution DWT timer utility for performance profiling.

## 2. Requirements

### 2.1 Dependency Management
* **REQ-DEP-01:** The `to9/embedded-log` library SHALL be integrated using CMake `FetchContent`.
* **REQ-DEP-02:** The integration SHALL ensure that developers are not required to manually manage git submodules to build the project.
* **REQ-DEP-03:** The CMake configuration SHALL compile the library and link it statically against the application target.

### 2.2 Logging Infrastructure
* **REQ-LOG-01:** An adapter layer (`logging_port.c` / `logging_port.h` or equivalent) SHALL be created to initialize and configure the `embedded-log` library.
* **REQ-LOG-02:** The adapter SHALL redirect the output of `embedded-log` to the existing UART/console backend currently used by `printf`.
* **REQ-LOG-03:** All existing occurrences of `printf` across `application/src/` and `application/bsp/` SHALL be replaced with the appropriate `embedded-log` macros (e.g., info, warn, error).
* **REQ-LOG-04:** The logging severity levels SHALL be chosen semantically based on the context of the original `printf` message.

### 2.3 Timestamping (RTC)
* **REQ-TS-01:** The `embedded-log` adapter SHALL provide a custom time provider function that retrieves the current time.
* **REQ-TS-02:** The time provider SHALL utilize the existing `rtc_manager.h` API to retrieve the current RTC time instead of directly calling HAL APIs, ensuring consistency with the rest of the application.
* **REQ-TS-03:** To achieve sub-second precision, the time provider SHALL read and incorporate the RTC SubSecond Register (SSR), formatting the output down to milliseconds.
* **REQ-TS-04:** The timestamp format SHALL remain consistent across all log messages.

### 2.4 Performance Profiling (DWT)
* **REQ-PROF-01:** No dedicated DWT utility module is required.
* **REQ-PROF-02:** For performance profiling, developers SHALL manually log the DWT cycle counter values before and after a block of code using standard `embedded-log` macros.

## 3. Implementation Steps
1. **CMake Setup:** Update `application/CMakeLists.txt` to include `FetchContent` for `to9/embedded-log`.
2. **Adapter Creation:** Implement the initialization, write hook, and time provider hook in a new porting file.
3. **Time Logic:** Implement the RTC SSR read logic using `rtc_manager.h`.
4. **Refactor:** Perform a codebase-wide find-and-replace to swap `printf` for `embedded-log` macros.
5. **Validation:** Build the project and visually inspect the UART output for correctly formatted timestamps, log levels, and content.