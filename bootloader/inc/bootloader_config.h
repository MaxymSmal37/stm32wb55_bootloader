#define BOOTLOADER_START 0x08000000UL

#define APP_FLASH_START  0x08008000UL
#define APP_FLASH_SIZE   (480U * 1024U)
#define APP_FLASH_END    (APP_FLASH_START + APP_FLASH_SIZE)
 
#define FLASH_PAGE_SIZE  0x1000UL /* 4 KB pages on STM32WB55 */
 
#define FLASH_KEY1       0x45670123UL
#define FLASH_KEY2       0xCDEF89ABUL
 
#define FLASH_WRITE_CHUNK 8U /


// Debug LED macros for toggling, enabling, and disabling the LED on GPIOE pin 4
#define DEBUG_LED_TOGGLE (GPIOE->ODR ^= (1U << 4U))
#define DEBUG_LED_ENABLE (GPIOE->ODR |= (1U << 4U))
#define DEBUG_LED_DISABLE (GPIOE->ODR &= ~(1U << 4U))