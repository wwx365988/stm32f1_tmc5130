#include "TMCDriver.h"

//EvalBoardDriverTypeDef TMCDriver;
EvalBoardDriverTypeDef TMCDriver =
{
	.config	=
	{
		.state           = CONFIG_READY,
		.configIndex     = 0,
		.shadowRegister  = { 0 }
	}
};

void tmcdriver_init()
{
	Evalboards.ch2.config               = &TMCDriver.config;
	Evalboards.ch2.config->state        = CONFIG_READY;
	Evalboards.ch2.config->configIndex  = 0;

	Evalboards.ch2.numberOfMotors       = 0;
	Evalboards.ch2.VMMin                = 0;
	Evalboards.ch2.VMMax                = -1;
	Evalboards.ch2.errors               = 0;

	board_setDummyFunctions(&Evalboards.ch2);
}
