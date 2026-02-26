#!/usr/bin/env bash
# flash.sh – load ELF via GDB through the SSH-tunnelled OpenOCD GDB server.
#
# OpenOCD must be running on the laptop with connect_assert_srst to work
# around the STM32F746 reconfiguring SWD pins after boot:
#
#   openocd -f board/stm32f7discovery.cfg \
#     -c "adapter speed 200" \
#     -c "reset_config srst_only srst_nogate connect_assert_srst" \
#     -c "init; reset halt"
#
# Then start the tunnel (dev_up.sh), which forwards laptop:3333 → VM:13333.
set -euo pipefail

ELF="${1:-build/display_test.elf}"

gdb-multiarch -q "$ELF" \
  -ex "set architecture armv7e-m" \
  -ex "target remote localhost:13333" \
  -ex "monitor reset halt" \
  -ex "load" \
  -ex "monitor reset run" \
  -ex "detach" \
  -ex "quit"

