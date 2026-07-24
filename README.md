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

### Toolchain location

`cmake/arm-none-eabi-gcc.cmake` defaults to
`/Applications/ArmGNUToolchain/14.3.rel1/arm-none-eabi/bin`. If the toolchain is
on your `PATH` it is found automatically; otherwise override the path:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
      -DTOOLCHAIN_PATH=/your/arm-gnu-toolchain/bin
```

## Layout

```
.
├── CMakeLists.txt                 # top-level: toolchain default, MCU flags, defines
├── cmake/
│   └── arm-none-eabi-gcc.cmake    # cross-compile toolchain file
├── build.sh                       # clean configure + build
├── STM32WB55CGUX_FLASH.ld         # linker script — run from FLASH (used by the build)
├── STM32WB55CGUX_RAM.ld           # linker script — run from RAM (alternative, unused)
├── app/
│   ├── CMakeLists.txt             # the executable: sources, includes, linker script, hex/bin
│   ├── inc/                       # application headers
│   ├── src/
│   │   ├── main.c                 # LED blink entry point
│   │   └── system_stm32wbxx.c     # minimal SystemInit() + SystemCoreClock
│   └── Startup/
│       └── startup_stm32wb55cgux.s  # vector table + Reset_Handler
├── CMSIS/                         # ARM CMSIS core + ST device headers (see below)
└── Logger/                        # logging helper (see below)
```

## Notes on the extra files

These are present in the tree but are **not all part of the LED-blink build** —
they are dependencies and scaffolding for the eventual bootloader:

- **`CMSIS/`** — Vendor-provided headers, only the include paths are used:
  - `CMSIS/Include/` — ARM core headers (`core_cm4.h`, intrinsics like `__NOP`).
  - `CMSIS/Device/ST/STM32WBxx/Include/` — ST register definitions. `main.c`
    includes `stm32wbxx.h`, which selects `stm32wb55xx.h` because the build
    defines `STM32WB55xx` (see top-level `CMakeLists.txt`). There is **no**
    `system_stm32wbxx.c` shipped with these headers, so the project provides its
    own minimal one under `app/src/`.

- **`app/Startup/startup_stm32wb55cgux.s`** — CMSIS startup. Defines the vector
  table (`.isr_vector`, linked to `0x08000000`) and `Reset_Handler`, which sets
  the stack, copies `.data`, zeroes `.bss`, runs C++ static constructors via
  `__libc_init_array`, then calls `main()`. It references `SystemInit`, supplied
  by `app/src/system_stm32wbxx.c`.

- **Linker scripts** — `STM32WB55CGUX_FLASH.ld` is the one wired into the build
  (`app/CMakeLists.txt`). It places code in FLASH (512 KB @ `0x08000000`),
  data/bss in RAM (`0x20000008`), and reserves `RAM_SHARED` for the wireless
  co-processor mailbox. `STM32WB55CGUX_RAM.ld` is a run-from-RAM variant kept
  for reference; switch `LINKER_SCRIPT` in `app/CMakeLists.txt` to use it.

- **`Logger/`** — A small leveled logger (`logMessage`, `DEBUG_LOG`/`INFO_LOG`/…).
  It is **not compiled** into the current blink (it depends on `printf`/RTC
  output that bare-metal blink does not wire up). To use it later, add
  `Logger/Logger.c` to the `add_executable` list and `Logger/` to the include
  dirs in `app/CMakeLists.txt`, and provide a `_write` syscall for output.

## LED pin

`app/src/main.c` blinks **PE4** — the user LED on the WeAct STM32WB55 Core Board
(active high). Change `LED_PORT` / `LED_PIN` at the top of `main.c` for
different hardware.
# stm32wb55_bootloader
