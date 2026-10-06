#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "ff.h"
#include "main.h"
#include "queue.h"
#include "RS485.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32f407xx.h"
#include "task.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_uart.h"
#include "stm32f4xx_ll_sdmmc.h"
#include "usart.h"
#include <stdint.h>
#include "ADC_DATA.h"

extern volatile Motor_Data_t ADC_Val;

#define REG_SEQUENCE_HIGH   0x0000U
#define REG_SEQUENCE_LOW    0x0001U
#define REG_MOTOR_RPM       0x0002U
#define REG_TEMPERATURE     0x0003U
#define REG_VOLTAGE         0x0004U
#define REG_CURRENT         0x0005U
#define REG_FAULT_CODE      0x0006U

RS485_REC_t RS485_Receive;
volatile uint16_t len = 0;
TaskHandle_t RS485_TaskHandle;
QueueHandle_t RS485_QueueHandle;


void RS485_SetTx()
{
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8, GPIO_PIN_RESET);
}

void RS485_SetReceive()
{
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8, GPIO_PIN_SET);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  
     if (huart->Instance == USART1) {

    RS485_Receive.len = Size;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(RS485_QueueHandle, &RS485_Receive, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    (void)HAL_UARTEx_ReceiveToIdle_IT(&huart1, RS485_Receive.Data, sizeof(RS485_Receive.Data));
    }

}

void RS485_Init()
{
    RS485_QueueHandle = xQueueCreate(10, sizeof(RS485_REC_t));
    if (RS485_QueueHandle == NULL) {
        vTaskDelete(NULL);
        return;
    }
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_RESET);
    if (HAL_UARTEx_ReceiveToIdle_IT(&huart1, RS485_Receive.Data, sizeof(RS485_Receive.Data)) != HAL_OK) {
        vTaskDelete(NULL);
        return;
    }
    
}


uint16_t Readuint8_To_uint16(const uint8_t *Data)
{
    return (uint16_t)(((uint16_t)Data[0] << 8) | Data[1]);

}

void Writeuint16_To_uint8(uint8_t *Data,uint16_t Value)
{

    Data[0] = (uint8_t)(Value >> 8);
    Data[1] = (uint8_t)Value;

}

uint16_t Modbus_CRC16( const uint8_t *Data , uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0; i < len; i++) {
        crc ^= Data[i];

        for (uint8_t bit = 0; bit < 8U; bit++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xA001U;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static uint8_t RS485_ReadRegister(uint16_t Address,uint16_t *Value)
{
    switch (Address) {
    case REG_SEQUENCE_HIGH:
        *Value = (uint16_t)(ADC_Val.sequence >> 16);
        return 1U;

    case REG_SEQUENCE_LOW:
        *Value = (uint16_t)(ADC_Val.sequence & 0xFFFFU);
        return 1U;

    case REG_MOTOR_RPM:
        *Value = ADC_Val.Motor_rpm;
        return 1U;

    case REG_TEMPERATURE:
        *Value = (uint16_t)(ADC_Val.Temp_val * 100.0f);
        return 1U;

    case REG_VOLTAGE:
        *Value = (uint16_t)(ADC_Val.Volt_val * 100.0f);
        return 1U;

    case REG_CURRENT:
        *Value = (uint16_t)(ADC_Val.Curr_val * 100.0f);
        return 1U;

    case REG_FAULT_CODE:
        *Value = ADC_Val.fault_code;
        return 1U;

    default:
        return 0U;
    }

}


uint16_t Modbus_ReadInput(const RS485_REC_t Receive_Data,const uint16_t Reg_AddressStart,const uint16_t Read_RegCount,uint8_t *Tx_Data,uint8_t *len)
{
    uint16_t Data_Length;
    uint16_t CRC_Value;

    if ((Read_RegCount == 0U) ||(Reg_AddressStart > REG_FAULT_CODE) || ((uint32_t)Reg_AddressStart + Read_RegCount - 1U > REG_FAULT_CODE)) {
        return 0U;
    }

    Tx_Data[0] = RS485_SLAVE_ADDRESS;
    Tx_Data[1] = 0x04U;
    Tx_Data[2] = (uint8_t)(Read_RegCount * 2U);

    for (uint16_t i = 0U; i < Read_RegCount; i++) {
        uint16_t Address;
        uint16_t Value;

        Address = Reg_AddressStart + i;

        if (RS485_ReadRegister(Address, &Value) == 0U) {
            return 0U;
        }

        Writeuint16_To_uint8(&Tx_Data[3U + i * 2U], Value);
    }

    Data_Length = (uint16_t)(3U + Read_RegCount * 2U);

    CRC_Value = Modbus_CRC16(Tx_Data, Data_Length);

    Tx_Data[Data_Length] = (uint8_t)(CRC_Value & 0xFFU);
    Tx_Data[Data_Length + 1U] = (uint8_t)(CRC_Value >> 8);

    *len = (uint8_t)(Data_Length + 2U);

    return 1U;
}

void RS485_Task(void *arg)
{
    uint16_t Receive_CRC = 0;
    RS485_REC_t Receive_QueueValue;
    static uint8_t RS485Tx_Data[64];
    uint8_t RS485_CMD;
    uint16_t Reg_AddressStart;
    uint16_t Read_RegCount;
    uint8_t Tx_len;
    (void)arg;
    RS485_Init();
    while (1) {
       
        xQueueReceive(RS485_QueueHandle, &Receive_QueueValue, portMAX_DELAY);

        if ((Receive_QueueValue.len < 8U) ||
            (Receive_QueueValue.len > sizeof(Receive_QueueValue.Data))) {
            continue;
        }

        if (Receive_QueueValue.Data[0] != RS485_SLAVE_ADDRESS) {
            continue;
        }
        Receive_CRC = (uint16_t)(((uint16_t)Receive_QueueValue.Data[Receive_QueueValue.len - 1] << 8 ) | Receive_QueueValue.Data[Receive_QueueValue.len - 2]);
        if (Receive_CRC !=  Modbus_CRC16( Receive_QueueValue.Data,Receive_QueueValue.len - 2)) {
            continue;
        }

        RS485_CMD = Receive_QueueValue.Data[1];
        Reg_AddressStart = Readuint8_To_uint16(&Receive_QueueValue.Data[2]);     
        Read_RegCount = Readuint8_To_uint16(&Receive_QueueValue.Data[4]); 
        
        switch (RS485_CMD) {
        
            case 0x0004:   
                if (Modbus_ReadInput(Receive_QueueValue,Reg_AddressStart,Read_RegCount,RS485Tx_Data,&Tx_len) != 1U) {
                    break;
                }
                RS485_SetTx();
                if (HAL_UART_Transmit_IT(&huart1, RS485Tx_Data, Tx_len) != HAL_OK) {
                    RS485_SetReceive();
                }
               
            default:break;
        }
    }
}
