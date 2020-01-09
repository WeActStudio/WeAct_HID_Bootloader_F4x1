#include "hid_bootloader.h"
#include "usb_device.h"
#include "usbd_customhid.h"
#include "rtc.h"
#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

typedef void (*pFunction)(void);
uint8_t USB_RX_Buffer[CUSTOM_HID_EPOUT_SIZE];
uint8_t USB_TX_Buffer[CUSTOM_HID_EPIN_SIZE]; //USB data -> PC
uint8_t new_data_is_received = 0;
uint32_t updateflag; // 后备存储器
uint16_t erase_page = 1;

static uint8_t CMD_SIGNATURE[6] = {'W','e','A','c','t',':'}; // "WeAct: "
/* Command: <Send next data pack> */
// static uint8_t CMD_DATA_RECEIVED[7] = {'W','e','A','c','t',':',2};// "WeAct: <cmd>"

#define FW_Version "V1.1"
#define Flash_Size (0x1FFF7A22UL)

#define CMD_ResetPage (0x00)
#define CMD_Reboot 		(0x01)
#define CMD_FW_Ver 		(0x02)
#define CMD_Ack    		(0x03)
#define CMD_Erase  		(0x04)

static uint8_t pageData[SECTOR_SIZE];

static void write_flash_sector(uint32_t currentPage);
static uint32_t erase_app_flash(void);
static uint8_t bootloader_init(void);
void hid_bootloader_GotoApp(void);

uint16_t mcuGetFlashSize(void)
{
   return (*(__IO uint16_t*)(Flash_Size));
}

void hid_bootloader_Init(void)
{
	//HAL_GPIO_WritePin(C13_GPIO_Port,C13_Pin,GPIO_PIN_RESET);
	// 如果是首次上电 则初始化 往0x8003FF4写96位ID
	// 同时不运行代码
	if(bootloader_init() != 0)
	{
		while(1)
		{
			HAL_GPIO_TogglePin(C13_GPIO_Port,C13_Pin);
			HAL_Delay(200);
		}
	}
	
	updateflag = LL_RTC_BAK_GetRegister(RTC,LL_RTC_BKP_DR19);
	
	if((HAL_GPIO_ReadPin(KEY_GPIO_Port,KEY_Pin) != BOOT_ENABLED) && (updateflag !=0x1234))
	{
		//hid_bootloader_GotoApp();
		hid_bootloader_Jump(USBD_HID_APP_DEFAULT_ADD);
	}

//	/* Enable Power Clock */
//  __HAL_RCC_PWR_CLK_ENABLE();
//  /* Allow access to Backup domain */
//  LL_PWR_EnableBkUpAccess();  
//  
  	
//  
//  /* Forbid access to Backup domain */
//  LL_PWR_DisableBkUpAccess();
//	__HAL_RCC_PWR_CLK_DISABLE();
}

void hid_bootloader_Jump(uint32_t addr)
{
	if(((*(__IO uint32_t*)addr) & 0x2FF80000 ) == 0x20000000)
	{
		pFunction JumpToApplication;
		uint32_t JumpAddress;
		
		/* Jump to user application */
		JumpAddress = *(__IO uint32_t*) (addr + 4);
		JumpToApplication = (pFunction) JumpAddress;

		/* Initialize user application's Stack Pointer */
		__set_MSP(*(__IO uint32_t*) addr);
		JumpToApplication();			
	}
}

void hid_bootloader_Run(void)
{
	static volatile uint32_t current_Page = (USER_CODE_OFFSET / 1024);
  static volatile uint16_t currentPageOffset = 0;
	
	if (new_data_is_received == 1) {
		new_data_is_received = 0;
		if (memcmp(USB_RX_Buffer, CMD_SIGNATURE, sizeof (CMD_SIGNATURE)) == 0) 
		{
			switch(USB_RX_Buffer[6])
			{
				case CMD_ResetPage:

				/*------------ Reset pages */
				current_Page = 16;
				currentPageOffset = 0;
				erase_page = 1;
				break;

				case CMD_Reboot:

				/*------------- Reset MCU */
//				if (currentPageOffset > 0) {

//				/* There are incoming
//					 data that are less
//					 than sector size
//					 (16384) */
//					write_flash_sector(current_Page);
//				}
				HAL_Delay(10);
				USBD_DeInit(&hUsbDeviceFS);
				HAL_Delay(10);
				HAL_NVIC_SystemReset();
				break;
				
				case CMD_FW_Ver:
					memcpy(USB_TX_Buffer,CMD_SIGNATURE,sizeof(CMD_SIGNATURE));
				  sprintf((char *)&USB_TX_Buffer[sizeof(CMD_SIGNATURE)],"%s 0x%X ROM: %dKB",FW_Version,HAL_GetDEVID(),mcuGetFlashSize());
					USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, USB_TX_Buffer, sizeof(USB_TX_Buffer));
				break;
				case CMD_Erase:
					memcpy(USB_TX_Buffer,CMD_SIGNATURE,sizeof(CMD_SIGNATURE));
					sprintf((char *)&USB_TX_Buffer[sizeof(CMD_SIGNATURE)],"%dKB",erase_app_flash());
					USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, USB_TX_Buffer, sizeof(USB_TX_Buffer));
				break;
			}
		}
		else 
		{
			memcpy(pageData + currentPageOffset, USB_RX_Buffer, HID_RX_SIZE);
			currentPageOffset += HID_RX_SIZE;
			if (currentPageOffset == SECTOR_SIZE) 
			{
				write_flash_sector(current_Page);
				current_Page++;
				currentPageOffset = 0;
				
				memset(USB_TX_Buffer, 0, sizeof(USB_TX_Buffer)); 
				USB_TX_Buffer[sprintf((char *)&USB_TX_Buffer,"WeAct:")] = CMD_Ack;
				//ST usb库存在异常
				USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, USB_TX_Buffer, sizeof(USB_TX_Buffer)); // 使用AC6编译正常 AC5进硬件错误
			}
		}
	}
}

static void write_flash_sector(uint32_t currentPage) 
{
  uint32_t pageAddress = FLASH_BASE + (currentPage * SECTOR_SIZE);
  uint32_t SectorError;

  HAL_GPIO_WritePin(C13_GPIO_Port, C13_Pin, GPIO_PIN_SET);	
  FLASH_EraseInitTypeDef EraseInit;
  HAL_FLASH_Unlock();
	
  /* Sector to the erase the flash memory (16, 32, 48 ... kbytes) */
  if ((currentPage == 16) || (currentPage == 32) ||
      (currentPage == 48) || (currentPage == 64) ||
      (currentPage % 128 == 0)) {
    EraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInit.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
    EraseInit.Banks = FLASH_BANK_1; 
    /* Specify sector number. Starts from 0x08004000 */
    EraseInit.Sector = erase_page++;
    /* This is also important! */
    EraseInit.NbSectors = 1;
    HAL_FLASHEx_Erase(&EraseInit, &SectorError);
  }

  uint32_t dat;
  for (int i = 0; i < SECTOR_SIZE; i += 4) {
    dat = pageData[i+3];
    dat <<= 8;
    dat += pageData[i+2];
    dat <<= 8;
    dat += pageData[i+1];
    dat <<= 8;
    dat += pageData[i];
		//if(dat != 0xFFFFFFFF)
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, pageAddress + i, dat);
  }
  HAL_GPIO_WritePin(C13_GPIO_Port, C13_Pin, GPIO_PIN_RESET);  
  HAL_FLASH_Lock();
}


static uint8_t bootloader_init(void)
{
	uint32_t data;
	uint8_t result = 0;
	
	data = (USBD_HID_APP_DEFAULT_ADD - 12);
	HAL_FLASH_Unlock();
	for(uint8_t i = 0;i<12;i+=4)
	{
		if(*(uint32_t *)(data+i) != *(uint32_t *)(UID_BASE+i))
		{
			if(*(uint32_t *)(data+i) == 0xFFFFFFFF)
				HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,(data+i),*(uint32_t *)(UID_BASE+i));
			result++;
		}
	}
	HAL_FLASH_Lock();
	return result;
}

// return	擦除扇区大小 单位 KB
static uint32_t erase_app_flash(void) 
{
	uint32_t *data;
	FLASH_EraseInitTypeDef EraseInit;
	uint32_t SectorError;
	uint16_t erase_app_page = 16;
	uint16_t erase_app_sector = 1;
	uint16_t flash_size;
	
	data = (uint32_t *) USBD_HID_APP_DEFAULT_ADD;
	
	EraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	EraseInit.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
	EraseInit.Banks = FLASH_BANK_1; 
	/* Specify sector number. Starts from 0x08004000 */
	EraseInit.Sector = erase_app_sector;
	/* This is also important! */
	EraseInit.NbSectors = 1;
	
	flash_size = mcuGetFlashSize();
	
	HAL_FLASH_Unlock();

	/* Starts from 0x08004000 */
	for(erase_app_page=16;erase_app_page<flash_size;)
	{
		if((*data != 0xFFFFFFFF))
		{
			
			HAL_FLASHEx_Erase(&EraseInit, &SectorError);
			
			if(erase_app_page < 64)
				erase_app_page += 16;
			else if(erase_app_page == 64)
				erase_app_page += 64;
			else
				erase_app_page += 128;
			
			data = (uint32_t *)(erase_app_page * 1024 + FLASH_BASE);
			erase_app_sector++;
			EraseInit.Sector = erase_app_sector;
		}
		else
		{
			break;
		}
	}
	
	HAL_FLASH_Lock();
	return (erase_app_page - 16);
}