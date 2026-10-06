#include "ADC_DATA.h"
#include "main.h"
#include "FreeRTOS.h"
#include "CLI.h"
#include "portmacro.h"
#include "stm32f407xx.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include "RTC_Time.h"
#include "projdefs.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_uart.h"
#include "usart.h"
#include "Uart_Tx.h"
#include <stdint.h>
#include <sys/_intsup.h>
#include "Stream_Control.h"

static uint8_t Cli_Rx_Byte;
static char Cli_Buffer[CLI_LINE_MAX];
static uint8_t Cli_Index;
static volatile uint8_t Cli_Line_Ready = 0;
extern volatile Motor_Data_t ADC_Val;
QueueHandle_t CLI_DataHandle;
CLI_Resp_t Resp_Data;

extern xQueueHandle Uart_TxHandle;
extern TaskHandle_t RS485_TaskHandle;

extern uint8_t RS485_ReceiveData[32];

static const Fault_Text_t Fault_table[] = {
    {ADC_FAULT_OVERCURRENT,       "OVERCURRENT"},
    {ADC_FAULT_OVERTEMP,          "OVERTEMP"},
    {ADC_FAULT_UNDERVOLT,         "UNDERVOLT"},
    {ADC_FAULT_MOTOR_ABNORMAL,    "MOTOR_ABNORMAL"},
    {ADC_FAULT_MOTOR_SPEED_STALL, "MOTOR_STALL"},
};


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    
  if (huart->Instance == USART2) {
      if (Cli_Line_Ready == 0U) {
          if ((Cli_Rx_Byte == '\r') || (Cli_Rx_Byte == '\n')) {
              if (Cli_Index > 0U) {
                  Cli_Buffer[Cli_Index] = '\0';
                  Cli_Line_Ready = 1U;
              }
          } else if (Cli_Index < (CLI_LINE_MAX - 1U)) {
              Cli_Buffer[Cli_Index++] = (char)Cli_Rx_Byte;
          } else {
              Cli_Index = 0U;
          }
      }
      (void)HAL_UART_Receive_IT(&huart2, &Cli_Rx_Byte, 1);
    }
  
}


HAL_StatusTypeDef CLI_Init(void)
{
    CLI_DataHandle = xQueueCreate(1, sizeof(Motor_Data_t));
    Cli_Index = 0U;
    Cli_Line_Ready = 0U;
    Resp_Data.Help_BufferLen = snprintf((char *)Resp_Data.Help_buffer, sizeof(Resp_Data.Help_buffer),  
    "CLI_CMD_HELP\r\n"
    "CLI_CMD_DATA\r\n"
    "CLI_CMD_RTC\r\n"
    "CLI_CMD_FAULT\r\n"
    "CLI_CMD_STATUS\r\n"
    "CLI_CMD_STREAM_ON\r\n"
    "CLI_CMD_STREAM_OFF\r\n"
    "CLI_CMD_UNKNOW\r\n");
    
    if (HAL_UART_Receive_IT(&huart2,  &Cli_Rx_Byte, 1) != HAL_OK) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

void CLI_Task(void *arg)
{
    (void)arg;
    UartTx_Frame_s Tx_Data;

    while (1) {
        if (Cli_Line_Ready != 0U) {
            Cli_Line_Ready = 0;
            if (strcmp(Cli_Buffer, "CLI_CMD_HELP") == 0) {
                memset(Cli_Buffer, 0, sizeof(Cli_Buffer));
                Cli_Index = 0;
                memcpy(Tx_Data.Tx_buffer, Resp_Data.Help_buffer, sizeof(Resp_Data.Help_buffer));
                Tx_Data.len = Resp_Data.Help_BufferLen;
                xQueueSend(Uart_TxHandle, &Tx_Data, portMAX_DELAY);

            }else if (strcmp(Cli_Buffer, "CLI_CMD_DATA") == 0) {
                memset(Cli_Buffer, 0, sizeof(Cli_Buffer));
                Cli_Index = 0;
                Motor_Data_t CLI_Data;
                BaseType_t Data_ready = pdFALSE;
                if (CLI_DataHandle != NULL) {
                    Data_ready = xQueuePeek(CLI_DataHandle, &CLI_Data, 0);
                }
                if (Data_ready == pdPASS) {
                    Tx_Data.len = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer),"DATA,%lu,%04u-%02u-%02uT%02u:%02u:%02u,%u,%.2f,%.2f,%.2f,0x%02X\r\n",
                                   (unsigned long)CLI_Data.sequence,
                                   (unsigned int)CLI_Data.DataTime.year,
                                   (unsigned int)CLI_Data.DataTime.month,
                                   (unsigned int)CLI_Data.DataTime.day,
                                   (unsigned int)CLI_Data.DataTime.hours,
                                   (unsigned int)CLI_Data.DataTime.minutes,
                                   (unsigned int)CLI_Data.DataTime.seconds,
                                   (unsigned int)CLI_Data.Motor_rpm,
                                   CLI_Data.Curr_val,
                                   CLI_Data.Temp_val,
                                   CLI_Data.Volt_val,
                                   (unsigned int)CLI_Data.fault_code);
                }
                if ((Tx_Data.len > 0) && (Tx_Data.len < (int)sizeof(Tx_Data.Tx_buffer))) {
                    xQueueSend(Uart_TxHandle, &Tx_Data, portMAX_DELAY);
                }


            }else if (strcmp(Cli_Buffer, "CLI_CMD_RTC") == 0) {
                Cli_Index = 0;
                RTC_DataTime_t datatime;
                RTC_ReadTimeData(&datatime);
                memset(Cli_Buffer, 0, sizeof(Cli_Buffer));
                Tx_Data.len = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer), "Time ,%04u-%02u-%02uT%02u:%02u:%02u\r\n",
                                        datatime.year,
                                        datatime.month,
                                        datatime.day,
                                        datatime.hours,
                                        datatime.minutes,
                                        datatime.seconds);
                xQueueSend(Uart_TxHandle, &Tx_Data, portMAX_DELAY);


            }else if (strcmp(Cli_Buffer, "CLI_CMD_FAULT") == 0) {
                uint8_t First = 1; 
                Cli_Index = 0;
                uint8_t used = 0;
                uint8_t Fault = ADC_Val.fault_code;
                used = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer), "FAULT,0x%X,",Fault);
                if (Fault == 0) {
                    used += snprintf((char *)&Tx_Data.Tx_buffer[used], sizeof(Tx_Data.Tx_buffer) - used,"NONE");
                } else {
                    for (uint8_t i = 0; i < 5; i++) {
                        if (Fault & Fault_table[i].Mask) {
                            used += snprintf((char *)&Tx_Data.Tx_buffer[used],sizeof(Tx_Data.Tx_buffer) - used, "%s%s", First ? "" : "|",Fault_table[i].Fault_Name);
                            First = 0;
                        }
                    }
                }
                used += snprintf((char *)&Tx_Data.Tx_buffer[used], sizeof(Tx_Data.Tx_buffer) - used,"\r\n");
                Tx_Data.len = used;
                xQueueSend(Uart_TxHandle, &Tx_Data, portMAX_DELAY);


            }else if (strcmp(Cli_Buffer, "CLI_CMD_STATUS") == 0) {
                Cli_Index = 0;
                
                uint8_t Fault = ADC_Val.fault_code;
                uint16_t Sequence = ADC_Val.sequence;
                if (Stream_Control_IsEnable()) {
                Tx_Data.len = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer),"STATUS,SEQUENCE=%u,STREAM=ON,FAULT=0x%02X\r\n", Sequence,Fault);
                }else {
                Tx_Data.len = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer),"STATUS,SEQUENCE=%u,STREAM=OFF,FAULT=0x%02X\r\n", Sequence,Fault);
                }
                
                xQueueSend(Uart_TxHandle, &Tx_Data, portMAX_DELAY);


            }else if (strcmp(Cli_Buffer, "CLI_CMD_STREAM_ON") == 0) {
                Cli_Index = 0;
               Tx_Data.len = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer), "STREAM_ON\r\n");
                Stream_Control_Set(1U);
                xQueueSend(Uart_TxHandle, &Tx_Data, portMAX_DELAY);


            }else if (strcmp(Cli_Buffer, "CLI_CMD_STREAM_OFF") == 0) {
                Cli_Index = 0;
                Tx_Data.len = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer), "STREAM_OFF\r\n");
                Stream_Control_Set(0);
                xQueueSend(Uart_TxHandle, &Tx_Data, portMAX_DELAY);


            }else{//未知命令
                Cli_Index = 0;
                Tx_Data.len = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer),"ERROR\r\n");
                xQueueSend(Uart_TxHandle, &Tx_Data, portMAX_DELAY);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));

    }
}
