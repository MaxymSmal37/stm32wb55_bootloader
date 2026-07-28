#include "bootloader.h"
#include "bootloader_config.h"
#include "communication_protocol.h"
#include "stm32wbxx.h"
#include "tusb.h"
#include <stdbool.h>
#include <string.h>

static const char usb_cdc_banner[] = "STM32WB55 Bootloader CDC ready\r\n";

#define USB_CDC_TX_BUFFER_SIZE 128U

static uint8_t usb_cdc_tx_buffer[USB_CDC_TX_BUFFER_SIZE];
static volatile uint32_t usb_cdc_tx_len = 0U;
static volatile uint32_t usb_cdc_rx_len = 0U;

static volatile bool usb_cdc_ready = false;  
static volatile bool usb_cdc_dtr   = false;  

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCAFE, 
    .idProduct          = 0x4000, 
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

// --- Configuration Descriptor ---
// (Ensure your actual configuration byte array `desc_configuration` is defined elsewhere)
extern uint8_t const desc_configuration[];

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

// --- String Descriptors ---
static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: Language is English (0x0409)
    "MyManufacturer",           // 1: Manufacturer
    "STM32WB55 Bootloader",     // 2: Product
    "123456",                   // 3: Serial
};

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_CDC_DESCRIPTOR(0, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

static uint16_t _desc_str[32];

static void usb_cdc_init_clock(void)
{
   RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN | RCC_APB1ENR1_USBEN;
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

  // PWR->CR2 доступний без окремого clock-enable на STM32WB55
  PWR->CR2 |= PWR_CR2_USV;

  RCC->CRRCR |= RCC_CRRCR_HSI48ON;
  while (!(RCC->CRRCR & RCC_CRRCR_HSI48RDY))
    ;

  //RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_CLK48SEL) | RCC_CCIPR_CLK48SEL_1;

  /* Enable the USB pull-up on PA12 as required by the STM32WB55 FS device. */
  GPIOA->MODER &= ~(3U << (12U * 2U));
  GPIOA->MODER |= (2U << (12U * 2U));
  GPIOA->OTYPER &= ~(1U << 12U);
  GPIOA->PUPDR &= ~(3U << (12U * 2U));
  GPIOA->AFR[1] &= ~(0xFU << ((12U - 8U) * 4U));
  GPIOA->AFR[1] |= (10U << ((12U - 8U) * 4U));

  GPIOA->MODER &= ~(3U << (11U * 2U));
  GPIOA->MODER |= (2U << (11U * 2U));
  GPIOA->OTYPER &= ~(1U << 11U);
  GPIOA->PUPDR &= ~(3U << (11U * 2U));
  GPIOA->AFR[1] &= ~(0xFU << ((11U - 8U) * 4U));
  GPIOA->AFR[1] |= (10U << ((11U - 8U) * 4U));
}

void usb_cdc_bootloader_init(void)
{
  usb_cdc_init_clock();

  NVIC_SetPriority(USB_HP_IRQn, 2);
  NVIC_SetPriority(USB_LP_IRQn, 2);
  NVIC_EnableIRQ(USB_HP_IRQn);
  NVIC_EnableIRQ(USB_LP_IRQn);

  tusb_rhport_init_t init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(0, &init);

  usb_cdc_ready = true;
  
}

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;
        char const *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}

void usb_cdc_bootloader_task(void)
{
  tud_task();  

  if (!usb_cdc_ready)
  {
    return;   
  }

  if (tud_cdc_connected() && tud_cdc_available())
  {
    uint8_t rx_buf[64];
    uint32_t count = tud_cdc_read(rx_buf, sizeof(rx_buf));
    if (count > 0)
    {
      usb_cdc_rx_len = count;
      for (uint32_t i = 0; i < count; ++i)
      {
        communication_handle_request(rx_buf[i]);
      }
    }
  }
}

void usb_cdc_bootloader_send(const uint8_t *data, uint32_t size)
{
  if (!usb_cdc_ready || !tud_cdc_connected() || !usb_cdc_dtr)
  {
    return;
  }

  if (size > sizeof(usb_cdc_tx_buffer))
  {
    size = sizeof(usb_cdc_tx_buffer);
  }

  memcpy((void *)usb_cdc_tx_buffer, data, size);
  usb_cdc_tx_len = size;
  tud_cdc_write(usb_cdc_tx_buffer, usb_cdc_tx_len);
  tud_cdc_write_flush();
}

void tud_mount_cb(void)
{
  usb_cdc_ready = true;
  usb_cdc_bootloader_send((const uint8_t *)usb_cdc_banner, sizeof(usb_cdc_banner) - 1U);
}

void tud_umount_cb(void)
{
  usb_cdc_ready = false;
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void)itf;
  (void)rts;
  usb_cdc_dtr = dtr;
}

void tud_cdc_rx_cb(uint8_t itf)
{
  (void)itf;
}

void USB_HP_IRQHandler(void)
{
  tud_int_handler(0);
}

void USB_LP_IRQHandler(void)
{
  tud_int_handler(0);
}
