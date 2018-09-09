#include <stdlib.h>
#include "HAL.h"
#include "LEDs.h"

static void init(void);
static void onStat(void);
static void onError(void);
static void offStat(void);
static void offError(void);
static void toggleStat(void);
static void toggleError(void);

LEDsTypeDef LEDs =
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
	HAL.IOs->config->reset(&HAL.IOs->pins->LED_STAT);
	HAL.IOs->config->reset(&HAL.IOs->pins->LED_ERROR);
	LED_OFF();
	LED_ERROR_OFF();
}

static void onStat()
{
	LED_ON();
}

static void onError()
{
	LED_ERROR_ON();
}

static void offStat()
{
	LED_OFF();
}

static void offError()
{
	LED_ERROR_OFF();
}

static void toggleStat()
{
	LED_TOGGLE();
}

static void toggleError()
{
	LED_ERROR_TOGGLE();
}
