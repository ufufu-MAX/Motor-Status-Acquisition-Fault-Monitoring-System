#include "cmsis_os2.h"
#include "main.h"
#include "FreeRTOS.h"
#include "Uart_Tx.h"
#include "projdefs.h"
#include "queue.h"
#include "portmacro.h"
#include "stm32f407xx.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_uart.h"
#include "usart.h"
#include "RS485.h"

xQueueHandle Uart_TxHandle;
TaskHandle_t Uart_TaskHandle;


void UartTx_Init()
{

Uart_TxHandle = xQueueCreate(6, sizeof(UartTx_Frame_s));

}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{

    if (huart->Instance == USART1) {
        RS485_SetReceive();
    }

    if (huart->Instance == USART2) {
        BaseType_t Woken = pdFALSE;
        vTaskNotifyGiveFromISR(Uart_TaskHandle, &Woken);
        portYIELD_FROM_ISR(Woken);
    }


  
}

void Uart_Task(void *arg)
{
    (void)arg;
    UartTx_Frame_s Frame;
    while (1) {
        xQueueReceive(Uart_TxHandle,&Frame,portMAX_DELAY);
        ulTaskNotifyTake(pdTRUE, 0);
        if (HAL_UART_Transmit_DMA(&huart2,Frame.Tx_buffer, Frame.len) != HAL_OK) {
            continue;
        }
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0U) {
            HAL_UART_AbortTransmit(&huart2);
        }       
    }
}
