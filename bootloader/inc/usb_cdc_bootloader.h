#ifndef USB_CDC_BOOTLOADER_H_
#define USB_CDC_BOOTLOADER_H_

#include <stdint.h>

void usb_cdc_bootloader_init(void);
void usb_cdc_bootloader_task(void);
void usb_cdc_bootloader_send(const uint8_t *data, uint32_t size);

#endif
