#include "bootloader.h"
#include "bootloader_config.h"
#include "stm32wbxx.h"
#include <string.h>

bootloader_t bootloader;

static flash_status_t flash_wait_for_last_operation(void)
{
  while (FLASH->SR & FLASH_SR_BSY)
  {
    /* bounded by page-erase/program time in practice */
  }

  uint32_t errors = FLASH->SR & (FLASH_SR_PROGERR | FLASH_SR_WRPERR |
                                 FLASH_SR_PGAERR | FLASH_SR_SIZERR |
                                 FLASH_SR_PGSERR);
  if (errors)
  {
    FLASH->SR = errors; /* W1C: clear the error flags */
    return FLASH_ERROR;
  }
  return FLASH_OK;
}

flash_status_t flash_lock(void)
{
  FLASH->CR |= FLASH_CR_LOCK;
  return FLASH_OK;
}

flash_status_t flash_unlock(void)
{
  if (FLASH->CR & FLASH_CR_LOCK)
  {
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
  }
  return FLASH_OK;
}

static flash_status_t flash_erase(void)
{
  flash_status_t status = FLASH_ERROR;

  DEBUG_LED_DISABLE;

  for (uint32_t addr = APP_FLASH_START;
       addr < (APP_FLASH_START + APP_FLASH_SIZE);
       addr += FLASH_PAGE_SIZE)
  {
    if (flash_wait_for_last_operation() != FLASH_OK)
    {
      bootloader.state = BOOTLOADER_ERROR;
      return FLASH_ERROR;
    }

    flash_unlock();

    uint32_t page_number = (addr - FLASH_BASE) / FLASH_PAGE_SIZE;

    FLASH->CR &= ~FLASH_CR_PNB;
    FLASH->CR |= (page_number << FLASH_CR_PNB_Pos) & FLASH_CR_PNB;
    FLASH->CR |= FLASH_CR_PER;
    FLASH->CR |= FLASH_CR_STRT;

    status = flash_wait_for_last_operation();

    flash_lock();
  }

  DEBUG_LED_ENABLE;
  return status;
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

  uint32_t app_stack_ptr = *(volatile uint32_t *)(APP_FLASH_START + 0U);
  uint32_t app_reset_addr = *(volatile uint32_t *)(APP_FLASH_START + 4U);

  __disable_irq();

  for (uint32_t i = 0U; i < 8U; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFU; /* mask every IRQ */
    NVIC->ICPR[i] = 0xFFFFFFFFU; /* drop any pending IRQ */
  }

  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;

  SCB->VTOR = APP_FLASH_START; /* relocate vector table to the app's */
  __set_MSP(app_stack_ptr);    /* set the app's own initial stack pointer */

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
  bootloader.mode = BOOT_MODE_APPLICATION;
}

flash_status_t bootloader_start_update(void)
{
  bootloader.state = BOOTLOADER_START_UPDATE;
  bootloader.mode = BOOT_MODE_BOOTLOADER;

  return FLASH_OK;
}

flash_status_t bootloader_erase_flash(void)
{
  bootloader.state = BOOTLOADER_ERASE_FLASH;
  flash_status_t status = flash_erase();
  return status;
}

flash_status_t bootloader_stop_update(void)
{
  bootloader.state = BOOTLOADER_END_UPDATE;
  return FLASH_OK;
}

flash_status_t bootloader_update_batch(uint8_t *data, uint8_t size)
{
  // Implement the logic to update the firmware with the provided data batch
  // This function should handle writing the data to flash memory
  // Ensure proper error handling and state management
  return FLASH_OK;
};

bootloader_mode_t bootloader_get_mode(void)
{
  return bootloader.mode;
}

void bootloader_app(void)
{
  switch (bootloader.state)
  {
  case BOOTLOADER_IDLE:
    break;

  case BOOTLOADER_START_UPDATE:

    // bootloader.state = BOOTLOADER_ERASE_FLASH;
    break;

  case BOOTLOADER_ERASE_FLASH:
    // bootloader.status = flash_erase();
    break;

  case BOOTLOADER_UPDATE:

    bootloader.state = BOOTLOADER_END_UPDATE;
    break;

  case BOOTLOADER_END_UPDATE:

    bootloader.state = BOOTLOADER_IDLE;
    break;

  default:
    bootloader.state = BOOTLOADER_ERROR;
    break;
  }
  bootloader.state = BOOTLOADER_IDLE;
}