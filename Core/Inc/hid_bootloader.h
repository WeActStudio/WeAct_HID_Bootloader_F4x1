#ifndef __hid_bootloader_H
#define __hid_bootloader_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "main.h"
#include "usbd_core.h"

// <USER CODE> flash start address.
#define USER_CODE_OFFSET  0x4000UL
#define USBD_HID_APP_DEFAULT_ADD  (FLASH_BASE + USER_CODE_OFFSET)
#define System_ISP_ADD            (0x1FFF0000)
#define SECTOR_SIZE   1024
#define HID_RX_SIZE   64

#define BOOT_ENABLED  GPIO_PIN_RESET

void hid_bootloader_Init(void);
void hid_bootloader_GotoApp(void);
void hid_bootloader_Jump(uint32_t addr);
void hid_bootloader_Run(void);
#ifdef __cplusplus
}
#endif

#endif

