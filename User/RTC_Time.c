#include "main.h"
#include "rtc.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_rtc.h"
#include "stm32f4xx_hal_rtc_ex.h"
#include <stdatomic.h>
#include <stdint.h>
#include "RTC_Time.h"



HAL_StatusTypeDef RTC_TimeInit()
{

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == RTC_INIT_MAGIC) {
        return HAL_OK;
    }

    sDate.Year = 26;
    sDate.Month = 9;
    sDate.WeekDay  = RTC_WEEKDAY_TUESDAY;
    sDate.Date  = 14;
    sTime.Hours = 13;
    sTime.Minutes = 3;
    sTime.Seconds = 22;

    if (HAL_RTC_SetTime(&hrtc,&sTime, RTC_FORMAT_BIN) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_RTC_SetDate(&hrtc,&sDate, RTC_FORMAT_BIN) != HAL_OK) {
        return HAL_ERROR;
    }
    
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_INIT_MAGIC);

    return HAL_OK;

}

HAL_StatusTypeDef RTC_ReadTimeData(RTC_DataTime_t *datatime)
{

    RTC_TimeTypeDef Time = {0};
    RTC_DateTypeDef Data = {0};

    if (datatime == NULL) {
        return HAL_ERROR;
    }
    if (HAL_RTC_GetTime(&hrtc, &Time, RTC_FORMAT_BIN) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_RTC_GetDate(&hrtc, &Data, RTC_FORMAT_BIN) != HAL_OK) {
        return  HAL_ERROR;
    }

    datatime->year = 2000 + Data.Year;
    datatime->month = Data.Month;
    datatime->weekday = Data.WeekDay;
    datatime->day = Data.Date;
    datatime->hours = Time.Hours;
    datatime->minutes = Time.Minutes;
    datatime->seconds = Time.Seconds;

    return HAL_OK;
}