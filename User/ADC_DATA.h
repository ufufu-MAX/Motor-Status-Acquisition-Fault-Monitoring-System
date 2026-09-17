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

#define TEMP_MIN_TPIP               10U
#define TEMP_MIN_RELEASE            12U

#define TEMP_MAX_TPIP               60U
#define TEMP_MAX_RELEASE            55U

#define CURRENT_TRIP                8U
#define CURRENT_RELEASE             7U

#define VOLT_TRIP                   2.8f
#define VOLT_RELEASE                3U

#define MOTOR_SPEED_MIN_TRIP        2000U
#define MOTOR_SPEED_MIN_RELSASE    2200U

#define MOTOR_SPEED_MAX_TRIP        4500U
#define MOTOR_SPEED_MAX_RELEASE     4300U

#define MOTOR_SPEED_STALL_TRIP      100U
#define MOTOR_SPEED_STALL_RELEASE   200U


typedef struct {

    uint32_t sequence;
    RTC_DataTime_t DataTime;

    uint16_t Motor_rpm;
    float Temp_val;
    float Volt_val;
    float Curr_val;
}Motor_Data_t;

typedef struct {
    uint16_t ADC_Data_Raw[4];
    uint32_t Timestamp;
}Adc_Sample_t;

void ADC_AcquisitionTask(void *arg);

void ADC_ProcessingTask(void *arg);


#endif
