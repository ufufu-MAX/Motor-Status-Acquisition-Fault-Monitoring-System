#include "FreeRTOS.h"
#include "main.h"
#include "portmacro.h"
#include "stm32f4xx_hal_gpio.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "spi.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "W25Q64.h"
#include "ADC_DATA.h"
#include "crc.h"

#define W25Q64_CS_PORT          GPIOA
#define W25Q64_CS_PIN           GPIO_PIN_4

xQueueHandle W25Q64_Handle;
TaskHandle_t W25Q64_TaskHandle;
uint32_t WriteAdd_Offset = 0x0;
static SemaphoreHandle_t W25Q64_Mutex;

uint32_t CRC_Calc_Bytes(CRC_HandleTypeDef *hcrc, uint8_t *pBuffer, uint32_t len)
{
    uint32_t words = len / 4;
    uint32_t rem = len % 4;
    uint32_t crc = HAL_CRC_Calculate(hcrc, (uint32_t *)pBuffer, words);
    if(rem)
    {
        uint32_t tmp = 0;
        memcpy(&tmp, pBuffer + words*4, rem);
        crc = HAL_CRC_Accumulate(hcrc, &tmp, 1);
    }
    return crc;
}


static uint8_t W25Q64_AddressValid(uint32_t address, uint32_t length)
{
    return (address < W25Q64_TOTAL_SIZE) &&
           (length <= (W25Q64_TOTAL_SIZE - address));
}

static void W25Q64_CS_Low(void)
{
    HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_RESET);
}

static void W25Q64_CS_High(void)
{
    HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_SET);
}

static HAL_StatusTypeDef W25Q64_SendAddress(uint32_t address)
{
    uint8_t Address_Data[3] = {0};

    Address_Data[0] = (uint8_t)(address >> 16);
    Address_Data[1] = (uint8_t)(address >> 8);
    Address_Data[2] = (uint8_t)address;

    return HAL_SPI_Transmit(&hspi1, Address_Data, 3, W25Q64_TIMEOUT_MS);
}

static W25Q64_Status_t W25Q64_ReadStatus(uint8_t *status)
{
    uint8_t Command = W25Q64_CMD_READ_STATUS;

    if (status == NULL) {
        return W25Q64_BAD_PARAMETER;
    }

    W25Q64_CS_Low();
    if (HAL_SPI_Transmit(&hspi1, &Command, 1, W25Q64_TIMEOUT_MS) != HAL_OK ||
        HAL_SPI_Receive(&hspi1, status, 1, W25Q64_TIMEOUT_MS) != HAL_OK) {
        W25Q64_CS_High();
        return W25Q64_ERROR;
    }
    W25Q64_CS_High();

    return W25Q64_OK;
}

static W25Q64_Status_t W25Q64_WriteEnable(void)
{
    uint8_t Command = W25Q64_CMD_WRITE_ENABLE;
    uint8_t status = 0;

    W25Q64_CS_Low();
    if (HAL_SPI_Transmit(&hspi1, &Command, 1, W25Q64_TIMEOUT_MS) != HAL_OK) {
        W25Q64_CS_High();
        return W25Q64_ERROR;
    }
    W25Q64_CS_High();

    if (W25Q64_ReadStatus(&status) != W25Q64_OK) {
        return W25Q64_ERROR;
    }
    if ((status & W25Q64_STATUS_WEL) == 0U) {
        return W25Q64_ERROR;
    }

    return W25Q64_OK;
}

static W25Q64_Status_t W25Q64_WaitReady(uint32_t timeout)
{
    uint32_t Tick = HAL_GetTick();
    uint8_t status = 0;

    while ((HAL_GetTick() - Tick) < timeout) {
        if (W25Q64_ReadStatus(&status) != W25Q64_OK) {
            return W25Q64_ERROR;
        }
        if ((status & W25Q64_STATUS_BUSY) == 0U) {
            return W25Q64_OK;
        }
    }

    return W25Q64_BUSY;
}


W25Q64_Status_t W25Q64_EraseSector(uint32_t address)
{
    uint8_t Command = W25Q64_CMD_SECTOR_ERASE;

    if ((address % W25Q64_SECTOR_SIZE) != 0U ||
        !W25Q64_AddressValid(address, W25Q64_SECTOR_SIZE)) {
        return W25Q64_BAD_PARAMETER;
    }
    if (W25Q64_WriteEnable() != W25Q64_OK) {
        return W25Q64_ERROR;
    }

    W25Q64_CS_Low();
    if (HAL_SPI_Transmit(&hspi1, &Command, 1, W25Q64_TIMEOUT_MS) != HAL_OK ||
        W25Q64_SendAddress(address) != HAL_OK) {
        W25Q64_CS_High();
        return W25Q64_ERROR;
    }
    W25Q64_CS_High();

    return W25Q64_WaitReady(W25Q64_TIMEOUT_MS);
}

W25Q64_Status_t W25Q64_WritePage(uint32_t address, uint8_t *data, uint16_t length)
{
    uint8_t Command = W25Q64_CMD_PAGE_PROGRAM;

    if ((data == NULL) || (length == 0U) || (length > W25Q64_PAGE_SIZE) ||
        !W25Q64_AddressValid(address, length) ||
        (((address & 0xFFU) + length) > W25Q64_PAGE_SIZE)) {
        return W25Q64_BAD_PARAMETER;
    }
    if (W25Q64_WriteEnable() != W25Q64_OK) {
        return W25Q64_ERROR;
    }

    W25Q64_CS_Low();
    if (HAL_SPI_Transmit(&hspi1, &Command, 1, W25Q64_TIMEOUT_MS) != HAL_OK ||
        W25Q64_SendAddress(address) != HAL_OK ||
        HAL_SPI_Transmit(&hspi1, data, length, W25Q64_TIMEOUT_MS) != HAL_OK) {
        W25Q64_CS_High();
        return W25Q64_ERROR;
    }
    W25Q64_CS_High();

    return W25Q64_WaitReady(W25Q64_TIMEOUT_MS);
}

W25Q64_Status_t W25Q64_Read(uint32_t address, uint8_t *data, uint32_t length)
{
    uint8_t Command = W25Q64_CMD_READ_DATA;

    if ((data == NULL) || (length == 0U) ||
        !W25Q64_AddressValid(address, length)) {
        return W25Q64_BAD_PARAMETER;
    }

    W25Q64_CS_Low();
    if (HAL_SPI_Transmit(&hspi1, &Command, 1, W25Q64_TIMEOUT_MS) != HAL_OK ||
        W25Q64_SendAddress(address) != HAL_OK ||
        HAL_SPI_Receive(&hspi1, data, length, W25Q64_TIMEOUT_MS) != HAL_OK) {
        W25Q64_CS_High();
        return W25Q64_ERROR;
    }
    W25Q64_CS_High();

    return W25Q64_OK;
}

W25Q64_Status_t W25Q64_Init()
{
    W25Q64_Handle = xQueueCreate(8, sizeof(Motor_Data_t));
    W25Q64_Mutex = xSemaphoreCreateMutex();
    HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_SET);
    return ((W25Q64_Handle != NULL) && (W25Q64_Mutex != NULL)) ? W25Q64_OK : W25Q64_ERROR;

}

W25Q64_Status_t W25Q64_ConfigLoad(void)
{
    W25Q64_ThresholdRecord_t record;
    ADC_Thresholds_t latest;
    RTC_DataTime_t time;
    uint64_t latest_key = 0U, key;
    uint8_t found = 0U, empty;

    for (uint32_t i = 0U; i < (W25Q64_SECTOR_SIZE / W25Q64_THRESHOLD_SLOT); i++) {
        if (W25Q64_Read(W25Q64_THRESHOLD_START + i * W25Q64_THRESHOLD_SLOT,
                        (uint8_t *)&record, sizeof(record)) != W25Q64_OK) {
            break;
        }
        empty = 1U;
        for (uint32_t j = 0U; j < sizeof(record); j++) {
            if (((uint8_t *)&record)[j] != 0xFFU) {
                empty = 0U;
                break;
            }
        }
        if (empty) {
            break;
        }
        if (CRC_Calc_Bytes(&hcrc, record.Data, sizeof(record.Data)) != record.Crc32) {
            break;
        }

        memcpy(&time, record.Data, sizeof(time));
        key = (((((uint64_t)time.year * 13U + time.month) * 32U + time.day) * 24U +
                time.hours) * 60U + time.minutes) * 60U + time.seconds;
        if (!found || (key >= latest_key)) {
            latest_key = key;
            memcpy(&latest, record.Data + sizeof(time), sizeof(latest));
            found = 1U;
        }
    }

    if (!found) {
        return W25Q64_ERROR;
    }

    Temp_Min_Trip = latest.Temp_Min_Trip;
    Temp_Min_Release = latest.Temp_Min_Release;
    Temp_Max_Trip = latest.Temp_Max_Trip;
    Temp_Max_Release = latest.Temp_Max_Release;
    Current_Trip = latest.Current_Trip;
    Current_Relsase = latest.Current_Relsase;
    Volt_Trip = latest.Volt_Trip;
    Volt_Relsase = latest.Volt_Relsase;
    Motor_Speed_Min_Trip = latest.Motor_Speed_Min_Trip;
    Motor_Speed_Min_Release = latest.Motor_Speed_Min_Release;
    Motor_Speed_Max_Trip = latest.Motor_Speed_Max_Trip;
    Motor_Speed_Max_Release = latest.Motor_Speed_Max_Release;
    Motor_Speed_Stall_Trip = latest.Motor_Speed_Stall_Trip;
    Motor_Speed_Stall_Release = latest.Motor_Speed_Stall_Release;
    return W25Q64_OK;
}

W25Q64_Status_t W25Q64_ConfigSave(const ADC_Thresholds_t *thresholds)
{
    W25Q64_ThresholdRecord_t record, verify;
    RTC_DataTime_t time;
    uint32_t address = W25Q64_THRESHOLD_START;
    uint8_t empty, erase = 0U;

    if ((thresholds == NULL) || (W25Q64_Mutex == NULL) ||
        (RTC_ReadTimeData(&time) != HAL_OK)) {
        return W25Q64_ERROR;
    }

    xSemaphoreTake(W25Q64_Mutex, portMAX_DELAY);
    for (uint32_t i = 0U; i < (W25Q64_SECTOR_SIZE / W25Q64_THRESHOLD_SLOT); i++) {
        if (W25Q64_Read(address, (uint8_t *)&record, sizeof(record)) != W25Q64_OK) {
            xSemaphoreGive(W25Q64_Mutex);
            return W25Q64_ERROR;
        }
        empty = 1U;
        for (uint32_t j = 0U; j < sizeof(record); j++) {
            if (((uint8_t *)&record)[j] != 0xFFU) {
                empty = 0U;
                break;
            }
        }
        if (empty) {
            break;
        }
        if (CRC_Calc_Bytes(&hcrc, record.Data, sizeof(record.Data)) != record.Crc32) {
            erase = 1U;
            break;
        }
        address += W25Q64_THRESHOLD_SLOT;
    }

    if ((address >= W25Q64_THRESHOLD_START + W25Q64_SECTOR_SIZE) || erase) {
        if (W25Q64_EraseSector(W25Q64_THRESHOLD_START) != W25Q64_OK) {
            xSemaphoreGive(W25Q64_Mutex);
            return W25Q64_ERROR;
        }
        address = W25Q64_THRESHOLD_START;
    }

    memset(&record, 0xFF, sizeof(record));
    memcpy(record.Data, &time, sizeof(time));
    memcpy(record.Data + sizeof(time), thresholds, sizeof(*thresholds));
    record.Crc32 = CRC_Calc_Bytes(&hcrc, record.Data, sizeof(record.Data));

    if ((W25Q64_WritePage(address, (uint8_t *)&record, sizeof(record)) != W25Q64_OK) ||
        (W25Q64_Read(address, (uint8_t *)&verify, sizeof(verify)) != W25Q64_OK) ||
        (CRC_Calc_Bytes(&hcrc, verify.Data, sizeof(verify.Data)) != verify.Crc32)) {
        xSemaphoreGive(W25Q64_Mutex);
        return W25Q64_ERROR;
    }

    xSemaphoreGive(W25Q64_Mutex);
    return W25Q64_OK;
}


static uint32_t W25Q64_FindWriteAddress(void)
{
    uint8_t slot[W25Q64_RECORD_SIZE];

    for (uint32_t addr = W25Q64_LOG_START;
         addr < W25Q64_LOG_END;
         addr += W25Q64_RECORD_SIZE) {

        if (W25Q64_Read(addr, slot, sizeof(slot)) != W25Q64_OK) {
            return UINT32_MAX;  // 读取失败
        }

        uint32_t i;
        for (i = 0; i < sizeof(slot); i++) {
            if (slot[i] != 0xFFU) {
                break;          // 这个格子写过，继续看下一个
            }
        }

        if (i == sizeof(slot)) {
            return addr;        // 64 字节全是 FF，找到可写位置
        }
    }

    return UINT32_MAX;          // 整个日志区都没有空格子
}



void W25Q64_Task(void *arg)
{

    W25Q64_Record_t Error_Data;
    W25Q64_Record_t Read_Data;
    uint32_t Error_Count = 0;
    xSemaphoreTake(W25Q64_Mutex, portMAX_DELAY);
    Error_Data.Head = W25Q64_RECORD_HEAD;
    Error_Data.Tail = W25Q64_RECORD_TAIL;
    WriteAdd_Offset = W25Q64_FindWriteAddress();
    if (WriteAdd_Offset == UINT32_MAX) {
        WriteAdd_Offset = W25Q64_LOG_START;
        
        if (W25Q64_EraseSector(WriteAdd_Offset) != W25Q64_OK) {
            xSemaphoreGive(W25Q64_Mutex);
            vTaskDelete(NULL);
            return;
        }
        
    }
    xSemaphoreGive(W25Q64_Mutex);
    while (1) {
        xQueueReceive(W25Q64_Handle, &Error_Data.Data, portMAX_DELAY);
        xSemaphoreTake(W25Q64_Mutex, portMAX_DELAY);

        Error_Data.CRC_Data = CRC_Calc_Bytes(&hcrc, (uint8_t*)&Error_Data.Data, sizeof(Error_Data.Data));
        if (W25Q64_WritePage(WriteAdd_Offset, (uint8_t*)&Error_Data, sizeof(Error_Data)) == W25Q64_OK) {
            
        }else {
            Error_Count++;
            xSemaphoreGive(W25Q64_Mutex);
            continue;
        }

        if (W25Q64_Read(WriteAdd_Offset, (uint8_t*)&Read_Data, sizeof(Read_Data)) != W25Q64_OK) {
            Error_Count++;
            WriteAdd_Offset = W25Q64_FindWriteAddress();
            xSemaphoreGive(W25Q64_Mutex);
            continue;
        }
    
        if (Read_Data.CRC_Data != Error_Data.CRC_Data) {
                Error_Count++;
                WriteAdd_Offset = W25Q64_FindWriteAddress();
                xSemaphoreGive(W25Q64_Mutex);
                continue;
            }

        WriteAdd_Offset += W25Q64_RECORD_SIZE;

        if ((WriteAdd_Offset % W25Q64_SECTOR_SIZE) == 0) {
            W25Q64_EraseSector(WriteAdd_Offset);
        }
        if (WriteAdd_Offset >= W25Q64_LOG_END) {

            WriteAdd_Offset = 0x0;
            W25Q64_EraseSector( WriteAdd_Offset);
        
        }

        xSemaphoreGive(W25Q64_Mutex);

    }

}
