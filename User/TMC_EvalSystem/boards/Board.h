#ifndef BOARD_H
#define BOARD_H

	#include "helpers/API_Header.h"

	#include "../hal/derivative.h"
	#include "../hal/HAL.h"
	#include "../tmc/VitalSignsMonitor.h"

	// Evalboard channel struct
	typedef struct
	{
		uint8  id;
		uint32 errors;
		uint32 VMMax;
		uint32 VMMin;
		unsigned char numberOfMotors;
		ConfigurationTypeDef *config;
		uint32 (*left)              (uint8 motor, int32 velocity);            // move left with velocity <velocity>
		uint32 (*right)             (uint8 motor, int32 velocity);            // move right with velocity <velocity>
		uint32 (*rotate)            (uint8 motor, int32 velocity);            // move right with velocity <velocity>
		uint32 (*stop)              (uint8 motor);                            // stop motor
		uint32 (*moveTo)            (uint8 motor, int32 position);            // move to position <position>
		uint32 (*moveBy)            (uint8 motor, int32 *ticks);              // move by <ticks>, changes ticks to absolute target
		uint32 (*moveProfile)       (uint8 motor, int32 position);            // move profile <position>
		uint32 (*SAP)               (uint8 type, uint8 motor, int32 value);   // set axis parameter -> TMCL conformance
		uint32 (*GAP)               (uint8 type, uint8 motor, int32 *value);  // get axis parameter -> TMCL conformance
		uint32 (*STAP)              (uint8 type, uint8 motor, int32 value);   // store axis parameter -> TMCL conformance
		uint32 (*RSAP)              (uint8 type, uint8 motor, int32 value);   // restore axis parameter -> TMCL conformance
		void (*readRegister)        (uint8 address, int32 *value);
		void (*writeRegister)       (uint8 address, int32 value);
		uint32 (*getMeasuredSpeed)  (uint8 motor, int32 *value);
		uint32 (*userFunction)      (uint8 type, uint8 motor, int32 *value);

		void (*periodicJob)         (uint32 tick);
		void (*deInit)              (void);

		void (*checkErrors)         (uint32 tick);
		void (*enableDriver)        (DriverState state);

		uint8 (*cover)              (uint8 data, uint8 lastTransfer);
	} EvalboardFunctionsTypeDef;

	// Evalboard errors
	typedef enum {
		ERROR_NONE		= 0x00,
		ERROR_GENERIC	= 0x01,
		ERROR_FUNCTION	= 0x02,
		ERROR_TYPE		= 0x04,
		ERROR_ADDRESS	= 0x04,	// todo CHECK 3: Soll das so? Habe es aus der const struct übernommen (LH)
		ERROR_MOTOR		= 0x08,
		ERROR_VALUE		= 0x10,
		ERROR_NOT_DONE	= 0x20
	} EvalboardErrorBit;

	// struct for our Evalsystem, with two available Evalboard channels
	typedef struct
	{
		EvalboardFunctionsTypeDef ch1;
		EvalboardFunctionsTypeDef ch2;
		DriverState driverEnable; // global driver status
	} EvalboardsTypeDef;

	extern EvalboardsTypeDef Evalboards;

	void periodicJobDummy(uint32 tick);
	void board_setDummyFunctions(EvalboardFunctionsTypeDef *channel);

	#include "TMCDriver.h"
	#include "TMCMotionController.h"

#endif /* BOARD_H */
