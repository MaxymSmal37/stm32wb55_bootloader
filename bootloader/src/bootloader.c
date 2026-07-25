#include "bootloader.h"
#include <string.h>
#include "stm32wbxx.h"

#define FLASH_PAGE_SIZE 2048U
#define FLASH_WRITE_CHUNK 8U
#define APP_FLASH_START 0x08008000UL
#define APP_FLASH_SIZE (480U * 1024U)


bootloader_t bootloader;

static void flash_erase(void)
{
  // Implement the logic to erase the flash memory
  // This function should handle erasing the necessary sectors/pages
  // Ensure proper error handling and state management
}

static void flash_wait_for_last_operation(void)
{
  // Implement the logic to wait for the last flash operation to complete
  // This function should check the status of the flash memory and wait until it is ready for the next operation
  // Ensure proper error handling and state management
}

static void flash_write(uint32_t address, uint8_t *data, uint32_t size)
{
  // Implement the logic to write data to flash memory
  // This function should handle writing the provided data to the specified address in flash memory
  // Ensure proper error handling and state management
}

void bootloader_jump_to_application(void)
{
  typedef void (*pFunction)(void);

  uint32_t app_stack_ptr  = *(volatile uint32_t *)(APP_FLASH_START + 0U);
  uint32_t app_reset_addr = *(volatile uint32_t *)(APP_FLASH_START + 4U);

  __disable_irq();

  for (uint32_t i = 0U; i < 8U; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFU; /* mask every IRQ */
    NVIC->ICPR[i] = 0xFFFFFFFFU; /* drop any pending IRQ */
  }

  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL  = 0U;

  SCB->VTOR = APP_FLASH_START;   /* relocate vector table to the app's */
  __set_MSP(app_stack_ptr);      /* set the app's own initial stack pointer */

  __DSB();
  __ISB();

  __enable_irq();

  pFunction app_entry = (pFunction)app_reset_addr;
  app_entry();

  while (1)
  {
    /* only reached if the jump itself somehow returned */
  }
}



void bootloader_init(void)
{
  memset((void *)&bootloader, 0, sizeof(bootloader_t));
  bootloader.state = BOOTLOADER_IDLE;
}

uint8_t bootloader_start_update(void)
{
  bootloader.state = BOOTLOADER_START_UPDATE;
  return 0;
}

uint8_t bootloader_stop_update(void)
{
  bootloader.state = BOOTLOADER_END_UPDATE;
  return 0;
}

uint8_t bootloader_update_batch(uint8_t *data, uint8_t size)
{
  // Implement the logic to update the firmware with the provided data batch
  // This function should handle writing the data to flash memory
  // Ensure proper error handling and state management
  return 1;
};

void bootloader_app(void)
{
  switch (bootloader.state)
  {
  case BOOTLOADER_IDLE:

    //bootloader.state = BOOTLOADER_START_UPDATE;
    break;

  case BOOTLOADER_START_UPDATE:

    bootloader.state = BOOTLOADER_ERASE_FLASH;
    break;

  case BOOTLOADER_ERASE_FLASH:

    bootloader.state = BOOTLOADER_UPDATE;
    break;

  case BOOTLOADER_UPDATE:

    bootloader.state = BOOTLOADER_END_UPDATE;
    break;

  case BOOTLOADER_END_UPDATE:

    bootloader.state = BOOTLOADER_IDLE;
    break;

  case BOOTLOADER_ERROR:

    bootloader.state = BOOTLOADER_IDLE;
    break;

  default:
    bootloader.state = BOOTLOADER_ERROR;
    break;
  }
}