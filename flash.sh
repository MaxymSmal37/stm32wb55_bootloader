#!/bin/bash
set -e

# Build, then flash the STM32WB55 over ST-Link with OpenOCD.

./build.sh

ELF=build/app/STM32wb55_Bootloader.elf

openocd -f interface/stlink.cfg -f target/stm32wbx.cfg \
        -c "program ${ELF} verify reset exit"
