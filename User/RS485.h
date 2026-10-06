#ifndef __RS485_H_
#define __RS485_H_

#include <stdint.h>
#define RS485_SLAVE_ADDRESS  1U

typedef struct
{
    uint8_t Data[32];
    uint16_t len;

}RS485_REC_t;

void RS485_SetTx();

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

/* 0x03 reads thresholds; 0x06 writes one threshold (RAM only).
 * Address  Value                         Unit
 * 0x0100   Current_Trip                  A * 100
 * 0x0101   Current_Relsase               A * 100
 * 0x0102   Temp_Max_Trip                 C * 100
 * 0x0103   Temp_Max_Release              C * 100
 * 0x0104   Motor_Speed_Stall_Trip        RPM (0..255)
 * 0x0105   Motor_Speed_Stall_Release     RPM (0..255)
 * 0x0106   Temp_Min_Trip                 C * 100
 * 0x0107   Temp_Min_Release              C * 100
 * 0x0108   Volt_Trip                     V * 100
 * 0x0109   Volt_Relsase                  V * 100
 * 0x010A   Motor_Speed_Min_Trip          RPM
 * 0x010B   Motor_Speed_Min_Release       RPM
 * 0x010C   Motor_Speed_Max_Trip          RPM
 * 0x010D   Motor_Speed_Max_Release       RPM
 * Scaled uint8_t thresholds accept multiples of 100; RPM values are unscaled.
 * Volt_Trip accepts 0.01 V.
 * Write: 01 06 01 00 03 20 89 1E (8 A; success echoes this frame).
 * Read:  01 03 01 00 00 0E C5 F2 (all 14 thresholds).
 * Exceptions: 01 = function, 02 = address, 03 = value/length/count.
 */

#endif
