#include "bootloader.h"
#include <string.h>

bootloader_t bootloader;

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