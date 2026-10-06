#ifndef __ADC_DATA_H_
#define __ADC_DATA_H_

#include <stdint.h>
#include <RTC_Time.h>
#define FAULT_CONFIRM_COUNT         3U
#define FAULT_RECOVER_COUNT         3U
#define FAULT_COUNT                 5U

#define ADC_FAULT_OVERCURRENT       (1U << 0)
#define ADC_FAULT_OVERTEMP          (1U << 1)
#define ADC_FAULT_UNDERVOLT         (1U << 2)
#define ADC_FAULT_MOTOR_ABNORMAL    (1U << 3)
#define ADC_FAULT_MOTOR_SPEED_STALL (1U << 4)

typedef struct
{
   
uint8_t Temp_Min_Trip;
uint8_t Temp_Min_Release;
uint8_t Temp_Max_Trip;
uint8_t Temp_Max_Release;

uint8_t Current_Trip;
uint8_t Current_Relsase;

float Volt_Trip;
uint8_t Volt_Relsase;

uint16_t Motor_Speed_Min_Trip;
uint16_t Motor_Speed_Min_Release;
uint16_t Motor_Speed_Max_Trip;
uint16_t Motor_Speed_Max_Release;
uint8_t Motor_Speed_Stall_Trip;
uint8_t Motor_Speed_Stall_Release;

}ADC_Thresholds_t;

extern volatile uint8_t Temp_Min_Trip;
extern volatile uint8_t Temp_Min_Release;
extern volatile uint8_t Temp_Max_Trip;
extern volatile uint8_t Temp_Max_Release;
extern volatile uint8_t Current_Trip;
extern volatile uint8_t Current_Relsase;
extern volatile float Volt_Trip;
extern volatile uint8_t Volt_Relsase;
extern volatile uint16_t Motor_Speed_Min_Trip;
extern volatile uint16_t Motor_Speed_Min_Release;
extern volatile uint16_t Motor_Speed_Max_Trip;
extern volatile uint16_t Motor_Speed_Max_Release;
extern volatile uint8_t Motor_Speed_Stall_Trip;
extern volatile uint8_t Motor_Speed_Stall_Release;


typedef struct {

    uint32_t sequence;
    RTC_DataTime_t DataTime;

    uint16_t Motor_rpm;
    float Temp_val;
    float Volt_val;
    float Curr_val;

    uint8_t fault_code;
}Motor_Data_t;

typedef struct {
    uint16_t ADC_Data_Raw[4];
    uint32_t Timestamp;
}Adc_Sample_t;

void ADC_AcquisitionTask(void *arg);

void ADC_ProcessingTask(void *arg);


#endif
