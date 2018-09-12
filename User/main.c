#include "Board.h"
#include "derivative.h"
#include "HAL.h"
#include "idDetection.h"
#include "TMCL.h"
#include "VitalSignsMonitor.h"
#include "BoardAssignment.h"

EvalboardsTypeDef Evalboards;

const char *VersionString = MODULE_ID"V303"; // module id and version of the firmware shown in the TMCL-IDE

/* Check if jumping into bootloader is forced                                           */
/*                                                                                      */
/* In order to jump to bootloader e.g. because of an accidental infinite loop           */
/* in a modified firmware you may short ID_CLK and ID_CH0 pins on start up.             */
/* This will force the entrance into bootloader mode and allow to replace bad firmware. */
void shallForceBoot()
{
	// toggle each pin and see if you can read the state on the other
	// leave if not, because this means that the pins are not tied together
	HAL.IOs->config->toOutput(&HAL.IOs->pins->ID_CLK);
	HAL.IOs->config->toInput(&HAL.IOs->pins->ID_CH0);

	HAL.IOs->config->setHigh(&HAL.IOs->pins->ID_CLK);
	if(!HAL.IOs->config->isHigh(&HAL.IOs->pins->ID_CH0))
		return;

	HAL.IOs->config->setLow(&HAL.IOs->pins->ID_CLK);
	if(HAL.IOs->config->isHigh(&HAL.IOs->pins->ID_CH0))
		return;

	HAL.IOs->config->toOutput(&HAL.IOs->pins->ID_CH0);
	HAL.IOs->config->toInput(&HAL.IOs->pins->ID_CLK);

	HAL.IOs->config->setHigh(&HAL.IOs->pins->ID_CH0);
	if(!HAL.IOs->config->isHigh(&HAL.IOs->pins->ID_CLK))
		return;

	HAL.IOs->config->setLow(&HAL.IOs->pins->ID_CH0);
	if(HAL.IOs->config->isHigh(&HAL.IOs->pins->ID_CLK))
		return;

	// not returned, this means pins are tied together
	tmcl_boot();
}

/* Call all standard initialization routines. */
static void init()
{
	IdAssignmentTypeDef ids;
	
	HAL.init();                  // Initialize Hardware Abstraction Layer
	
	RS232.txN("test for usart1...",18);
    UART.txN("test for usart3...",18);
	#if 1
	IDDetection_init();          // Initialize board detection
	tmcl_init();                 // Initialize TMCL communication

	tmcdriver_init();            // Initialize dummy driver board --> preset EvalBoards.ch2
	tmcmotioncontroller_init();  // Initialize dummy motion controller board  --> preset EvalBoards.ch1

	VitalSignsMonitor.busy = 1;  // Put state to busy
	Evalboards.driverEnable = DRIVER_ENABLE;
	Evalboards.ch1.id = 0;       // preset id for driver board to 0 --> error/not found
	Evalboards.ch2.id = 0;       // preset id for driver board to 0 --> error/not found

	// We disable the drivers before configurating anything
	//V_EN
	HAL.IOs->config->toOutput(&HAL.IOs->pins->DIO0);
	HAL.IOs->config->setHigh(&HAL.IOs->pins->DIO0);
    //H_EN
	HAL.IOs->config->toOutput(&HAL.IOs->pins->DIO4);
	HAL.IOs->config->setHigh(&HAL.IOs->pins->DIO4);

	IDDetection_initialScan(&ids);  // start initial board detection
	IDDetection_initialScan(&ids);  // start second time, first time not 100% reliable, not sure why - too fast after startup?
	if(!ids.ch1.id && !ids.ch2.id)
	{
		shallForceBoot();           // only checking to force jump into bootloader if there are no boards attached
		// todo CHECK 2: Workaround: shallForceBoot() changes pin settings - change them again here, since otherwise IDDetection partially breaks (LH)
		HAL.IOs->config->toOutput(&HAL.IOs->pins->ID_CLK);
		HAL.IOs->config->toInput(&HAL.IOs->pins->ID_CH0);
    }
	Board_assign(&ids);             // assign boards with detected id

	VitalSignsMonitor.busy 	= 0;    // not busy any more!
		#endif
}

int main(void)
{
	// Start all initialization routines
	init();
	while(1)
	{
//		// Check all parameters and life signs and mark errors
//		vitalsignsmonitor_checkVitalSigns();

//		// Perodic jobs of Motion controller/Driver boards
//		Evalboards.ch1.periodicJob(systick_getTick());
//		Evalboards.ch2.periodicJob(systick_getTick());

//		// Process TMCL communication
		tmcl_process();

        LED_ON();
        delay_ms(250);
        LED_OFF();
        delay_ms(250);

        handle_all_key();
	}
#ifdef UNRET
	return 0;
#endif
}
