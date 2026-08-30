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

    FLASH->CR &= ~FLASH_CR_PER; 

    flash_lock();
  }

  DEBUG_LED_ENABLE;
  return status;
}

static void flash_write(uint32_t address, uint8_t *data, uint32_t size)
{
  if (flash_wait_for_last_operation() != FLASH_OK)
  {
    bootloader.state = BOOTLOADER_ERROR;
    return;
  }

  flash_unlock();

  FLASH->CR |= FLASH_CR_PG;

  for (uint32_t i = 0; i < size; i += FLASH_WRITE_CHUNK)
  {
    uint64_t data_chunk = 0xFFFFFFFFFFFFFFFF; 
    
    uint32_t copy_size = (size - i) < FLASH_WRITE_CHUNK ? (size - i) : FLASH_WRITE_CHUNK;
    memcpy(&data_chunk, &data[i], copy_size);

    *(volatile uint64_t *)(address + i) = data_chunk;

    if (flash_wait_for_last_operation() != FLASH_OK)
    {
      bootloader.state = BOOTLOADER_ERROR;
      FLASH->CR &= ~FLASH_CR_PG; 
      flash_lock();
      return;
    }
  }

  FLASH->CR &= ~FLASH_CR_PG;

  flash_lock();
}

flash_status_t bootloader_update_batch(uint8_t *data, uint8_t size)
{
  static uint32_t current_address = APP_FLASH_START;

  if (current_address + size > APP_FLASH_END)
  {
    bootloader.state = BOOTLOADER_ERROR;
    return FLASH_ERROR;
  }

  flash_write(current_address, data, size);
  current_address += size;

  if (current_address >= APP_FLASH_END)
  {
    bootloader.state = BOOTLOADER_END_UPDATE;
  }
  return FLASH_OK;
}

void bootloader_jump_to_application(void)
{
  typedef void (*pFunction)(void);

  uint32_t app_stack_ptr = *(volatile uint32_t *)(APP_FLASH_START + 0U);
  uint32_t app_reset_addr = *(volatile uint32_t *)(APP_FLASH_START + 4U);

  system_deinit();

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

  if (status == FLASH_OK)
  {
    bootloader.state = BOOTLOADER_UPDATE;
  }
  else
  {
    bootloader.state = BOOTLOADER_ERROR;
  }

  return status;
}

flash_status_t bootloader_stop_update(void)
{
  bootloader.state = BOOTLOADER_END_UPDATE;
  return FLASH_OK;
}

bootloader_mode_t bootloader_get_mode(void)
{
  return bootloader.mode;
}

bootloader_state_t bootloader_get_state(void)
{
  return bootloader.state;
}
