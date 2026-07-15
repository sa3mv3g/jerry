#!/bin/bash

VENV_PATH=".venv"  # Update this path if your venv is located elsewhere
LOG_FILE="modbus_performance_results_$(date +%Y%m%d_%H%M%S).log"
COMMAND="uv run tests/integration/test_modbus_performance.py --host 192.168.0.200"
RUNS=10

# Activate virtual environment
if [ ! -f "$VENV_PATH/bin/activate" ]; then
    echo "ERROR: Virtual environment not found at '$VENV_PATH'" | tee -a "$LOG_FILE"
    exit 1
fi

source "$VENV_PATH/bin/activate"
echo "Virtual environment activated: $VENV_PATH" | tee -a "$LOG_FILE"

echo "Starting Modbus Performance Test - $(date)" | tee -a "$LOG_FILE"
echo "Command: $COMMAND" | tee -a "$LOG_FILE"
echo "Total Runs: $RUNS" | tee -a "$LOG_FILE"
echo "========================================" | tee -a "$LOG_FILE"

for i in $(seq 1 $RUNS); do
    echo "" | tee -a "$LOG_FILE"
    echo "--- Run $i of $RUNS | $(date) ---" | tee -a "$LOG_FILE"

    $COMMAND 2>&1 | tee -a "$LOG_FILE"
    EXIT_CODE=${PIPESTATUS[0]}

    echo "Exit Code: $EXIT_CODE" | tee -a "$LOG_FILE"
done

echo "" | tee -a "$LOG_FILE"
echo "========================================" | tee -a "$LOG_FILE"
echo "All $RUNS runs completed - $(date)" | tee -a "$LOG_FILE"

deactivate
echo "Virtual environment deactivated."