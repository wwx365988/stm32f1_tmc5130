#ifndef SysTick_H
#define SysTick_H

#include "API_Header.h"

//后面将全局变量优化,保留
typedef struct
{
    uint16 g_run_count;
    uint8  g_second;
    struct  
    {  
        unsigned int flag_bit0:1;  
        unsigned int flag_bit1:1;  
        unsigned int flag_bit2:1;  
        unsigned int flag_bit3:1;  
        unsigned int flag_bit4:1;  
        unsigned int flag_bit5:1;  
        unsigned int flag_bit6:1;  
        unsigned int flag_bit7:1;  
    }bit;
}SYSTICK_TYPEDEF;

typedef enum
{
    KEY_ZERO = 0x2,
    KEY_LD = 0x4,
    KEY_RU = 0x8,
}KEY_VALUE;
void delay_ms(uint16 das);
void systick_task(void);
void SysTick_Init(void);
void systick_init(void);

#endif /* SysTick_H */
