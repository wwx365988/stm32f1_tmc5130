#include "derivative.h"
#include "HAL.h"

#define AIRCR_VECTKEY_MASK ((uint32)0x05FA0000)

static void init(void);
static void reset(uint8 ResetPeripherals);
static void NVIC_DeInit(void);

static const IOsFunctionsTypeDef IOFunctions =
{
	.config  = &IOs,
	.pins    = &IOMap,
};

const HALTypeDef HAL =
{
	.init         = init,
	.reset        = reset,
	.NVIC_DeInit  = NVIC_DeInit,
	.SPI          = &SPI,
	.LEDs         = &LEDs,
	.KEYs         = &KEYs,
	.ADCs         = &ADCs,
	.IOs          = &IOFunctions,
	.RS232        = &RS232,
	.Timer        = &Timer,
	.UART         = &UART
};

static void init(void)
{
	__enable_irq();

	systick_init();//后面再来研究一下如何配置
    delay_ms(100);

	IOs.init();
	IOMap.init();
//	SPI.init();
	RS232.init();
    UART.init();
//	LEDs.init();
//  KEYs.init();
//	ADCs.init();
}

static void __attribute((noreturn)) reset(uint8 ResetPeripherals)
{
	// Disable interrupts
	__disable_irq();

	if(ResetPeripherals)
		SCB->AIRCR = AIRCR_VECTKEY_MASK | SCB_AIRCR_SYSRESETREQ_Msk;
	else
		SCB->AIRCR = AIRCR_VECTKEY_MASK | SCB_AIRCR_VECTRESET_Msk;

	// SYSRESETREQ does not happen instantly since peripheral reset timing is not specified.
	// Trap execution here so nothing else happens until the reset completes.
	while(1);
}

static void NVIC_DeInit(void)
{
	uint32 index;

	for(index = 0; index < 8; index++)
	{
		NVIC->ICER[index] = 0xFFFFFFFF;
		NVIC->ICPR[index] = 0xFFFFFFFF;
	}

	for(index = 0; index < 240; index++)
	{
		 NVIC->IP[index] = 0x00000000;
	}
}

void _exit(int i)	// function has the attribute noreturn per default
{
	UNUSED(i);
	while(1) {};
}

void _kill(void)
{
}

void _getpid(void)
{
}
