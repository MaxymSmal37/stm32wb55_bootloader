#!/bin/bash
set -e

# Build, then flash the STM32WB55 over ST-Link with OpenOCD.

./build.sh


# Build, then flash one or both firmware images to the STM32WB55 over
# ST-Link with OpenOCD.
#
# Usage:
#   ./flash.sh              # flash the bootloader (default; lives at 0x08000000)
#   ./flash.sh bootloader   # same as above
#   ./flash.sh app          # flash the application directly to its slot
#                            # (0x08008000) for bench testing, bypassing the
#                            # UART update protocol
#   ./flash.sh both         # flash bootloader + app in one OpenOCD session

TARGET="${1:-bootloader}"

BOOTLOADER_ELF=build/bootloader/Bootloader.elf
APP_ELF=build/app/STM32wb55_App.elf

./build.sh

case "$TARGET" in
  bootloader)
    openocd -f interface/stlink.cfg -f target/stm32wbx.cfg \
            -c "program ${BOOTLOADER_ELF} verify reset exit"
    ;;
  app)
    openocd -f interface/stlink.cfg -f target/stm32wbx.cfg \
            -c "program ${APP_ELF} verify reset exit"
    ;;
  both)
    openocd -f interface/stlink.cfg -f target/stm32wbx.cfg \
            -c "program ${BOOTLOADER_ELF} verify" \
            -c "program ${APP_ELF} verify reset exit"
    ;;
  *)
    echo "Unknown target '$TARGET' (expected 'bootloader', 'app', or 'both')" >&2
    exit 1
    ;;
esac


# ELF=build/bootloader/Bootloader.elf

# openocd -f interface/stlink.cfg -f target/stm32wbx.cfg \
#         -c "program ${ELF}  verify reset exit"


# ELF=build/app/STM32wb55_Bootloader.elf

# openocd -f interface/stlink.cfg -f target/stm32wbx.cfg \
#         -c "program ${ELF}  verify reset exit"
