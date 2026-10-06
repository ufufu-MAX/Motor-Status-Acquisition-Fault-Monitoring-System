#ifndef __RS485_H_
#define __RS485_H_

#include <stdint.h>
#define RS485_SLAVE_ADDRESS  1U

typedef struct
{
    uint8_t Data[32];
    uint16_t len;

}RS485_REC_t;



void RS485_Task(void *arg);

void RS485_SetReceive(void);

/*| Modbus 寄存器地址 | 对应的数据 |

| `0x0000` | 数据序号高 16 位 |
| `0x0001` | 数据序号低 16 位 |
| `0x0002` | 转速 RPM |
| `0x0003` | 温度 × 100 |
| `0x0004` | 电压 × 100 |
| `0x0005` | 电流 × 100 |
| `0x0006` | 故障码 |*/

#endif