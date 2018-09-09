/*
 * TMC5130.c
 *
 *  Created on: 03.07.2017
 *      Author: LK
 */

#include "TMC5130.h"

// Default Register Values
#define R10 0x00071703  // IHOLD_IRUN
#define R3A 0x00010000  // ENC_CONST
#define R60 0xAAAAB554  // MSLUT[0]
#define R61 0x4A9554AA  // MSLUT[1]
#define R62 0x24492929  // MSLUT[2]
#define R63 0x10104222  // MSLUT[3]
#define R64 0xFBFFFFFF  // MSLUT[4]
#define R65 0xB5BB777D  // MSLUT[5]
#define R66 0x49295556  // MSLUT[6]
#define R67 0x00404222  // MSLUT[7]
#define R68 0xFFFF8056  // MSLUTSEL
#define R69 0x00F70000  // MSLUTSTART
#define R6C 0x000101D5  // CHOPCONF
#define R70 0x000504C8  // PWMCONF

/* Register access permissions:
 * 0: none (reserved)
 * 1: read
 * 2: write
 * 3: read/write
 * 7: read^write (seperate functions/values)
 */
const u8 tmc5130_defaultRegisterAccess[REGISTER_COUNT] =
{
//	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F
	3, 1, 1, 2, 7, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x00 - 0x0F
	2, 2, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10 - 0x1F
	3, 3, 1, 2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 3, 0, 0, // 0x20 - 0x2F
	0, 0, 0, 2, 3, 1, 1, 0, 3, 3, 2, 1, 1, 0, 0, 0, // 0x30 - 0x3F
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x40 - 0x4F
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x50 - 0x5F
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 2, 2, 1, // 0x60 - 0x6F
	2, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 0x70 - 0x7F
};

const s32 tmc5130_defaultRegisterResetState[REGISTER_COUNT] =
{
//	0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   A,   B,   C,   D,   E,   F
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 0x00 - 0x0F
	R10, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 0x10 - 0x1F
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 0x20 - 0x2F
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   R3A, 0,   0,   0,   0,   0, // 0x30 - 0x3F
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 0x40 - 0x4F
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 0x50 - 0x5F
	R60, R61, R62, R63, R64, R65, R66, R67, R68, R69, 0,   0,   R6C, 0,   0,   0, // 0x60 - 0x6F
	R70, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 0x70 - 0x7F
};

// => SPI wrapper
extern void tmc5130_writeDatagram(uint8 motor, uint8 address, uint8 x1, uint8 x2, uint8 x3, uint8 x4);
extern void tmc5130_writeInt(uint8 motor, uint8 address, int value);
extern int tmc5130_readInt(u8 motor, uint8 address);
// <= SPI wrapper

void tmc5130_initConfig(TMC5130TypeDef *tmc5130)
{
	int i;
	tmc5130->velocity  = 0;
	tmc5130->oldTick   = 0;
	tmc5130->oldX      = 0;

	for(i = 0; i < REGISTER_COUNT; i++)
	{
		tmc5130->registerAccess[i]      = tmc5130_defaultRegisterAccess[i];
		tmc5130->registerResetState[i]  = tmc5130_defaultRegisterResetState[i];
	}
};

void tmc5130_writeConfiguration(u8 motor, TMC5130TypeDef *tmc5130, ConfigurationTypeDef *TMC5130_config)
{
	uint8 *ptr = &TMC5130_config->configIndex;
	const int32 *settings = (TMC5130_config->state == CONFIG_RESTORE) ? TMC5130_config->shadowRegister : tmc5130->registerResetState;

	while((*ptr < REGISTER_COUNT) && !IS_WRITEABLE(tmc5130->registerAccess[*ptr]))
		(*ptr)++;

	if(*ptr < REGISTER_COUNT)
	{
		tmc5130_writeInt(motor, *ptr, settings[*ptr]);
		(*ptr)++;
	}
	else
	{
		TMC5130_config->state = CONFIG_READY;
	}
}

void tmc5130_periodicJob(u8 motor, uint32 tick, TMC5130TypeDef *tmc5130, ConfigurationTypeDef *TMC5130_config)
{
	int XActual;
	uint32 tickDiff;
	
	if(TMC5130_config->state != CONFIG_READY)
	{
		tmc5130_writeConfiguration(motor, tmc5130, TMC5130_config);
		return;
	}

	if((tickDiff = tick-tmc5130->oldTick) >= 5)	// measured speed dx/dt
	{
		XActual = tmc5130_readInt(motor, TMC5130_XACTUAL);
		TMC5130_config->shadowRegister[TMC5130_XACTUAL] = XActual;
		tmc5130->velocity = (int) ((float) ((XActual-tmc5130->oldX) / (float) tickDiff) * (float) 1048.576);

		tmc5130->oldX     = XActual;
		tmc5130->oldTick  = tick;
	}
}

uint8 tmc5130_reset(ConfigurationTypeDef *TMC5130_config)
{
	if(TMC5130_config->state != CONFIG_READY)
		return 0;

	TMC5130_config->state        = CONFIG_RESET;
	TMC5130_config->configIndex  = 0;

	return 1;
}

uint8 tmc5130_restore(ConfigurationTypeDef *TMC5130_config)
{
	if(TMC5130_config->state != CONFIG_READY)
		return 0;

	TMC5130_config->state        = CONFIG_RESTORE;
	TMC5130_config->configIndex  = 0;

	return 1;
}
