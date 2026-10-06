#include "main.h"
#include <stdint.h>
#include <stdio.h>

static volatile  uint8_t Stream_Enable = 0;

void Stream_Control_Set(uint8_t Enable)
{
    Stream_Enable = (Enable != 0) ? 1 : 0;

}
uint8_t Stream_Control_IsEnable()
{

    return Stream_Enable ;

}