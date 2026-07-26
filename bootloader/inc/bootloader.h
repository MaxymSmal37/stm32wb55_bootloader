//

#include "stdint.h"

typedef enum
{
  BOOTLOADER_IDLE = 0,
  BOOTLOADER_START_UPDATE,
  BOOTLOADER_ERASE_FLASH,
  BOOTLOADER_UPDATE,
  BOOTLOADER_END_UPDATE,
} bootloader_state_t;

typedef enum
{
  FLASH_OK = 0,
  FLASH_ERROR,
  FLASH_BUSY,
  FLASH_TIMEOUT,
} flash_status_t;

typedef enum
{
  BOOTLOADER_OK = 0,
  BOOTLOADER_ERROR,
} bootloader_status_t;

typedef enum
{
  BOOT_MODE_BOOTLOADER = 0,
  BOOT_MODE_APPLICATION,
} bootloader_mode_t;

typedef struct
{
  bootloader_state_t state;
  bootloader_mode_t mode;
  bootloader_status_t status;
} bootloader_t;

void bootloader_init(void);
void bootloader_app(void);
void bootloader_jump_to_application(void);
bootloader_mode_t bootloader_get_mode(void);
flash_status_t bootloader_start_update(void);
flash_status_t bootloader_erase_flash(void);
flash_status_t bootloader_update_batch(uint8_t *data, uint8_t size);
flash_status_t bootloader_stop_update(void);
