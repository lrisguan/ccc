#!/bin/bash

#
# ccc
# Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
#
# This program is released under the terms of the GNU General Public License version 2(GPLv2).
# See https://opensource.org/licenses/GPL-2.0 for more information.
#
# Project homepage: https://github.com/lrisguan/ccc
# Description: A frontend of C
#
#

echo "[cmake] configuring..."
cmake -S . -B build
echo "[cmake] configured."

echo "[cmake] building..."
cmake --build build -j
echo "[cmake] built."

echo "[cmake] testing..."
ctest --test-dir ./build --output-on-failure
echo "[cmake] tested."
