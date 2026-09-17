#ifndef  __RTC_Time_H_
#define  __RTC_Time_H_

#include <stdint.h>

#define RTC_INIT_MAGIC    0xA5A55A5AU

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
}RTC_DataTime_t;

HAL_StatusTypeDef RTC_TimeInit();

HAL_StatusTypeDef RTC_ReadTimeData(RTC_DataTime_t *datatime);

#endif