#include "FreeRTOS.h"
#include "main.h"
#include "cmsis_os2.h"
#include "projdefs.h"
#include "stm32f4xx_hal_uart.h"
#include "task.h"
#include "usart.h"
#include "LED.h"
#include "SD.h"
#include "RS485.h"
#include "stm32f407xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include <stdint.h>

TaskHandle_t LED_Handle;
uint8_t a[2] = {0xA5,0x55};
void LED(void *arg)
{
  
    while(1)
    {
    (void)arg;
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,GPIO_PIN_SET);
    osDelay(pdMS_TO_TICKS(1000));
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,GPIO_PIN_RESET);
    osDelay(pdMS_TO_TICKS(1000));

    }
    

}
