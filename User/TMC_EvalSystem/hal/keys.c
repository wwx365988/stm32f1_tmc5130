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
	HAL.IOs->config->reset(&HAL.IOs->pins->KEY_V_ZERO);
	HAL.IOs->config->reset(&HAL.IOs->pins->KEY_LD);
    HAL.IOs->config->reset(&HAL.IOs->pins->KEY_RU);
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

void handle_all_key(void)
{
    extern uint8_t vInKeyStatic;
	if ((vInKeyStatic & 0x2) > 0)      /* 按住 KEY_V_ZERO复位,STM32烧写 */
	{
        vInKeyStatic &= ~(0x2);
        GPIOC->ODR ^= GPIO_Pin_4;
        //do_something1();
    }
    else if ((vInKeyStatic & 0x4) > 0)  /* 按住KEY_LD复位,水平手动模式 */
	{
        vInKeyStatic &= ~(0x4);
        //do_something2();
    }
    else if ((vInKeyStatic & 0x8) > 0)  /* 按住KEY_RU复位,垂直手动模式 */
	{
        vInKeyStatic &= ~(0x8);
        //do_something3();
    }
}


