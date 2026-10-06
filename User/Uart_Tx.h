#ifndef __UART_TX_H_
#define __UART_TX_H_


#include <stdint.h>

typedef struct
{

    uint16_t len;
    uint8_t Tx_buffer[128];

}UartTx_Frame_s;


void UartTx_Init(void);

void Uart_Task(void *arg);

#endif