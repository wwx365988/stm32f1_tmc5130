#include "TMCMotionController.h"

EvalBoardMotionControllerTypeDef TMCMotionController =
{
	.config	=
	{
		.state           = CONFIG_READY,
		.configIndex     = 0,
		.shadowRegister  = { 0 }
	}
};

void tmcmotioncontroller_init()
{
	Evalboards.ch1.config               = &TMCMotionController.config;
	Evalboards.ch1.config->state        = CONFIG_READY;
	Evalboards.ch1.config->configIndex  = 0;

	Evalboards.ch1.numberOfMotors       = 0;
	Evalboards.ch1.VMMin                = 0;
	Evalboards.ch1.VMMax                = -1;
	Evalboards.ch1.errors               = 0;

	board_setDummyFunctions(&Evalboards.ch1);
}
