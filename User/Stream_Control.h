#ifndef __STREAM_CONTROL_H_
#define __STREAM_CONTROL_H_

#include <stdint.h>

void Stream_Control_Set(uint8_t Enable);

uint8_t Stream_Control_IsEnable(void);

#endif