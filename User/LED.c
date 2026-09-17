#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "task.h"
#include "main.h"
#include "LED.h"
#include "stm32f407xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"

TaskHandle_t LED_Handle;
void LED(void *arg)
{
    while(1)
    {
    (void)arg;
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,GPIO_PIN_SET);
    osDelay(500);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,GPIO_PIN_RESET);
    osDelay(500);

    }
    

}
