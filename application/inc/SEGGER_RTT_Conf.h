#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

// Size of the buffer for terminal output of target, up to host
#define BUFFER_SIZE_UP (2048)

// Size of the buffer for terminal input to target from host
#define BUFFER_SIZE_DOWN (16)

// Max. number of up-buffers (0 is terminal)
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS (2)

// Max. number of down-buffers (0 is terminal)
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS (2)

// Default mode for terminal
#define SEGGER_RTT_MODE_DEFAULT SEGGER_RTT_MODE_NO_BLOCK_SKIP

#endif
