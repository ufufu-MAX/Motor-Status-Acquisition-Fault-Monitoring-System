#include "main.h"
#include "FreeRTOS.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_uart.h"
#include "task.h"
#include "queue.h"
#include "adc.h"
#include "usart.h"
#include "cmsis_os2.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ADC_DATA.h"
#include "RTC_Time.h"


volatile Motor_Data_t ADC_Val;
TaskHandle_t ADC_ProcessingHandle;
TaskHandle_t ADC_AcquisitionHandle;
Adc_Sample_t ADC_Data;
QueueHandle_t ADC_QueueHandle;

static volatile uint8_t FaultState = 0;
static uint8_t FaultBadCount[FAULT_COUNT] = {0};
static uint8_t FaultGoodCount[FAULT_COUNT] = {0};

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
        if ((Data->Curr_val >= CURRENT_TRIP)) {
            Fault |= ADC_FAULT_OVERCURRENT;
        }
    }else {
            if (Data->Curr_val >= CURRENT_RELEASE) {
                Fault |= ADC_FAULT_OVERCURRENT;
            }
    }
//********************************************************************* */
    if ((FaultState & ADC_FAULT_OVERTEMP) == 0) {
        if ((Data->Temp_val >= TEMP_MAX_TPIP) || (Data->Temp_val <= TEMP_MIN_TPIP)) {
            Fault |= ADC_FAULT_OVERTEMP;
        }
    }else {
        if ((Data->Temp_val >= TEMP_MAX_RELEASE) || (Data->Temp_val <= TEMP_MIN_RELEASE)) {
            Fault |= ADC_FAULT_OVERTEMP;
        }
    }
    //********************************************************************* */
    if ((FaultState & ADC_FAULT_UNDERVOLT) == 0) {
        if ((Data->Volt_val >= VOLT_TRIP)) {
            Fault |= ADC_FAULT_UNDERVOLT;
        }
    }else {
            if (Data->Volt_val >= VOLT_RELEASE) {
                Fault |= ADC_FAULT_UNDERVOLT;
            }
    }
/*********************************************************************************** */
    if ((FaultState & ADC_FAULT_MOTOR_SPEED_STALL) == 0) {
        if (Data->Motor_rpm <= MOTOR_SPEED_STALL_TRIP) {
            Fault |= ADC_FAULT_MOTOR_SPEED_STALL;
            return  Fault; 
        }
        if ((FaultState & ADC_FAULT_MOTOR_ABNORMAL) == 0) {
            if ((Data->Motor_rpm >= MOTOR_SPEED_MAX_TRIP) || Data->Motor_rpm <= MOTOR_SPEED_MIN_TRIP) {
                Fault |= ADC_FAULT_MOTOR_ABNORMAL;
            }  
        }else {
            if ((Data->Motor_rpm >= MOTOR_SPEED_MAX_RELEASE) || Data->Motor_rpm <= MOTOR_SPEED_MIN_RELSASE) {
                Fault |= ADC_FAULT_MOTOR_ABNORMAL;
            }
        }
    }else {
        if (Data->Motor_rpm <= MOTOR_SPEED_STALL_RELEASE) {
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
    uint8_t Tx_buffer[128] = {0};
    uint16_t Tx_Raw[4] = {0};
    uint32_t Adc_Sum[4] = {0};
    uint8_t Sample_count = 0;
    uint8_t Fault = 0;
    RTC_DataTime_t datatime;
    (void)arg;
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
                Fault = ADC_UpdateFault(&ADC_Val);
                break;
            }           
        }
             if (RTC_ReadTimeData(&datatime) == HAL_OK) {
                ADC_Val.DataTime = datatime;
                ADC_Val.sequence++;
                int len = snprintf((char *)Tx_buffer, sizeof(Tx_buffer),"序号：%lu: %u年%u月%u日 %u时%u分%u秒: RPM:%u,I:%.2f,T:%.2f,V:%.2f,FAULT:0x%02X\r\n",
                                   (unsigned long)ADC_Val.sequence,
                                   (unsigned int)datatime.year,
                                   (unsigned int)datatime.month,
                                   (unsigned int)datatime.day,
                                   (unsigned int)datatime.hours,
                                   (unsigned int)datatime.minutes,
                                   (unsigned int)datatime.seconds,
                                   (unsigned int)ADC_Val.Motor_rpm,
                                   ADC_Val.Curr_val,
                                   ADC_Val.Temp_val,
                                   ADC_Val.Volt_val,
                                   (unsigned int)Fault);
                if(HAL_UART_Transmit_DMA(&huart2, Tx_buffer, len) != HAL_OK){
                    Error_Handler();
                }
             }else {
                Error_Handler();
             }
            
    }
}
