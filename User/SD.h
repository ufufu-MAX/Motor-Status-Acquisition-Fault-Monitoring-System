#ifndef __SD_H_
#define __SD_H_


#include <stdint.h>

extern volatile uint8_t SD_Online;

void SD_Task(void *arg);

#endif
