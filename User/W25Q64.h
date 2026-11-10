#ifndef __W25Q64_H_
#define __W25Q64_H_

#include "ADC_DATA.h"
#include <stdint.h>

#define W25Q64_PAGE_SIZE       256U
#define W25Q64_SECTOR_SIZE     4096U
#define W25Q64_TOTAL_SIZE      0x800000UL
#define W25Q64_TIMEOUT_MS       1000U

#define W25Q64_CMD_WRITE_ENABLE 0x06U
#define W25Q64_CMD_READ_STATUS  0x05U
#define W25Q64_CMD_SECTOR_ERASE 0x20U
#define W25Q64_CMD_PAGE_PROGRAM 0x02U
#define W25Q64_CMD_READ_DATA    0x03U

#define W25Q64_STATUS_BUSY      0x01U
#define W25Q64_STATUS_WEL       0x02U

#define W25Q64_RECORD_HEAD 0xA55AA55AU
#define W25Q64_RECORD_TAIL 0x55AA55AAU
#define W25Q64_RECORD_SIZE 64U

#define W25Q64_LOG_END     0x700000UL
#define W25Q64_LOG_START   0x000000UL
#define W25Q64_THRESHOLD_START 0x7FF000UL
#define W25Q64_THRESHOLD_SLOT  64U

typedef enum
{
    W25Q64_OK = 0,
    W25Q64_ERROR,
    W25Q64_BUSY,
    W25Q64_BAD_PARAMETER
}W25Q64_Status_t;

typedef struct
{

    uint32_t Head;
    Motor_Data_t Data;
    uint32_t CRC_Data;
    uint32_t Tail;

}W25Q64_Record_t;

typedef struct
{
    uint8_t Data[60];
    uint32_t Crc32;
} W25Q64_ThresholdRecord_t;

W25Q64_Status_t W25Q64_EraseSector(uint32_t address);
W25Q64_Status_t W25Q64_WritePage(uint32_t address, uint8_t *data, uint16_t length);
W25Q64_Status_t W25Q64_Read(uint32_t address, uint8_t *data, uint32_t length);


W25Q64_Status_t W25Q64_Init(void);
W25Q64_Status_t W25Q64_ConfigLoad(void);
W25Q64_Status_t W25Q64_ConfigSave(const ADC_Thresholds_t *thresholds);


void W25Q64_Task(void *arg);

#endif
