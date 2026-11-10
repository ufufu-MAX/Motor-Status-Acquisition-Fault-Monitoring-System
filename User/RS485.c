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
#include <string.h>
#include "ADC_DATA.h"
#include "semphr.h"
#include "W25Q64.h"

extern volatile Motor_Data_t ADC_Val;
xSemaphoreHandle Motor_Mutex = NULL;

#define REG_SEQUENCE_HIGH   0x0000U
#define REG_SEQUENCE_LOW    0x0001U
#define REG_MOTOR_RPM       0x0002U
#define REG_TEMPERATURE     0x0003U
#define REG_VOLTAGE         0x0004U
#define REG_CURRENT         0x0005U
#define REG_FAULT_CODE      0x0006U

#define REG_CURRENT_TRIP              0x0100U
#define REG_CURRENT_RELEASE           0x0101U
#define REG_TEMP_MAX_TRIP             0x0102U
#define REG_TEMP_MAX_RELEASE          0x0103U
#define REG_MOTOR_SPEED_STALL_TRIP     0x0104U
#define REG_MOTOR_SPEED_STALL_RELEASE  0x0105U
#define REG_TEMP_MIN_TRIP             0x0106U
#define REG_TEMP_MIN_RELEASE          0x0107U
#define REG_VOLT_TRIP                 0x0108U
#define REG_VOLT_RELEASE              0x0109U
#define REG_MOTOR_SPEED_MIN_TRIP       0x010AU
#define REG_MOTOR_SPEED_MIN_RELEASE    0x010BU
#define REG_MOTOR_SPEED_MAX_TRIP       0x010CU
#define REG_MOTOR_SPEED_MAX_RELEASE    0x010DU

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
    RS485_SetReceive();
    if (HAL_UARTEx_ReceiveToIdle_IT(&huart1, RS485_Receive.Data, sizeof(RS485_Receive.Data)) != HAL_OK) {
        vTaskDelete(NULL);
        return;
    }
    (void)HAL_UARTEx_ReceiveToIdle_IT(&huart1, RS485_Receive.Data, sizeof(RS485_Receive.Data));
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

static uint8_t RS485_ReadThreshold(uint16_t Address,uint16_t *Value)
{
    switch (Address) {
    case REG_CURRENT_TRIP:
        *Value = (uint16_t)Current_Trip * 100U;
        break;
    case REG_CURRENT_RELEASE:
        *Value = (uint16_t)Current_Relsase * 100U;
        break;
    case REG_TEMP_MAX_TRIP:
        *Value = (uint16_t)Temp_Max_Trip * 100U;
        break;
    case REG_TEMP_MAX_RELEASE:
        *Value = (uint16_t)Temp_Max_Release * 100U;
        break;
    case REG_MOTOR_SPEED_STALL_TRIP:
        *Value = Motor_Speed_Stall_Trip;
        break;
    case REG_MOTOR_SPEED_STALL_RELEASE:
        *Value = Motor_Speed_Stall_Release;
        break;
    case REG_TEMP_MIN_TRIP:
        *Value = (uint16_t)Temp_Min_Trip * 100U;
        break;
    case REG_TEMP_MIN_RELEASE:
        *Value = (uint16_t)Temp_Min_Release * 100U;
        break;
    case REG_VOLT_TRIP:
        *Value = (uint16_t)(Volt_Trip * 100.0f + 0.5f);
        break;
    case REG_VOLT_RELEASE:
        *Value = (uint16_t)Volt_Relsase * 100U;
        break;
    case REG_MOTOR_SPEED_MIN_TRIP:
        *Value = Motor_Speed_Min_Trip;
        break;
    case REG_MOTOR_SPEED_MIN_RELEASE:
        *Value = Motor_Speed_Min_Release;
        break;
    case REG_MOTOR_SPEED_MAX_TRIP:
        *Value = Motor_Speed_Max_Trip;
        break;
    case REG_MOTOR_SPEED_MAX_RELEASE:
        *Value = Motor_Speed_Max_Release;
        break;
    default:
        return 0U;
    }
    return 1U;
}

/* Return 0 on success, otherwise a Modbus exception code. */
static uint8_t RS485_WriteThreshold(uint16_t Address,uint16_t Value)
{
    ADC_Thresholds_t threshold;

    memset(&threshold, 0, sizeof(threshold));
    threshold.Temp_Min_Trip = Temp_Min_Trip;
    threshold.Temp_Min_Release = Temp_Min_Release;
    threshold.Temp_Max_Trip = Temp_Max_Trip;
    threshold.Temp_Max_Release = Temp_Max_Release;
    threshold.Current_Trip = Current_Trip;
    threshold.Current_Relsase = Current_Relsase;
    threshold.Volt_Trip = Volt_Trip;
    threshold.Volt_Relsase = Volt_Relsase;
    threshold.Motor_Speed_Min_Trip = Motor_Speed_Min_Trip;
    threshold.Motor_Speed_Min_Release = Motor_Speed_Min_Release;
    threshold.Motor_Speed_Max_Trip = Motor_Speed_Max_Trip;
    threshold.Motor_Speed_Max_Release = Motor_Speed_Max_Release;
    threshold.Motor_Speed_Stall_Trip = Motor_Speed_Stall_Trip;
    threshold.Motor_Speed_Stall_Release = Motor_Speed_Stall_Release;

    switch (Address) {
    case REG_CURRENT_TRIP:
        if ((Value > 1000U) || (Value % 100U != 0U) ||
            (Value <= (uint16_t)threshold.Current_Relsase * 100U)) {
            return 0x03U;
        }
        threshold.Current_Trip = (uint8_t)(Value / 100U);
        break;
    case REG_CURRENT_RELEASE:
        if ((Value % 100U != 0U) ||
            (Value >= (uint16_t)threshold.Current_Trip * 100U)) {
            return 0x03U;
        }
        threshold.Current_Relsase = (uint8_t)(Value / 100U);
        break;
    case REG_TEMP_MAX_TRIP:
        if ((Value > 10000U) || (Value % 100U != 0U) ||
            (Value <= (uint16_t)threshold.Temp_Max_Release * 100U)) {
            return 0x03U;
        }
        threshold.Temp_Max_Trip = (uint8_t)(Value / 100U);
        break;
    case REG_TEMP_MAX_RELEASE:
        if ((Value % 100U != 0U) ||
            (Value >= (uint16_t)threshold.Temp_Max_Trip * 100U) ||
            (Value <= (uint16_t)threshold.Temp_Min_Release * 100U)) {
            return 0x03U;
        }
        threshold.Temp_Max_Release = (uint8_t)(Value / 100U);
        break;
    case REG_MOTOR_SPEED_STALL_TRIP:
        if ((Value > UINT8_MAX) || (Value >= threshold.Motor_Speed_Stall_Release)) {
            return 0x03U;
        }
        threshold.Motor_Speed_Stall_Trip = (uint8_t)Value;
        break;
    case REG_MOTOR_SPEED_STALL_RELEASE:
        if ((Value > UINT8_MAX) || (Value <= threshold.Motor_Speed_Stall_Trip) ||
            (Value >= threshold.Motor_Speed_Min_Trip)) {
            return 0x03U;
        }
        threshold.Motor_Speed_Stall_Release = (uint8_t)Value;
        break;
    case REG_TEMP_MIN_TRIP:
        if ((Value % 100U != 0U) ||
            (Value >= (uint16_t)threshold.Temp_Min_Release * 100U)) {
            return 0x03U;
        }
        threshold.Temp_Min_Trip = (uint8_t)(Value / 100U);
        break;
    case REG_TEMP_MIN_RELEASE:
        if ((Value % 100U != 0U) ||
            (Value <= (uint16_t)threshold.Temp_Min_Trip * 100U) ||
            (Value >= (uint16_t)threshold.Temp_Max_Release * 100U)) {
            return 0x03U;
        }
        threshold.Temp_Min_Release = (uint8_t)(Value / 100U);
        break;
    case REG_VOLT_TRIP:
        if ((Value == 0U) || (Value > 330U) ||
            (Value >= (uint16_t)threshold.Volt_Relsase * 100U)) {
            return 0x03U;
        }
        threshold.Volt_Trip = (float)Value / 100.0f;
        break;
    case REG_VOLT_RELEASE:
        if ((Value > 330U) || (Value % 100U != 0U) ||
            (Value <= (uint16_t)(threshold.Volt_Trip * 100.0f + 0.5f))) {
            return 0x03U;
        }
        threshold.Volt_Relsase = (uint8_t)(Value / 100U);
        break;
    case REG_MOTOR_SPEED_MIN_TRIP:
        if ((Value <= threshold.Motor_Speed_Stall_Release) ||
            (Value >= threshold.Motor_Speed_Min_Release)) {
            return 0x03U;
        }
        threshold.Motor_Speed_Min_Trip = Value;
        break;
    case REG_MOTOR_SPEED_MIN_RELEASE:
        if ((Value <= threshold.Motor_Speed_Min_Trip) ||
            (Value >= threshold.Motor_Speed_Max_Release)) {
            return 0x03U;
        }
        threshold.Motor_Speed_Min_Release = Value;
        break;
    case REG_MOTOR_SPEED_MAX_TRIP:
        if (Value <= threshold.Motor_Speed_Max_Release) {
            return 0x03U;
        }
        threshold.Motor_Speed_Max_Trip = Value;
        break;
    case REG_MOTOR_SPEED_MAX_RELEASE:
        if ((Value >= threshold.Motor_Speed_Max_Trip) ||
            (Value <= threshold.Motor_Speed_Min_Release)) {
            return 0x03U;
        }
        threshold.Motor_Speed_Max_Release = Value;
        break;
    default:
        return 0x02U;
    }
    if (W25Q64_ConfigSave(&threshold) != W25Q64_OK) {
        return 0x04U;
    }

    Temp_Min_Trip = threshold.Temp_Min_Trip;
    Temp_Min_Release = threshold.Temp_Min_Release;
    Temp_Max_Trip = threshold.Temp_Max_Trip;
    Temp_Max_Release = threshold.Temp_Max_Release;
    Current_Trip = threshold.Current_Trip;
    Current_Relsase = threshold.Current_Relsase;
    Volt_Trip = threshold.Volt_Trip;
    Volt_Relsase = threshold.Volt_Relsase;
    Motor_Speed_Min_Trip = threshold.Motor_Speed_Min_Trip;
    Motor_Speed_Min_Release = threshold.Motor_Speed_Min_Release;
    Motor_Speed_Max_Trip = threshold.Motor_Speed_Max_Trip;
    Motor_Speed_Max_Release = threshold.Motor_Speed_Max_Release;
    Motor_Speed_Stall_Trip = threshold.Motor_Speed_Stall_Trip;
    Motor_Speed_Stall_Release = threshold.Motor_Speed_Stall_Release;
    return 0U;
}

static uint8_t Modbus_ReadHolding(uint16_t Reg_AddressStart,uint16_t Read_RegCount,uint8_t *Tx_Data,uint8_t *len)
{
    uint16_t Data_Length;
    uint16_t CRC_Value;

    if ((Read_RegCount == 0U) || (Read_RegCount > 14U)) {
        return 0x03U;
    }
    if ((Reg_AddressStart < REG_CURRENT_TRIP) ||
        ((uint32_t)Reg_AddressStart + Read_RegCount - 1U > REG_MOTOR_SPEED_MAX_RELEASE)) {
        return 0x02U;
    }

    Tx_Data[0] = RS485_SLAVE_ADDRESS;
    Tx_Data[1] = 0x03U;
    Tx_Data[2] = (uint8_t)(Read_RegCount * 2U);

    taskENTER_CRITICAL();
    for (uint16_t i = 0U; i < Read_RegCount; i++) {
        uint16_t Value;
        if (RS485_ReadThreshold(Reg_AddressStart + i, &Value) == 0U) {
            taskEXIT_CRITICAL();
            return 0x02U;
        }
        Writeuint16_To_uint8(&Tx_Data[3U + i * 2U], Value);
    }
    taskEXIT_CRITICAL();

    Data_Length = (uint16_t)(3U + Read_RegCount * 2U);
    CRC_Value = Modbus_CRC16(Tx_Data, Data_Length);
    Tx_Data[Data_Length] = (uint8_t)(CRC_Value & 0xFFU);
    Tx_Data[Data_Length + 1U] = (uint8_t)(CRC_Value >> 8);
    *len = (uint8_t)(Data_Length + 2U);
    return 0U;
}

static void Modbus_Exception(uint8_t CMD,uint8_t Exception,uint8_t *Tx_Data,uint8_t *len)
{
    uint16_t CRC_Value;
    Tx_Data[0] = RS485_SLAVE_ADDRESS;
    Tx_Data[1] = CMD | 0x80U;
    Tx_Data[2] = Exception;
    CRC_Value = Modbus_CRC16(Tx_Data, 3U);
    Tx_Data[3] = (uint8_t)(CRC_Value & 0xFFU);
    Tx_Data[4] = (uint8_t)(CRC_Value >> 8);
    *len = 5U;
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
    uint8_t Exception;
    uint32_t Tx_Start;
    Motor_Mutex = xSemaphoreCreateMutex();
    (void)arg;
    RS485_Init();
    while (1) {
       
        if (xQueueReceive(RS485_QueueHandle, &Receive_QueueValue, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if ((Receive_QueueValue.len < 4U) ||
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
        Tx_len = 0U;
        Exception = 0U;
        Reg_AddressStart = 0U;
        Read_RegCount = 0U;
        if (((RS485_CMD == 0x03U) || (RS485_CMD == 0x04U) || (RS485_CMD == 0x06U)) &&
            (Receive_QueueValue.len != 8U)) {
            Exception = 0x03U;
        }else {
            if (Receive_QueueValue.len == 8U) {
                Reg_AddressStart = Readuint8_To_uint16(&Receive_QueueValue.Data[2]);
                Read_RegCount = Readuint8_To_uint16(&Receive_QueueValue.Data[4]);
            }

            switch (RS485_CMD) {

            case 0x0003:
                Exception = Modbus_ReadHolding(Reg_AddressStart,Read_RegCount,RS485Tx_Data,&Tx_len);
                break;

            case 0x0004:
                if (Modbus_ReadInput(Receive_QueueValue,Reg_AddressStart,Read_RegCount,RS485Tx_Data,&Tx_len) != 1U) {
                    Exception = (Read_RegCount == 0U || Read_RegCount > 7U) ? 0x03U : 0x02U;
                }
                break;

            case 0x0006:
                Exception = RS485_WriteThreshold(Reg_AddressStart,Read_RegCount);
                if (Exception == 0U) {
                    for (uint8_t i = 0U; i < 8U; i++) {
                        RS485Tx_Data[i] = Receive_QueueValue.Data[i];
                    }
                    Tx_len = 8U;
                }
                break;

            default:
                Exception = 0x01U;
                break;
            }
        }
        if (Exception != 0U) {
            Modbus_Exception(RS485_CMD, Exception, RS485Tx_Data, &Tx_len);
        }

        RS485_SetTx();
        if (HAL_UART_Transmit_IT(&huart1, RS485Tx_Data, Tx_len) != HAL_OK) {
            RS485_SetReceive();
            continue;
        }

        /* Keep the response buffer unchanged until transmission completes. */
        Tx_Start = HAL_GetTick();
        while (huart1.gState != HAL_UART_STATE_READY) {
            if ((uint32_t)(HAL_GetTick() - Tx_Start) >= 1000U) {
                (void)HAL_UART_AbortTransmit(&huart1);
                RS485_SetReceive();
                break;
            }
            osDelay(1);
        }
    }
}
