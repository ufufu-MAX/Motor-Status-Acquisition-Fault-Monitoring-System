#ifndef __CLI_H_
#define __CLI_H_

#include <stdint.h>

#define CLI_LINE_MAX  64U
#define RESP_HELP     128
#define RESP_NORMAL   32

typedef enum
{
    CLI_CMD_HELP = 0,
    CLI_CMD_DATA,
    CLI_CMD_RTC,
    CLI_CMD_FAULT,
    CLI_CMD_STATUS,
    CLI_CMD_STREAM_ON,
    CLI_CMD_STREAM_OFF,
    CLI_CMD_UNKNOW,
}CLI_Command_t;

typedef struct
{
    uint8_t Help_buffer[RESP_HELP];
    uint8_t Help_BufferLen;
    uint8_t Rtc_Buffer[RESP_NORMAL];
    uint8_t Fault_Buffer[RESP_NORMAL];
    uint8_t Status_Buffer[RESP_NORMAL];
    uint8_t Stream_status;
    uint8_t Normal_Len;
}CLI_Resp_t;

typedef struct
{
    uint8_t Mask;
    const char *Fault_Name;
}Fault_Text_t;

HAL_StatusTypeDef CLI_Init(void);


void CLI_Task(void *arg);

HAL_StatusTypeDef Uart_Tx(const uint8_t *Data,uint8_t len);

#endif
