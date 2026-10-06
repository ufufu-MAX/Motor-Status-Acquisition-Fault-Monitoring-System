/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "queue.h"
#include "LED.h"
#include "ADC_DATA.h"
#include "RTC_Time.h"
#include "CLI.h"
#include "Uart_Tx.h"
#include "SD.h"
#include "W25Q64.h"
#include "RS485.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
extern TaskHandle_t LED_Handle;
extern TaskHandle_t ADC_ProcessingHandle;
extern TaskHandle_t ADC_AcquisitionHandle;
extern QueueHandle_t ADC_QueueHandle;
extern TaskHandle_t Uart_TaskHandle;
extern TaskHandle_t W25Q64_TaskHandle;
extern TaskHandle_t RS485_TaskHandle;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  ADC_QueueHandle = xQueueCreate(10, 4 * sizeof(uint16_t));
  if (ADC_QueueHandle == NULL)
  {
    Error_Handler();
  }

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  xTaskCreate(LED, "LED_Test", 128, NULL, osPriorityLow7, &LED_Handle);
  xTaskCreate(ADC_ProcessingTask, "ADC_ProcessingTask", 512, NULL, osPriorityNormal, &ADC_ProcessingHandle);
  xTaskCreate(ADC_AcquisitionTask, "ADC_AcquisitionTask", 256,NULL, osPriorityNormal, &ADC_AcquisitionHandle);
  xTaskCreate(CLI_Task, "CLI", 256, NULL, osPriorityNormal, NULL);
  xTaskCreate(Uart_Task, "UART", 256, NULL, osPriorityNormal,&Uart_TaskHandle);
  xTaskCreate(SD_Task, "SD", 256, NULL, osPriorityNormal,NULL);
  xTaskCreate(W25Q64_Task, "W25Q64", 256, NULL, osPriorityNormal,&W25Q64_TaskHandle);
  if(xTaskCreate(RS485_Task, "RS485", 256, NULL, osPriorityNormal,&RS485_TaskHandle) != pdPASS){
    vTaskDelete(LED_Handle);
  }
  
  
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

