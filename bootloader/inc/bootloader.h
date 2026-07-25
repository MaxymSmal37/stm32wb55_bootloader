//

#include "stdint.h"

typedef enum
{
  BOOTLOADER_IDLE = 0,
  BOOTLOADER_START_UPDATE,
  BOOTLOADER_ERASE_FLASH,
  BOOTLOADER_UPDATE,
  BOOTLOADER_END_UPDATE,
  BOOTLOADER_ERROR,
} bootloader_state_t;

typedef struct
{
  bootloader_state_t state;
} bootloader_t;

void bootloader_init(void);
void bootloader_app(void);
uint8_t bootloader_start_update(void);
uint8_t bootloader_update_batch(uint8_t *data, uint8_t size);
uint8_t bootloader_stop_update(void);
void bootloader_jump_to_application(void);
