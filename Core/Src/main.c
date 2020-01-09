/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "rtc.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hid_bootloader.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
unsigned short key_value;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	uint32_t tick,tick1,tick2;
	uint8_t breathsw = 1;
  /* USER CODE END 1 */
  

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
	
  MX_GPIO_Init();
	HAL_Delay(1);
	hid_bootloader_Init();
	while(HAL_GPIO_ReadPin(KEY_GPIO_Port,KEY_Pin) == BOOT_ENABLED)
	{
		HAL_GPIO_TogglePin(C13_GPIO_Port,C13_Pin);
		HAL_Delay(60);
	}
	HAL_GPIO_WritePin(C13_GPIO_Port, C13_Pin, GPIO_PIN_SET); 
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_RTC_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
	LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR19, 0);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		hid_bootloader_Run();
		
		/* C13 ºôÎüµÆ²âÊÔ */
	  /* C13 Breathing Lamp test */
		static uint8_t pwmset;
		static uint16_t time;
		static uint8_t timeflag;
		static uint8_t timecount;
	  if((HAL_GetTick() - tick >= 1 ))
	  {
		   tick = HAL_GetTick();
			
		   if(breathsw == 1)
			{
				 /* ºôÎüµÆ */
				 /* Breathing Lamp */
				if(timeflag == 0)
				{
					time ++;
					if(time >= 1200) timeflag = 1;
				}
				else
				{
					time --;
					if(time <= 110) timeflag = 0;
				}

				/* Õ¼¿Õ±ÈÉèÖÃ */
				/* Duty Cycle Setting */
				pwmset = time/120;

				/* 20ms Âö¿í */
				/* 20ms Pulse Width */
				if(timecount > 20) timecount = 0;
				else timecount ++;

				if(timecount >= pwmset ) HAL_GPIO_WritePin(C13_GPIO_Port,C13_Pin,GPIO_PIN_SET);
				else HAL_GPIO_WritePin(C13_GPIO_Port,C13_Pin,GPIO_PIN_RESET);
			}
		}
		
		if(HAL_GetTick() - tick1 >= 50)
		{
			tick1 = HAL_GetTick();
			
			static GPIO_PinState KEY_last = GPIO_PIN_SET;
			static uint32_t long_press_count = 0;
			
			if(HAL_GPIO_ReadPin(KEY_GPIO_Port,KEY_Pin) == BOOT_ENABLED)
			{
				KEY_last = BOOT_ENABLED;
				
				long_press_count ++;
				if(long_press_count > 12) // 0.6s
				{
					/* long press */
					while(HAL_GPIO_ReadPin(KEY_GPIO_Port,KEY_Pin) == BOOT_ENABLED)
					{
						HAL_GPIO_TogglePin(C13_GPIO_Port,C13_Pin);
						HAL_Delay(100);
					}
					/*  Jump to Embedded bootloader */
					USBD_DeInit(&hUsbDeviceFS);
					HAL_GPIO_WritePin(C13_GPIO_Port,C13_Pin,GPIO_PIN_SET);
					HAL_Delay(10);
					HAL_DeInit();
					hid_bootloader_Jump(System_ISP_ADD);
				}
			}
			else if(HAL_GPIO_ReadPin(KEY_GPIO_Port,KEY_Pin) != KEY_last)
			{
				KEY_last = HAL_GPIO_ReadPin(KEY_GPIO_Port,KEY_Pin);
				
				/* short press */
				if(long_press_count < 8)
				{
					HAL_GPIO_WritePin(C13_GPIO_Port,C13_Pin,GPIO_PIN_SET);
					breathsw = breathsw == 0?1:0;
				}
				long_press_count = 0;
			}
		}
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
