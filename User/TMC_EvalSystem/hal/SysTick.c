#include "HAL.h"
#include "SysTick.h"

static __IO u32 TimingDelay;
volatile u32 systick = 0;

uint16_t g_run_cnt       = 0;
uint8_t  g_second        = 0;
uint8_t  g_key_ld        = 0;
uint8_t  g_key_ld_cnt    = 0;
uint8_t  g_key_ru        = 0;
uint8_t  g_key_ru_cnt    = 0;
uint8_t  g_key_zero      = 0;
uint8_t  g_key_zero_cnt  = 0;

#define  KEY_V_ZERO GPIO_Pin_13
#define  KEY_V_LD   GPIO_Pin_14
#define  KEY_V_RU   GPIO_Pin_15

#define KEY_SHORT_PRESS   10   //10*2ms=20ms
#define KEY_LONG_PRESS    750  //750*2ms=1.5S

uint8_t vInKeyStatic = 0;

void __attribute__ ((interrupt)) SysTick_Handler(void);

void SysTick_Handler(void)
{
	systick_task();
}

void systick_init(void)
{
    /* SystemFrequency / 1000    1ms中断一次
     * SystemFrequency / 100000  10us中断一次
     * SystemFrequency / 1000000 1us中断一次
     */
    if (SysTick_Config(SystemCoreClock / 500))   // ST3.5.0库版本,2ms进入一次滴答中断
    { 
        /* Capture error */ 
        while (1);
    }
    // 关闭滴答定时器  
//    SysTick->CTRL &= ~ SysTick_CTRL_ENABLE_Msk;

}

u32 systick_getTick()
{
	return systick;
}

void delay_ms(uint32 delay)
{
#ifdef __EBOS__
  	TimingDelay = delay;
	
  	while(TimingDelay > 0)
    {
		    ;
	}
#endif
    uint32 startTick = systick;
	while((systick-startTick) <= delay) {}
}

void key_scan(void)
{
    if ((GPIOC->IDR&KEY_V_ZERO) == 0)  //KEY_V_ZERO按下
    {
        if (g_key_zero == 0) //如果最初标记为0
        {	
            if (g_key_zero_cnt >= KEY_SHORT_PRESS) //计数值达到一定值后才认为真正按下(消除机械抖动)
            {
                    g_key_zero = 1;//按键按下标志
                    vInKeyStatic |= KEY_ZERO;//按键值,自定义
            }
            else 
            {
                    g_key_zero_cnt++;	//滴答定时器计数
            }
        }
    }
    else   //KEY_V_ZERO松开
    {
            g_key_zero = 0;
            g_key_zero_cnt  = 0;
    }
    
    if ((GPIOC->IDR&KEY_LD) == 0)  //KEY_LD按下
    {
        if (g_key_ld == 0) //如果最初标记为0
        {	
            if (g_key_ld_cnt >= KEY_SHORT_PRESS) //计数值达到一定值后才认为真正按下(消除机械抖动)
            {
                    g_key_ld = 1;//按键按下标志
                    vInKeyStatic |= KEY_LD;//按键值,自定义
            }
            else 
            {
                    g_key_ld_cnt++;	//滴答定时器计数
            }
        }
    }
    else   //KEY_V_ZERO松开
    {
            g_key_ld = 0;
            g_key_ld_cnt  = 0;
    }

    if ((GPIOC->IDR&KEY_RU) == 0)  //KEY_RU按下
    {
        if (g_key_ru == 0) //如果最初标记为0
        {	
            if (g_key_ru_cnt >= KEY_SHORT_PRESS) //计数值达到一定值后才认为真正按下(消除机械抖动)
            {
                    g_key_ru = 1;//按键按下标志
                    vInKeyStatic |= KEY_RU;//按键值,自定义
            }
            else 
            {
                    g_key_ru_cnt++;	//滴答定时器计数
            }
        }
    }
    else   //KEY_V_ZERO松开
    {
            g_key_ru = 0;
            g_key_ru_cnt  = 0;
    }
}

void systick_task(void)
{	
#ifdef __EBOS__
    if (TimingDelay > 0) 
    {
        TimingDelay--;		
    }
#endif
    systick++;

    g_run_cnt++;
    if(g_run_cnt == 250) 
    { 
        //GPIOC->ODR ^= GPIO_Pin_4;
        g_second = 1;
        g_run_cnt = 0;
    }
     
    key_scan();	

    WWDG_SetCounter(127);  // Update WWDG counter
}
