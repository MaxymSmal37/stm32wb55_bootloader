/*
******************************************************************************
**
**  File        : LinkerScript.ld
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32WB55xG Device
**                      1024Kbytes FLASH
**                      256Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2024 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* end of RAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;  /* required amount of heap  */
_Min_Stack_Size = 0x400; /* required amount of stack */

/* Specify the memory areas */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
FLASH (rx)       : ORIGIN = 0x08000000, LENGTH = 32K
RAM (xrw)        : ORIGIN = 0x20000008, LENGTH = 0x2FFF8
RAM_SHARED (xrw) : ORIGIN = 0x20030000, LENGTH = 10K
}

SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } >FLASH
  
  .text       : 
  { 
    *(.text*) 
    KEEP (*(.init))
    KEEP (*(.fini))
  } >FLASH
  
  .rodata     : { *(.rodata*) } >FLASH

  /* Додано відсутні секції ініціалізації, які очікує startup-файл */
  .ARM.extab (READONLY) :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH
  
  .ARM (READONLY) :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array (READONLY) :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  
  .init_array (READONLY) :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  
  .fini_array (READONLY) :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* Секція ініціалізованих даних (Data) */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* Початок секції data в RAM */
    *(.data)           
    *(.data*)          
    . = ALIGN(4);
    _edata = .;        /* Кінець секції data в RAM */
  } >RAM AT> FLASH

  /* Звідки копіювати ініціалізовані дані у Flash */
  _sidata = LOADADDR(.data);

  /* Секція неініціалізованих даних (BSS) */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;         /* Початок BSS у RAM */
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         /* Кінець BSS у RAM */
  } >RAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0       : { *(.ARM.attributes) }
   MAPPING_TABLE (NOLOAD) : { *(MAPPING_TABLE) } >RAM_SHARED
   MB_MEM1 (NOLOAD)       : { *(MB_MEM1) } >RAM_SHARED

   /* used by the startup to initialize .MB_MEM2 data */
  _siMB_MEM2 = LOADADDR(.MB_MEM2);
  .MB_MEM2 :
  {
    _sMB_MEM2 = . ;
    *(MB_MEM2) ;
    _eMB_MEM2 = . ;
  } >RAM_SHARED AT> FLASH
}