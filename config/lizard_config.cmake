# Copyright (c) 2026
# All rights reserved.
#
# Lizard configuration for CMake.
#
# All analysis parameters (thresholds, exclude patterns, paths) are defined in
# tools/run_lizard.py, which is the single source of truth for Lizard config.
# This file only passes the --release flag for Release builds so run_lizard.py
# selects XML output instead of HTML.

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(LIZARD_CLI_PARAMS --release)
else()
    set(LIZARD_CLI_PARAMS "")
endif()
