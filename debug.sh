#!/bin/bash

openocd -f interface/stlink.cfg -f target/stm32wbx.cfg
arm-none-eabi-gdb build/bootloader/Bootloader.elf