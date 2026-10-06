#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "adc.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_uart.h"
#include "usart.h"
#include "cmsis_os2.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ADC_DATA.h"
#include "RTC_Time.h"
#include "CLI.h"
#include "Uart_Tx.h"
#include "Stream_Control.h"

Motor_Data_t ADC_Val;
TaskHandle_t ADC_ProcessingHandle;
TaskHandle_t ADC_AcquisitionHandle;
Adc_Sample_t ADC_Data;
QueueHandle_t ADC_QueueHandle;

extern QueueHandle_t CLI_DataHandle;
extern xQueueHandle Uart_TxHandle;
extern xQueueHandle SD_Handle;
extern xQueueHandle W25Q64_Handle;
extern TaskHandle_t W25Q64_TaskHandle;

static volatile uint8_t FaultState = 0;
static uint8_t FaultBadCount[FAULT_COUNT] = {0};
static uint8_t FaultGoodCount[FAULT_COUNT] = {0};

/*
#define TEMP_MIN_TPIP               10U
#define TEMP_MIN_RELEASE            12U

#define TEMP_MAX_TPIP               60U
#define TEMP_MAX_RELEASE            55U

#define CURRENT_TRIP                8U
#define CURRENT_RELEASE             7U

#define VOLT_TRIP                   2.8f
#define VOLT_RELEASE                3U

#define MOTOR_SPEED_MIN_TRIP        2000U
#define MOTOR_SPEED_MIN_RELSASE     2200U

#define MOTOR_SPEED_MAX_TRIP        4500U
#define MOTOR_SPEED_MAX_RELEASE     4300U

#define MOTOR_SPEED_STALL_TRIP      100U
#define MOTOR_SPEED_STALL_RELEASE   200U
*/

volatile uint8_t Temp_Min_Trip = 10;
volatile uint8_t Temp_Min_Release = 12;
volatile uint8_t Temp_Max_Trip = 60;
volatile uint8_t Temp_Max_Release = 55;

volatile uint8_t Current_Trip = 8;
volatile uint8_t Current_Relsase = 7;

volatile float Volt_Trip = 2.8;
volatile uint8_t Volt_Relsase = 3;

volatile uint16_t Motor_Speed_Min_Trip = 2000;
volatile uint16_t Motor_Speed_Min_Release = 2200;
volatile uint16_t Motor_Speed_Max_Trip = 4500;
volatile uint16_t Motor_Speed_Max_Release = 4300;
volatile uint8_t Motor_Speed_Stall_Trip = 100;
volatile uint8_t Motor_Speed_Stall_Release = 200;




static uint8_t Fault_Bits[FAULT_COUNT] =
{
    ADC_FAULT_OVERCURRENT,
    ADC_FAULT_OVERTEMP,
    ADC_FAULT_UNDERVOLT,
    ADC_FAULT_MOTOR_ABNORMAL,
    ADC_FAULT_MOTOR_SPEED_STALL
};

static void Motor_ConverAll(uint16_t *Data)
{
    ADC_Val.Motor_rpm = (uint16_t)Data[0];
    ADC_Val.Curr_val = 10 *Data[1] / 4096.0;
    ADC_Val.Temp_val = 100 * Data[2] / 4096.0;
    ADC_Val.Volt_val = 3.3 * Data[3] / 4096.0;
}


static uint8_t ADC_GetInstantFault(const volatile Motor_Data_t *Data)
{

    uint8_t Fault = 0;
    
    if ((FaultState & ADC_FAULT_OVERCURRENT) == 0) {
        if ((Data->Curr_val >= Current_Trip)) {
            Fault |= ADC_FAULT_OVERCURRENT;
        }
    }else {
            if (Data->Curr_val >= Current_Relsase) {
                Fault |= ADC_FAULT_OVERCURRENT;
            }
    }
//********************************************************************* */
    if ((FaultState & ADC_FAULT_OVERTEMP) == 0) {
        if ((Data->Temp_val >= Temp_Max_Trip) || (Data->Temp_val <= Temp_Min_Trip)) {
            Fault |= ADC_FAULT_OVERTEMP;
        }
    }else {
        if ((Data->Temp_val >= Temp_Max_Release) || (Data->Temp_val <= Temp_Min_Release)) {
            Fault |= ADC_FAULT_OVERTEMP;
        }
    }
    //********************************************************************* */
    if ((FaultState & ADC_FAULT_UNDERVOLT) == 0) {
        if ((Data->Volt_val >= Volt_Trip)) {
            Fault |= ADC_FAULT_UNDERVOLT;
        }
    }else {
            if (Data->Volt_val >= Volt_Relsase) {
                Fault |= ADC_FAULT_UNDERVOLT;
            }
    }
/*********************************************************************************** */
    if ((FaultState & ADC_FAULT_MOTOR_SPEED_STALL) == 0) {
        if (Data->Motor_rpm <= Motor_Speed_Stall_Trip) {
            Fault |= ADC_FAULT_MOTOR_SPEED_STALL;
            return  Fault; 
        }
        if ((FaultState & ADC_FAULT_MOTOR_ABNORMAL) == 0) {
            if ((Data->Motor_rpm >= Motor_Speed_Max_Trip) || Data->Motor_rpm <= Motor_Speed_Min_Trip) {
                Fault |= ADC_FAULT_MOTOR_ABNORMAL;
            }  
        }else {
            if ((Data->Motor_rpm >= Motor_Speed_Max_Release) || Data->Motor_rpm <= Motor_Speed_Min_Release) {
                Fault |= ADC_FAULT_MOTOR_ABNORMAL;
            }
        }
    }else {
        if (Data->Motor_rpm <= Motor_Speed_Stall_Release) {
            Fault |= ADC_FAULT_MOTOR_SPEED_STALL;
           
        }
    }
    return Fault;
}

uint8_t ADC_UpdateFault(const volatile Motor_Data_t *Data)
{
    uint8_t i = 0;
    uint8_t bit = 0;
    uint8_t Current_Fault = 0;
    Current_Fault = ADC_GetInstantFault(Data);
    for (i=0; i < FAULT_COUNT; i++) {
        bit = Fault_Bits[i];
            if((bit & Current_Fault) != 0){
                FaultGoodCount[i] = 0;
                if (FaultBadCount[i] < 3) {
                        FaultBadCount[i]++;
                }
                if (FaultBadCount[i] >= 3) {
                        FaultBadCount[i] = FAULT_CONFIRM_COUNT;
                        FaultState |= bit;
                }
            }else {
                FaultBadCount[i] = 0;
                if (FaultGoodCount[i] < 3) {
                    FaultGoodCount[i]++;
                }
                if (FaultGoodCount[i] >= 3) {
                    FaultGoodCount[i]  = FAULT_RECOVER_COUNT;
                    FaultState &= (uint8_t)~bit;
                }
            }
    }
    return FaultState;
}

void ADC_AcquisitionTask(void *arg)
{   
    if (ADC_QueueHandle == NULL) {
        Error_Handler();
    }
    (void)arg;
    while (1)
    {
        osDelay(100);
     
        if(HAL_ADC_Start_DMA(&hadc1, (uint32_t *)ADC_Data.ADC_Data_Raw, 4) != HAL_OK){
            continue;
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xQueueSend(ADC_QueueHandle,ADC_Data.ADC_Data_Raw, portMAX_DELAY);

    }
}



void ADC_ProcessingTask(void *arg)
{
    uint16_t Tx_Raw[4] = {0};
    uint32_t Adc_Sum[4] = {0};
    uint8_t Sample_count = 0;
    UartTx_Frame_s Tx_Data;
    uint32_t Error_PacketFromW25Q64 = 0;
    (void)arg;

    /* 等待 SD_Task 恢复 MOTOR.CSV 中的最后一个序号 */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while(1){
        osDelay(100);
        for (;;) {
            if(xQueueReceive(ADC_QueueHandle, Tx_Raw, portMAX_DELAY) != pdPASS){
                continue;
            }
            Sample_count++;
            Adc_Sum[0] += Tx_Raw[0];
            Adc_Sum[1] += Tx_Raw[1];
            Adc_Sum[2] += Tx_Raw[2];
            Adc_Sum[3] += Tx_Raw[3];

            if(Sample_count >= 8){
                uint16_t Adc_average[4] = {0};
                Adc_average[0] = Adc_Sum[0] / 8;
                Adc_average[1] = Adc_Sum[1] / 8;
                Adc_average[2] = Adc_Sum[2] / 8;
                Adc_average[3] = Adc_Sum[3] / 8;
                memset(Adc_Sum, 0, sizeof(Adc_Sum));
                Sample_count = 0;
                Motor_ConverAll(Adc_average);
                taskENTER_CRITICAL();
                ADC_Val.fault_code = ADC_UpdateFault(&ADC_Val);
                taskEXIT_CRITICAL();
                break;
            }           
        }

             if (RTC_ReadTimeData(&ADC_Val.DataTime) == HAL_OK) {
                ADC_Val.sequence++;
                Motor_Data_t cli_snapshot = ADC_Val;
                if (CLI_DataHandle != NULL) {
                    (void)xQueueOverwrite(CLI_DataHandle, &cli_snapshot);
                }
                Tx_Data.len = snprintf((char *)Tx_Data.Tx_buffer, sizeof(Tx_Data.Tx_buffer),"DATA,%lu,%04u-%02u-%02uT%02u:%02u:%02u,%u,%.2f,%.2f,%.2f,0x%02X\r\n",
                                   (unsigned long)ADC_Val.sequence,
                                   (unsigned int)ADC_Val.DataTime.year,
                                   (unsigned int)ADC_Val.DataTime.month,
                                   (unsigned int)ADC_Val.DataTime.day,
                                   (unsigned int)ADC_Val.DataTime.hours,
                                   (unsigned int)ADC_Val.DataTime.minutes,
                                   (unsigned int)ADC_Val.DataTime.seconds,
                                   (unsigned int)ADC_Val.Motor_rpm,
                                   ADC_Val.Curr_val,
                                   ADC_Val.Temp_val,
                                   ADC_Val.Volt_val,
                                   (unsigned int)ADC_Val.fault_code);
                
                if ((Tx_Data.len <= 0) || (Tx_Data.len >= (int)sizeof(Tx_Data.Tx_buffer))) {
                    continue;
                }                   
                if (ADC_Val.fault_code != 0) {
                    if (xQueueSend(W25Q64_Handle, &ADC_Val, 0) != pdPASS) {
                        Error_PacketFromW25Q64++;
                    }
                }
                if (SD_Handle != NULL) {
                    if (xQueueSend(SD_Handle, &ADC_Val, 0) != pdPASS) {
                            ;
                    }
                }
                    
                if (Stream_Control_IsEnable()) {
                    xQueueSend(Uart_TxHandle,&Tx_Data, portMAX_DELAY);
                     
                }
               
             }else {
                Error_Handler();
             }
            
    }
}

