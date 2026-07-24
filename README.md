# STM32WB55 Bootloader

Bare-metal firmware project for the **STM32WB55CGUx** (Cortex-M4) built with
CMake and the GNU Arm Embedded toolchain (`arm-none-eabi-gcc`). The current
application is a minimal LED blink used to bring up the build.

## Building

```bash
./build.sh
```

This wipes `build/`, configures with the bundled toolchain file, and compiles.
Equivalently:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
cmake --build build
```

Build outputs land in `build/app/`:

| File                          | Purpose                                  |
| ----------------------------- | ---------------------------------------- |
| `STM32wb55_Bootloader.elf`    | Full image with symbols (for debugging)  |
| `STM32wb55_Bootloader.hex`    | Intel HEX, for flashing                  |
| `STM32wb55_Bootloader.bin`    | Raw binary, for flashing                 |
| `STM32wb55_Bootloader.map`    | Linker map (symbol/section placement)    |

The configure step also prints a memory-usage summary (FLASH / RAM / RAM_SHARED).

