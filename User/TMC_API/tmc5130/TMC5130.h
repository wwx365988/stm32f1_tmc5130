/*
 * TMC5130.h
 *
 *  Created on: 03.07.2017
 *      Author: LK
 */

#ifndef API_IC_TMC5130_H
#define API_IC_TMC5130_H

	#include "../helpers/API_Header.h"
	#include "TMC5130_Register.h"
	#include "TMC5130_Mask_Shift.h"

	#define REGISTER_COUNT 128

	#define TMC5130_FIELD_READ(motor, address, mask, shift) \
		FIELD_READ(tmc5130_readInt, motor, address, mask, shift)
	#define TMC5130_FIELD_WRITE(motor, address, mask, shift, value) \
		FIELD_WRITE(tmc5130_writeInt, motor, address, mask, shift, value)
	#define TMC5130_FIELD_UPDATE(motor, address, mask, shift, value) \
		FIELD_UPDATE(tmc5130_readInt, tmc5130_writeInt, motor, address, mask, shift, value)

	//Factor between 10ms units and internal units for 16MHz
	#define TPOWERDOWN_FACTOR (4.17792*100.0/255.0)
	// TPOWERDOWN_FACTOR = k * 100 / 255 where k = 2^18 * 255 / fClk for fClk = 16000000)

	typedef struct
	{
		int velocity, oldX;
		uint32 oldTick;
		int32 registerResetState[REGISTER_COUNT];
		uint8 registerAccess[REGISTER_COUNT];
	} TMC5130TypeDef;

	void tmc5130_initConfig(TMC5130TypeDef *tmc5130);
	void tmc5130_periodicJob(u8 motor, uint32 tick, TMC5130TypeDef *tmc5130, ConfigurationTypeDef *TMC5130_config);
	u8 tmc5130_reset(ConfigurationTypeDef *TMC5130_config);
	u8 tmc5130_restore(ConfigurationTypeDef *TMC5130_config);

#endif /* API_IC_TMC5130_H */
