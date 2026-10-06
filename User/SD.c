#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "queue.h"
#include "task.h"
#include "fatfs.h"
#include "ff.h"
#include "integer.h"
#include "ADC_DATA.h"
#include "sdio.h"
#include "bsp_driver_sd.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


xQueueHandle SD_Handle;
extern Motor_Data_t ADC_Val;
extern TaskHandle_t ADC_ProcessingHandle;

static HAL_StatusTypeDef SD_Init()
{
    FRESULT Result;
    Result = f_mount(&SDFatFS, (TCHAR *)SDPath, 1);
    if (Result == FR_OK) {

        SD_Handle = xQueueCreate(8, sizeof(Motor_Data_t));

        if (SD_Handle == NULL) {
            return HAL_ERROR;
        }
        return HAL_OK;
    }else {
        return HAL_ERROR;
    }
}

static uint32_t SD_ReadLastSequence(FIL *File, FSIZE_t *Valid_End)
{

    uint32_t Last_Sequence = 0;
    char line[128];

    *Valid_End = 0;

    if (f_lseek(File, 0) != FR_OK) {
        return  0;
    }

    while (f_gets(line, sizeof(line), File) != NULL) {
        FSIZE_t Current_End = f_tell(File);
        char *end;
        unsigned long Value;

        if (strchr(line, '\n') == NULL) {
            break;
        }

        *Valid_End = Current_End;

        if (line[0] < '0' || line[0] > '9') {
            continue;
        }

        Value = strtoul(line, &end, 10);

        if ((end != line) && (*end == ',') && (Value <= 0xFFFFFFFFUL)) {
            Last_Sequence = (uint32_t)Value;
        }
        
    }
    return Last_Sequence;

}


static FRESULT SD_TryRemount(void)
{
    FRESULT result;
    UINT written;
    uint32_t last_sequence;
    FSIZE_t valid_end;
    const char header[] =
        "sequence,time,rpm,current,temp,voltage,fault\r\n";

    /* 解除旧卷注册，避免继续使用拔卡前的文件对象 */
    result = f_mount(NULL, (TCHAR *)SDPath, 0);
    if (result != FR_OK) {
        return result;
    }

    /* 重新检测并挂载当前 SD 卡 */
    result = f_mount(&SDFatFS, (TCHAR *)SDPath, 1);
    if (result != FR_OK) {
        goto failed;
    }

    result = f_open(&SDFile,
                    "0:/MOTOR.CSV",
                    FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
    if (result != FR_OK) {
        goto failed;
    }

    /* 找到最后一条以换行符结束的记录，并清除不完整尾行 */
    last_sequence = SD_ReadLastSequence(&SDFile, &valid_end);
    (void)last_sequence; /* 运行中不回退 ADC 序号，避免序号重复 */

    result = f_lseek(&SDFile, valid_end);
    if (result != FR_OK) {
        goto failed;
    }

    result = f_truncate(&SDFile);
    if (result != FR_OK) {
        goto failed;
    }

    /* 新卡或空文件需要补写表头 */
    if (f_size(&SDFile) == 0) {
        result = f_write(&SDFile, header, sizeof(header) - 1, &written);
        if ((result != FR_OK) || (written != sizeof(header) - 1)) {
            if (result == FR_OK) {
                result = FR_DISK_ERR;
            }
            goto failed;
        }

        result = f_sync(&SDFile);
        if (result != FR_OK) {
            goto failed;
        }
    }

    /* 恢复到文件末尾，后续记录从这里追加 */
    result = f_lseek(&SDFile, f_size(&SDFile));
    if (result != FR_OK) {
        goto failed;
    }

    return FR_OK;

failed:
    /* 让下次重试从重新挂载开始，不继续使用可能失效的 FIL */
    (void)f_mount(NULL, (TCHAR *)SDPath, 0);
    return result;
}


void SD_Task(void *arg)
{
    FRESULT Result;
    UINT Written;
    Motor_Data_t Write_Data;
    char line[128];
    uint8_t Flush_CacheCount = 0;
    uint8_t SD_Online = 1;
    uint32_t last_sequence;
    FSIZE_t valid_end;
    const char Header[] =  "sequence,time,rpm,current,temp,voltage,fault\r\n";
    if (SD_Init() != HAL_OK) {
        ADC_Val.sequence = 0;
        xTaskNotifyGive(ADC_ProcessingHandle);
        vTaskDelete(NULL);
        return;
    }

   Result = f_open(&SDFile,
                "0:/MOTOR.CSV",
                FA_OPEN_ALWAYS | FA_READ | FA_WRITE);

    if (Result != FR_OK) {
        ADC_Val.sequence = 0;
        xTaskNotifyGive(ADC_ProcessingHandle);
        vTaskDelete(NULL);
        return;
    }

    /* 读取旧文件最后一条有效记录的序号 */
    last_sequence = SD_ReadLastSequence(&SDFile, &valid_end);

    /* 删除文件末尾可能存在的不完整行 */
    if ((f_lseek(&SDFile, valid_end) != FR_OK) || (f_truncate(&SDFile) != FR_OK)) {
        ADC_Val.sequence = 0;
        xTaskNotifyGive(ADC_ProcessingHandle);
        vTaskDelete(NULL);
        return;
    }

    /* 新文件第一次使用时写表头 */
    if (f_size(&SDFile) == 0) {
        Result = f_write(&SDFile, Header,(UINT)sizeof(Header) - 1,&Written);

        if ((Result != FR_OK) || (Written != (UINT)sizeof(Header) - 1)) {
            ADC_Val.sequence = 0;
            xTaskNotifyGive(ADC_ProcessingHandle);
            vTaskDelete(NULL);
            return;
        }
    }

    /* 文件指针移动到真正的末尾，后面执行追加写入 */
    if (f_lseek(&SDFile, f_size(&SDFile)) != FR_OK) {
        ADC_Val.sequence = 0;
        xTaskNotifyGive(ADC_ProcessingHandle);
        vTaskDelete(NULL);
        return;
    }

    /* 恢复旧序号 */
    ADC_Val.sequence = last_sequence;

    /* 通知 ADC 任务：序号已经恢复，可以开始工作 */
    xTaskNotifyGive(ADC_ProcessingHandle);

    while (1) 
    {
        if (!SD_Online) {
            osDelay(pdMS_TO_TICKS(1000));
            if (SD_TryRemount() == FR_OK) {
                SD_Online = 1;
            }
            continue;
        }

        osDelay(pdMS_TO_TICKS(10));

        xQueueReceive(SD_Handle, &Write_Data, portMAX_DELAY);
        int len = snprintf(line, sizeof(line), "%lu,%04u-%02u-%02uT%02u:%02u:%02u,%u,%.2f,%.2f,%.2f,0x%02X\r\n", 
                                                (unsigned long)Write_Data.sequence,
                                                (unsigned int)Write_Data.DataTime.year,
                                                (unsigned int)Write_Data.DataTime.month,
                                                (unsigned int)Write_Data.DataTime.day,
                                                (unsigned int)Write_Data.DataTime.hours,
                                                (unsigned int)Write_Data.DataTime.minutes,
                                                (unsigned int)Write_Data.DataTime.seconds,
                                                (unsigned int)Write_Data.Motor_rpm,
                                                Write_Data.Curr_val,
                                                Write_Data.Temp_val,
                                                Write_Data.Volt_val,
                                                (unsigned int)Write_Data.fault_code);

        if ((len <= 0) || (len >= (int)sizeof(line))) {
            f_close(&SDFile);
            osDelay(portMAX_DELAY);
        }       
        Result = f_write(&SDFile, line, (UINT)len, &Written);
        if ((Result != FR_OK) || (Written != (UINT)len)) {
            SD_Online = 0;
            continue;
        }
        Flush_CacheCount++;
        if (Flush_CacheCount >= 10) {
            Flush_CacheCount = 0;
            if (Result == FR_OK) {
                Result = f_sync(&SDFile);
                if (Result != FR_OK) {
                    SD_Online = 0;
                    continue;
                }
            }
        }

    }
}
