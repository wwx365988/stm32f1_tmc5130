#include <stdlib.h>
#include "HAL.h"
#include "keys.h"

static void init(void);
static void onStat(void);
static void onError(void);
static void offStat(void);
static void offError(void);
static void toggleStat(void);
static void toggleError(void);

KEYsTypeDef KEYs =
{
	.init  = init,
	.stat  =
	{
		.on      = onStat,
		.off     = offStat,
		.toggle  = toggleStat,
	},
	.error =
	{
		.on      = onError,
		.off     = offError,
		.toggle  = toggleError,
	},
};

static void init()
{
	HAL.IOs->config->reset(&HAL.IOs->pins->KEY_STAT);
	HAL.IOs->config->reset(&HAL.IOs->pins->KEY_ERROR);
	KEY_OFF();
	KEY_ERROR_OFF();
}

static void onStat()
{
	KEY_ON();
}

static void onError()
{
	KEY_ERROR_ON();
}

static void offStat()
{
	KEY_OFF();
}

static void offError()
{
	KEY_ERROR_OFF();
}

static void toggleStat()
{
	KEY_TOGGLE();
}

static void toggleError()
{
	KEY_ERROR_TOGGLE();
}

/// 不精确的延时
static void Key_Delay(__IO u32 nCount)
{
	for(; nCount != 0; nCount--);
} 


unsigned char Key_Scan(GPIO_TypeDef* GPIOx,u16 GPIO_Pin)
{			
	/*检测是否有按键按下 */
	if(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == 0 ) 
	{	   
		/*延时消抖*/
		Key_Delay(10000);		
		if(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == 0 )  
		{	 
			/*等待按键释放 */
			while(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == 0);   
			return 	0;	 
		}
		else
			return 1;
	}
	else
		return 1;
}

