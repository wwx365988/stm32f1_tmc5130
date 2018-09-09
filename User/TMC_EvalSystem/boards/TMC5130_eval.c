#include "../tmc/stepDir.h"
#include "Board.h"
#include "TMC5130.h"

#define TMC5130_MAX_VELOCITY      8388096
#define TMC5130_MAX_ACCELERATION  65535

#define ERRORS_VM        (1<<0)
#define ERRORS_VM_UNDER  (1<<1)
#define ERRORS_VM_OVER   (1<<2)

#define VM_MIN  50   // VM[V/10] min
#define VM_MAX  480  // VM[V/10] max

#define MOTORS         1 // number of motors for this board
#define DEFAULT_MOTOR  0

static uint32 vMaxModified = 0;

static uint32 right(uint8 motor, int32 velocity);
static uint32 left(uint8 motor, int32 velocity);
static uint32 rotate(uint8 motor, int32 velocity);
static uint32 stop(uint8 motor);
static uint32 moveTo(uint8 motor, int32 position);
static uint32 moveBy(uint8 motor, int32 *ticks);
static uint32 GAP(uint8 type, uint8 motor, int32 *value);
static uint32 SAP(uint8 type, uint8 motor, int32 value);
static void readRegister(uint8 address, int32 *value);
static void writeRegister(uint8 address, int32 value);
static uint32 getMeasuredSpeed(uint8 motor, int32 *value);

static void periodicJob(uint32 tick);
static void checkErrors(uint32 tick);
static void deInit(void);
static uint32 userFunction(uint8 type, uint8 motor, int32 *value);

static uint8 reset(void);
static void enableDriver(DriverState state);

SPIChannelTypeDef *TMC5130_SPIChannel[MOTORS];
TMC5130TypeDef TMC5130[MOTORS];
ConfigurationTypeDef *TMC5130_config[MOTORS];

// => SPI wrapper
void tmc5130_writeDatagram(uint8 motor, uint8 address, uint8 x1, uint8 x2, uint8 x3, uint8 x4)
{
	int value;
	TMC5130_SPIChannel[motor]->readWrite(address | 0x80, FALSE);
	TMC5130_SPIChannel[motor]->readWrite(x1, FALSE);
	TMC5130_SPIChannel[motor]->readWrite(x2, FALSE);
	TMC5130_SPIChannel[motor]->readWrite(x3, FALSE);
	TMC5130_SPIChannel[motor]->readWrite(x4, TRUE);

	value = x1;
	value <<= 8;
	value |= x2;
	value <<= 8;
	value |= x3;
	value <<= 8;
	value |= x4;

	TMC5130_config[motor]->shadowRegister[address & 0x7F] = value;
}

void tmc5130_writeInt(uint8 motor, uint8 address, int value)
{
	tmc5130_writeDatagram(motor, address, 0xFF & (value>>24), 0xFF & (value>>16), 0xFF & (value>>8), 0xFF & (value>>0));
}

int tmc5130_readInt(u8 motor, uint8 address)
{
	int value;
	address &= 0x7F;

	// register not readable -> shadow register copy
	if(!IS_READABLE(TMC5130[motor].registerAccess[address]))
		return TMC5130_config[motor]->shadowRegister[address];

	TMC5130_SPIChannel[motor]->readWrite(address, FALSE);
	TMC5130_SPIChannel[motor]->readWrite(0, FALSE);
	TMC5130_SPIChannel[motor]->readWrite(0, FALSE);
	TMC5130_SPIChannel[motor]->readWrite(0, FALSE);
	TMC5130_SPIChannel[motor]->readWrite(0, TRUE);

	TMC5130_SPIChannel[motor]->readWrite(address, FALSE);
	value = TMC5130_SPIChannel[motor]->readWrite(0, FALSE);
	value <<= 8;
	value |= TMC5130_SPIChannel[motor]->readWrite(0, FALSE);
	value <<=	8;
	value |= TMC5130_SPIChannel[motor]->readWrite(0, FALSE);
	value <<= 8;
	value |= TMC5130_SPIChannel[motor]->readWrite(0, TRUE);

	return value;
}
// <= SPI wrapper

typedef struct
{
	IOPinTypeDef  *REFL_UC;
	IOPinTypeDef  *REFR_UC;
	IOPinTypeDef  *DRV_ENN_CFG6;
	IOPinTypeDef  *ENCA_DCIN_CFG5;
	IOPinTypeDef  *ENCB_DCEN_CFG4;
	IOPinTypeDef  *ENCN_DCO;

	IOPinTypeDef  *SWSEL;
	IOPinTypeDef  *SWN_DIAG0;
	IOPinTypeDef  *SWP_DIAG1;

	IOPinTypeDef  *AIN_REF_SW;
	IOPinTypeDef  *AIN_REF_PWM;
} PinsTypeDef;

static PinsTypeDef Pins;

static uint32 rotate(uint8 motor, int32 velocity)
{
	if(motor >= MOTORS)
		return ERROR_MOTOR;

	vMaxModified = TRUE;

	// set absolute velocity, independant from direction
	tmc5130_writeInt(motor, TMC5130_VMAX, abs(velocity));

	// signedness defines velocity mode direction bit in rampmode register

	tmc5130_writeDatagram(motor, TMC5130_RAMPMODE, 0, 0, 0, (velocity >= 0) ? TMC5130_MODE_VELPOS : TMC5130_MODE_VELNEG);

	return ERROR_NONE;
}

static uint32 right(uint8 motor, int32 velocity)
{
	return rotate(motor, velocity);
}

static uint32 left(uint8 motor, int32 velocity)
{
	return rotate(motor, -velocity);
}

static uint32 stop(uint8 motor)
{
	return rotate(motor, 0);
}

static uint32 moveTo(uint8 motor, int32 position)
{
	if(motor >= MOTORS)
		return ERROR_MOTOR;

	tmc5130_writeInt(motor, TMC5130_RAMPMODE, SD_RAMP_MODE_POSITIONING);
	tmc5130_writeInt(motor, TMC5130_VMAX, TMC5130_config[motor]->shadowRegister[TMC5130_VMAX]); // Not written to register while in rampVelocity mode -> write copy when switching
	tmc5130_writeInt(motor, TMC5130_XTARGET, position);

	return ERROR_NONE;
}

// TODO: wieso werden 'ticks' als Pointer �ben?  �be eines int32 sollte auch funktionieren (KW)
static uint32 moveBy(uint8 motor, int32 *ticks)
{
	if(motor >= MOTORS)
		return ERROR_MOTOR;

	// determine actual position and add numbers of ticks to move
	*ticks = tmc5130_readInt(motor, TMC5130_XACTUAL) + *ticks;

	return moveTo(motor, *ticks);
}

// \xml <?xml version="1.0" encoding="UTF-8" standalone="no"?>
// \xml <tmc:product_module author="BS" xmlns:tmc="http://www.trinamic.com" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.trinamic.com https://www.trinamic.com/fileadmin/xml/TMC_Schema.xsd">
// \xml <name>TMC5130-EVAL</name>
// \xml <name_fw_version>5130</name_fw_version>
// \xml <hardware_version>1.5</hardware_version>
// \xml <software_version>2.91</software_version>
static uint32 handleParameter(u8 readWrite, u8 motor, u8 type, int32 *value)
{
	uint32 buffer;
	u32 errors = ERROR_NONE;

	if(motor >= MOTORS)
		return ERROR_MOTOR;

	// \xml <axis_parameters axis="0">
	switch(type)
	{
	case 0:
		// \xml <tmc:axis_parameter type="0" name="Target position" identifier="par::TargetPosition">
		// \xml   <desc>The desired target position in position mode.</desc>
		// \xml   <tmc:value_signed min="#s32_MIN#" max="#s32_MAX#" default="0" unit="[usteps]" access="RW"></tmc:value_signed>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_XTARGET);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_XTARGET, *value);
		}
		break;
	case 1:
		// \xml <tmc:axis_parameter type="1" name="Actual position" identifier="par::ActualPosition">
		// \xml   <desc>The actual position of the motor. Stop the motor before overwriting it. Should normally only be overwritten for reference position setting.</desc>
		// \xml   <tmc:value_signed min="#s32_MIN#" max="#s32_MAX#" default="0" unit="[usteps]" access="RW"></tmc:value_signed>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_XACTUAL);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_XACTUAL, *value);
		}
		break;
	case 2:
		// \xml <tmc:axis_parameter type="2" name="Target speed" identifier="par::TargetVelocity">
		// \xml   <desc>The desired speed in velocity mode. Not valid in position mode.</desc>
		// \xml   <tmc:value_signed min="-#TMC5130_MAX_VELOCITY#" max="#TMC5130_MAX_VELOCITY#" default="0" unit="[pps]" access="RW"></tmc:value_signed>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_VMAX);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_VMAX, *value);
		}
		break;
	case 3:
		// \xml <tmc:axis_parameter type="3" name="Actual speed" identifier="par::ActualVelocity">
		// \xml   <desc>The actual speed of the motor.</desc>
		// \xml   <tmc:value_signed min="-#TMC5130_MAX_VELOCITY#" max="#TMC5130_MAX_VELOCITY#" default="0" unit="[pps]" access="R"></tmc:value_signed>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_VACTUAL);
			*value = CAST_Sn_TO_S32(*value, 24);
		} else if(readWrite == WRITE) {
			errors |= ERROR_TYPE;
		}
		break;
	case 4:
		// \xml <tmc:axis_parameter type="4" name="Maximum speed" identifier="par::MaxVelocity">
		// \xml   <desc>The maximum speed used for positioning ramps.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#TMC5130_MAX_VELOCITY#" default="150000" unit="[pps]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_config[motor]->shadowRegister[TMC5130_VMAX];
		} else if(readWrite == WRITE) {
			TMC5130_config[motor]->shadowRegister[TMC5130_VMAX] = abs(*value);
			if(tmc5130_readInt(motor, TMC5130_RAMPMODE) == TMC5130_MODE_POSITION)
			{
				tmc5130_writeInt(motor, TMC5130_VMAX, abs(*value));
			}
		}
		break;
	case 5:
		// \xml <tmc:axis_parameter type="5" name="Maximum acceleration" identifier="par::MaxAcceleration">
		// \xml   <desc>Maximum acceleration in positioning ramps. Acceleration and deceleration value in velocity mode.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#TMC5130_MAX_ACCELERATION#" default="1000" unit="[pps/s]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_AMAX);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_AMAX, *value);
		}
		break;
	case 6:
		// \xml <tmc:axis_parameter type="6" name="Maximum current" identifier="par::MaxCurrent">
		// \xml   <desc>Motor current used when motor is running. Where 0 = 3.125% up to 31 = 100% of maximum module current. Calculated by: maximum module current * (value + 1)/ 32.</desc>
		// \xml   <tmc:value_unsigned min="0" max="31" default="24" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_IHOLD_IRUN, TMC5130_IRUN_MASK, TMC5130_IRUN_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_IHOLD_IRUN, TMC5130_IRUN_MASK, TMC5130_IRUN_SHIFT, *value);
		}
		break;
	case 7:
		// \xml <tmc:axis_parameter type="7" name="Standby current" identifier="par::StandbyCurrent">
		// \xml   <desc>The current used when the motor is not running. Where 0 = 3.125% up to 31 = 100% of maximum module current. Calculated by: maximum module current * (value + 1)/ 32. This value should be as low as possible so that the motor can cool down when it is not moving. Please see also parameter 214.</desc>
		// \xml   <tmc:value_unsigned min="0" max="31" default="3" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_IHOLD_IRUN, TMC5130_IHOLD_MASK, TMC5130_IHOLD_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_IHOLD_IRUN, TMC5130_IHOLD_MASK, TMC5130_IHOLD_SHIFT, *value);
		}
		break;
	case 8:
		// \xml <tmc:axis_parameter type="8" name="Position reached flag" identifier="par::PositionReachedFlag">
		// \xml   <desc>This flag is always set when target position and actual position are equal.</desc>
		// \xml   <tmc:value_bool desc_false="Target position not reached" desc_true="Target position reached" default="false" access="R"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = (tmc5130_readInt(motor, TMC5130_RAMPSTAT) & TMC5130_RS_POSREACHED)? 1:0;
		} else if(readWrite == WRITE) {
			errors |= ERROR_TYPE;
		}
		break;
	case 10:
		// \xml <tmc:axis_parameter type="10" name="Right endstop" identifier="par::RightEndstop">
		// \xml   <desc>Reference switch right status.</desc>
		// \xml   <tmc:value_bool desc_false="Inactive" desc_true="Active" default="false" access="R"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = (tmc5130_readInt(motor, TMC5130_RAMPSTAT) & TMC5130_RS_STOPR)? 0:1;
		} else if(readWrite == WRITE) {
			errors |= ERROR_TYPE;
		}
		break;
	case 11:
		// \xml <tmc:axis_parameter type="11" name="Left endstop" identifier="par::LeftEndstop">
		// \xml   <desc>Reference switch left status.</desc>
		// \xml   <tmc:value_bool desc_false="Inactive" desc_true="Active" default="false" access="R"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = (tmc5130_readInt(motor, TMC5130_RAMPSTAT) & TMC5130_RS_STOPL)? 0:1;
		} else if(readWrite == WRITE) {
			errors |= ERROR_TYPE;
		}
		break;
	case 12:
		// \xml <tmc:axis_parameter type="12" name="Automatic right stop" identifier="par::AutomaticRightStop">
		// \xml   <desc>Enables automatic motor stop during active right reference switch input.</desc>
		// \xml   <tmc:value_bool desc_false="Inactive" desc_true="Active" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = (tmc5130_readInt(motor, TMC5130_SWMODE) & TMC5130_SW_STOPR_ENABLE)? 1:0;
		} else if(readWrite == WRITE) {
			buffer = tmc5130_readInt(motor, TMC5130_SWMODE);
			if(*value == 0)
				tmc5130_writeInt(motor, TMC5130_SWMODE, buffer | TMC5130_SW_STOPR_ENABLE);
			else
				tmc5130_writeInt(motor, TMC5130_SWMODE, buffer & ~TMC5130_SW_STOPR_ENABLE);
		}
		break;
	case 13:
		// \xml <tmc:axis_parameter type="13" name="Automatic left stop" identifier="par::AutomaticLeftStop">
		// \xml   <desc>Enables automatic motor stop during active left reference switch input.</desc>
		// \xml   <tmc:value_bool desc_false="Inactive" desc_true="Active" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = (tmc5130_readInt(motor, TMC5130_SWMODE) & TMC5130_SW_STOPL_ENABLE)? 1:0;
		} else if(readWrite == WRITE) {
			buffer	= tmc5130_readInt(motor, TMC5130_SWMODE);
			if(*value==0)
				tmc5130_writeInt(motor, TMC5130_SWMODE, buffer | TMC5130_SW_STOPL_ENABLE);
			else
				tmc5130_writeInt(motor, TMC5130_SWMODE, buffer & ~TMC5130_SW_STOPL_ENABLE);
		}
		break;
	case 14:
		// \xml <tmc:axis_parameter type="14" name="SW_MODE Register" identifier="par::SW_MODE">
		// \xml   <desc>Complete SW_MODE reference switch and StallGuard2 event configuration register.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u16_MAX#" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_SWMODE);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_SWMODE, *value);
		}
		break;
	case 15:
		// \xml <tmc:axis_parameter type="15" name="Acceleration A1" identifier="par::A1">
		// \xml   <desc>First acceleration between VSTART and V1 (in position mode only).</desc>
		// \xml   <tmc:value_unsigned min="0" max="#TMC5130_MAX_ACCELERATION#" default="100" unit="[pps/s]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_A1);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_A1, *value);
		}
		break;
	case 16:
		// \xml <tmc:axis_parameter type="16" name="Velocity V1" identifier="par::V1">
		// \xml   <desc>First acceleration / deceleration phase target velocity (in position mode only). Setting this value to 0 turns off the first acceleration / deceleration phase, maximum acceleration (axis parameter 5) and maximum deceleration (axis parameter 17) are used only.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u20_MAX#" default="2" unit="[pps]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_V1);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_V1, *value);
		}
		break;
	case 17:
		// \xml <tmc:axis_parameter type="17" name="Maximum Deceleration" identifier="par::MaxDeceleration">
		// \xml   <desc>Maximum deceleration in positioning ramps. Used to decelerate from maximum positioning speed (axis parameter 4) to velocity V1</desc>
		// \xml   <tmc:value_unsigned min="0" max="#TMC5130_MAX_ACCELERATION#" default="5" unit="[pps/s]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_DMAX);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_DMAX, *value);
		}
		break;
	case 18:
		// \xml <tmc:axis_parameter type="18" name="Deceleration D1" identifier="par::D1">
		// \xml   <desc>Deceleration between V1 and VSTOP (in positioning mode only).</desc>
		// \xml   <tmc:value_unsigned min="0" max="#TMC5130_MAX_ACCELERATION#" default="200" unit="[pps/s]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_D1);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_D1, *value);
		}
		break;
	case 19:
		// \xml <tmc:axis_parameter type="19" name="Velocity VSTART" identifier="par::StartVelocity">
		// \xml   <desc>Motor start velocity (in position mode only). Do not set VSTART higher than VSTOP.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u18_MAX#" default="1" unit="[pps]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_VSTART);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_VSTART, *value);
		}
		break;
	case 20:
		// \xml <tmc:axis_parameter type="20" name="Velocity VSTOP" identifier="par::StopVelocity">
		// \xml   <desc>Motor stop velocity (in position mode only).</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u18_MAX#" default="10" unit="[pps]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_VSTOP);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_VSTOP, *value);
		}
		break;
	case 21:
		// \xml <tmc:axis_parameter type="21" name="Waiting time after ramp down" identifier="par::RampWaitTime">
		// \xml   <desc>Defines the waiting time after ramping down to zero velocity before next movement or direction inversion can start. Time range is about 0 to 2 seconds.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u16_MAX#" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_TZEROWAIT);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_TZEROWAIT, *value);
		}
		break;

	case 23:
		// \xml <tmc:axis_parameter type="23" name="Speed threshold for high speed mode" identifier="par::THIGH">
		// \xml   <desc>Speed threshold for de-activating coolStep and switching into a different chopper mode and fullstepping to maximize torque.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u20_MAX#" default="0" unit="[pps]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			buffer = tmc5130_readInt(motor, TMC5130_THIGH);
			*value = MIN(0xFFFFF, (1<<24) / ((buffer)? buffer:1));
		} else if(readWrite == WRITE) {
			*value = MIN(0xFFFFF, (1<<24) / ((*value)? *value:1));
			tmc5130_writeInt(motor, TMC5130_THIGH, *value);
		}
		break;
	case 24:
		// \xml <tmc:axis_parameter type="24" name="Minimum speed for switching to dcStep" identifier="par::VDCMIN">
		// \xml   <desc>Minimum speed for switching to dcStep.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u22_MAX#" default="0" unit="[pps]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_VDCMIN);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_VDCMIN, *value);
		}
		break;
	case 27:
		// \xml <tmc:axis_parameter type="27" name="High speed chopper mode" identifier="par::HighSpeedChopperMode">
		// \xml   <desc>Switch to other chopper mode when measured speed is higher than axis parameter 22 when set to 1.</desc>
		// \xml   <tmc:value_bool desc_false="Inactive" desc_true="Active" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_VHIGHCHM_MASK, TMC5130_VHIGHCHM_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_VHIGHCHM_MASK, TMC5130_VHIGHCHM_SHIFT, *value);
		}
		break;
	case 28:
		// \xml <tmc:axis_parameter type="28" name="High speed fullstep mode" identifier="par::HighSpeedFullstepMode">
		// \xml   <desc>Switch to fullstep mode when measured speed is higher than axis parameter 23 when set to 1.</desc>
		// \xml   <tmc:value_bool desc_false="Inactive" desc_true="Active" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_VHIGHFS_MASK, TMC5130_VHIGHFS_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_VHIGHFS_MASK, TMC5130_VHIGHFS_SHIFT, *value);
		}
		break;
	case 29:
		// todo XML CHECK 2: check description #3
		// \xml <tmc:axis_parameter type="29" name="Measured Speed" identifier="par::MeasuredSpeed">
		// \xml   <desc>Current speed value measured from the TMC5130 current position or from encoder.</desc>
		// \xml   <tmc:value_signed min="#s32_MIN#" max="#s32_MAX#" default="0" unit="" access="R"></tmc:value_signed>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130[motor].velocity;
		} else if(readWrite == WRITE) {
			errors |= ERROR_TYPE;
		}
		break;
	case 33:
		// \xml <tmc:axis_parameter type="33" name="Analog I Scale" identifier="par::I_scale_analog">
		// \xml   <desc>Selection between internal reference voltage and voltage supplied to AIN as current reference.</desc>
		// \xml   <tmc:value_bool desc_false="Normal Operation (internal)" desc_true="Voltage Supplied to AIN" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_GCONF, TMC5130_I_SCALE_ANALOG_MASK, TMC5130_I_SCALE_ANALOG_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_GCONF, TMC5130_I_SCALE_ANALOG_MASK, TMC5130_I_SCALE_ANALOG_SHIFT, *value);
		}
		break;
	case 34:
		// \xml <tmc:axis_parameter type="34" name="Internal RSense" identifier="par::internal_Rsense">
		// \xml   <desc>Activation of internal resistors to use current supplied into AIN as reference for internal sense resistor</desc>
		// \xml   <tmc:value_bool desc_false="Normal Operation" desc_true="Internal Sense Resistors" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_GCONF, TMC5130_INTERNAL_RSENSE_MASK, TMC5130_INTERNAL_RSENSE_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_GCONF, TMC5130_INTERNAL_RSENSE_MASK, TMC5130_INTERNAL_RSENSE_SHIFT, *value);
		}
		break;
	case 140:
		// \xml <tmc:axis_parameter type="140" name="Microstep Resolution" identifier="par::MicrostepResolution">
		// \xml   <desc>Microstep resolutions per full step:</desc>
		// \xml   <tmc:value_choice default="256" unit="" access="RW">
		// \xml     <choice_element value="1" desc="fullstep"></choice_element>
		// \xml     <choice_element value="2" desc="halfstep"></choice_element>
		// \xml     <choice_element value="4" desc="4 microsteps"></choice_element>
		// \xml     <choice_element value="8" desc="8 microsteps"></choice_element>
		// \xml     <choice_element value="16" desc="16 microsteps"></choice_element>
		// \xml     <choice_element value="32" desc="32 microsteps"></choice_element>
		// \xml     <choice_element value="64" desc="64 microsteps"></choice_element>
		// \xml     <choice_element value="128" desc="128 microsteps"></choice_element>
		// \xml     <choice_element value="256" desc="256 microsteps"></choice_element>
		// \xml   </tmc:value_choice>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = 256 >> TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_MRES_MASK, TMC5130_MRES_SHIFT);
		} else if(readWrite == WRITE) {
			switch(*value)
			{
			case 1:    *value = 8;   break;
			case 2:    *value = 7;   break;
			case 4:    *value = 6;   break;
			case 8:    *value = 5;   break;
			case 16:   *value = 4;   break;
			case 32:   *value = 3;   break;
			case 64:   *value = 2;   break;
			case 128:  *value = 1;   break;
			case 256:  *value = 0;   break;
			default:   *value = -1;  break;
			}

			if(*value != -1)
			{
				TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_MRES_MASK, TMC5130_MRES_SHIFT, *value);
			}
			else
				errors |= ERROR_VALUE;
		}
		break;
	case 162:
		// \xml <tmc:axis_parameter type="162" name="Chopper blank time" identifier="par::ChopperBlankTime">
		// \xml   <desc>Selects the comparator blank time. This time needs to safely cover the switching event and the duration of the ringing on the sense resistor. Normally leave at the default value.</desc>
		// \xml   <tmc:value_unsigned min="0" max="3" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_TBL_MASK, TMC5130_TBL_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_TBL_MASK, TMC5130_TBL_SHIFT, *value);
		}
		break;
	case 163:
		// \xml <tmc:axis_parameter type="163" name="Constant TOff Mode" identifier="par::ConstantTOffMode">
		// \xml   <desc>Selection of the chopper mode.</desc>
		// \xml   <tmc:value_bool desc_false="spread cycle" desc_true="classic constant off time" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_CHM_MASK, TMC5130_CHM_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_CHM_MASK, TMC5130_CHM_SHIFT, *value);
		}
		break;
	case 164:
		// \xml <tmc:axis_parameter type="164" name="Disable fast decay comparator" identifier="par::DisableFastDecayComparator">
		// \xml   <desc>See parameter 163. For "classic const. off time", setting this parameter to "1" will disable current comparator usage for termination of fast decay cycle.</desc>
		// \xml   <tmc:value_bool desc_false="enable comparator" desc_true="disable comparator" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_DISFDCC_MASK, TMC5130_DISFDCC_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_DISFDCC_MASK, TMC5130_DISFDCC_SHIFT, *value);
		}
		break;
	case 165:
		// \xml <tmc:axis_parameter type="165" name="Chopper hysteresis end / fast decay time" identifier="par::ChopperHysteresisEnd">
		// \xml   <desc>See parameter 163. For "spread cycle" chopper mode this parameter will set / return the hysteresis end setting (hysteresis end value after a number of decrements). For "classic const. off time" chopper mode this parameter will set / return the fast decay time.</desc>
		// \xml   <tmc:value_unsigned min="0" max="15" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		buffer = tmc5130_readInt(motor, TMC5130_CHOPCONF);
		if(readWrite == READ) {
			if(buffer & (1<<14))
			{
				*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_HEND_MASK, TMC5130_HEND_SHIFT);
			}
			else
			{
				*value = ((buffer >> 4) & 0x07) | (buffer & (1<<11))? (1<<3):0;
			}
		} else if(readWrite == WRITE) {
			if(buffer & (1<<14))
			{
				TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_HEND_MASK, TMC5130_HEND_SHIFT, *value);
			}
			else
			{
				if(*value & (1<<3))
					buffer |= (0x01<<11);
				else
					buffer &= ~(0x01<<11);

				buffer &= ~(0x07<<4);
				buffer |= (*value & 0x0F) << 4;

				tmc5130_writeInt(motor, TMC5130_CHOPCONF,buffer);
			}
		}
		break;
	case 166:
		// \xml <tmc:axis_parameter type="166" name="Chopper hysteresis start / sine wave offset" identifier="par::ChopperHysteresisStart">
		// \xml   <desc>See parameter 163. For "spread cycle" chopper mode this parameter will set / return the Hysteresis start setting (please note that this value is an offset to the hysteresis end value). For "classic const. off time" chopper mode this parameter will set / return the sine wave offset.</desc>
		// \xml   <tmc:value_unsigned min="0" max="8" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		buffer = tmc5130_readInt(motor, TMC5130_CHOPCONF);
		if(readWrite == READ) {
			if(buffer & (1<<14))
			{
				*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_HSTRT_MASK, TMC5130_HSTRT_SHIFT);
			}
			else
			{
				*value = ((buffer >> 7) & 0x0F) | (buffer & (1<<11))? 1<<3 : 0;
			}
		} else if(readWrite == WRITE) {
			if(tmc5130_readInt(motor, TMC5130_CHOPCONF) & (1<<14))
			{
				TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_HSTRT_MASK, TMC5130_HSTRT_SHIFT, *value);
			}
			else
			{
				TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_OFFSET_MASK, TMC5130_OFFSET_SHIFT, *value);
			}
		}
		break;
	case 167:
		// \xml <tmc:axis_parameter type="167" name="Chopper off time" identifier="par::TOff">
		// \xml   <desc>The off time setting controls the minimum chopper frequency. An off time within the range of 5us to 20us will fit. Off time setting for constant t Off chopper: NCLK = 12 + 32  tOFF (Minimum is 64 clocks) Setting this parameter to zero completely disables all driver transistors and the motor can freewheel.</desc>
		// \xml   <tmc:value_unsigned min="0" max="15" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_TOFF_MASK, TMC5130_TOFF_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_TOFF_MASK, TMC5130_TOFF_SHIFT, *value);
		}
		break;
	case 168:
		// \xml <tmc:axis_parameter type="168" name="smartEnergy current minimum (SEIMIN)" identifier="par::SEIMIN">
		// \xml   <desc>Sets the lower motor current limit for coolStep operation by scaling the maximum current (see axis parameter 6) value.</desc>
		// \xml   <tmc:value_bool desc_false="1/2 of CS" desc_true="1/4 of CS" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_COOLCONF, TMC5130_SEIMIN_MASK, TMC5130_SEIMIN_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_COOLCONF, TMC5130_SEIMIN_MASK, TMC5130_SEIMIN_SHIFT, *value);
		}
		break;
	case 169:
		// \xml <tmc:axis_parameter type="169" name="smartEnergy current down step" identifier="par::SECDS">
		// \xml   <desc>Sets the number of stallGuard2 readings above the upper threshold necessary for each current decrement of the motor current. Number of stallGuard2 measurements per decrement: Scaling: 0. . . 3: 32, 8, 2, 1. 0: slow decrement, 3: fast decrement</desc>
		// \xml   <tmc:value_unsigned min="0" max="3" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_COOLCONF, TMC5130_SEDN_MASK, TMC5130_SEDN_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_COOLCONF, TMC5130_SEDN_MASK, TMC5130_SEDN_SHIFT, *value);
		}
		break;
	case 170:
		// \xml <tmc:axis_parameter type="170" name="smartEnergy hysteresis" identifier="par::smartEnergyHysteresis">
		// \xml   <desc>Sets the distance between the lower and the upper threshold for stallGuard2 reading. Above the upper threshold the motor current becomes decreased. Hysteresis: ([AP172] + 1) * 32. Upper stallGuard threshold: ([AP172] + [AP170] + 1) * 32</desc>
		// \xml   <tmc:value_unsigned min="0" max="15" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_COOLCONF, TMC5130_SEMAX_MASK, TMC5130_SEMAX_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_COOLCONF, TMC5130_SEMAX_MASK, TMC5130_SEMAX_SHIFT, *value);
		}
		break;
	case 171:
		// \xml <tmc:axis_parameter type="171" name="smartEnergy current up step" identifier="par::SECUS">
		// \xml   <desc>Sets the current increment step. The current becomes incremented for each measured stallGuard2 value below the lower threshold see smartEnergy hysteresis start). Current increment step size: Scaling: 0. . . 3: 1, 2, 4, 8. 0: slow increment, 3: fast increment / fast reaction to rising load</desc>
		// \xml   <tmc:value_unsigned min="0" max="3" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_COOLCONF, TMC5130_SEUP_MASK, TMC5130_SEUP_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_COOLCONF, TMC5130_SEUP_MASK, TMC5130_SEUP_SHIFT, *value);
		}
		break;
	case 172:
		// \xml <tmc:axis_parameter type="172" name="smartEnergy hysteresis start" identifier="par::smartEnergyHysteresisStart">
		// \xml   <desc>The lower threshold for the stallGuard2 value (see smart Energy current up step).</desc>
		// \xml   <tmc:value_unsigned min="0" max="15" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_COOLCONF, TMC5130_SEMIN_MASK, TMC5130_SEMIN_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_COOLCONF, TMC5130_SEMIN_MASK, TMC5130_SEMIN_SHIFT, *value);
		}
		break;
	case 173:
		// \xml <tmc:axis_parameter type="173" name="stallGuard2 filter enable" identifier="par::SG2FilterEnable">
		// \xml   <desc>Enables the stallGuard2 filter for more precision of the movement. If set, reduces the measurement frequency to one measurement per four fullsteps. In most cases it is expedient to set the filtered mode before using coolStep. Use the standard mode for step loss detection.</desc>
		// \xml   <tmc:value_bool desc_false="standard mode" desc_true="filtered mode" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_COOLCONF, TMC5130_SFILT_MASK, TMC5130_SFILT_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_COOLCONF, TMC5130_SFILT_MASK, TMC5130_SFILT_SHIFT, *value);
		}
		break;
	case 174:
		// \xml <tmc:axis_parameter type="174" name="stallGuard2 threshold" identifier="par::SG2Threshold">
		// \xml   <desc>This signed value controls stallGuard2 threshold level for stall output and sets the optimum measurement range for readout. A lower value gives a higher sensitivity. Zero is the starting value. A higher value makes stallGuard2 less sensitive and requires more torque to indicate a stall.</desc>
		// \xml   <tmc:value_signed min="-64" max="63" default="0" unit="" access="RW"></tmc:value_signed>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_COOLCONF, TMC5130_SGT_MASK, TMC5130_SGT_SHIFT);
			*value = CAST_Sn_TO_S32(*value, 7);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_COOLCONF, TMC5130_SGT_MASK, TMC5130_SGT_SHIFT, *value);
		}
		break;
	case 179:
		// \xml <tmc:axis_parameter type="179" name="VSense" identifier="par::VSense">
		// \xml   <desc>Sense resistor voltage:</desc>
		// \xml   <tmc:value_bool desc_false="High sense resistor voltage" desc_true="Low sense resistor voltage" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_VSENSE_MASK, TMC5130_VSENSE_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_VSENSE_MASK, TMC5130_VSENSE_SHIFT, *value);
		}
		break;
	case 180:
		// \xml <tmc:axis_parameter type="180" name="smartEnergy actual current" identifier="par::smartEnergyActualCurrent">
		// \xml   <desc>This status value provides the actual motor current setting as controlled by coolStep. The value goes up to the CS value and down to the portion of CS as specified by SEIMIN. Actual motor current scaling factor: 0. . . 31: 1/32, 2/32, . . . 32/32</desc>
		// \xml   <tmc:value_unsigned min="0" max="31" default="0" unit="" access="R"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_DRVSTATUS, TMC5130_CS_ACTUAL_MASK, TMC5130_CS_ACTUAL_SHIFT);
		} else if(readWrite == WRITE) {
			errors |= ERROR_TYPE;
		}
		break;
	case 181:
		// \xml <tmc:axis_parameter type="181" name="smartEnergy stall velocity" identifier="par::smartEnergyStallVelocity">
		// \xml   <desc>Velocity from which stop on stall feature is active</desc>
		// \xml   <tmc:value_unsigned min="0" max="#TMC5130_MAX_VELOCITY#" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		//this function sort of doubles with 182 but is necessary to allow cross chip compliance
		if(readWrite == READ) {
			if(tmc5130_readInt(motor, TMC5130_SWMODE) & (1<<10))
			{
				buffer = tmc5130_readInt(motor, TMC5130_TCOOLTHRS);
				*value = MIN(0xFFFFF, (1<<24) / ((buffer)? buffer : 1));
			}
			else
				*value = 0;
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_SWMODE, TMC5130_SG_STOP_MASK, TMC5130_SG_STOP_SHIFT, *value);

			*value = MIN(0xFFFFFF, (1<<24) / ((*value) ? *value : 1));
			tmc5130_writeInt(motor, TMC5130_TCOOLTHRS, *value);
		}
		break;
	case 182:
		// \xml <tmc:axis_parameter type="182" name="smartEnergy threshold speed" identifier="par::smartEnergyThresholdSpeed">
		// \xml   <desc>Above this speed coolStep becomes enabled.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u20_MAX#" default="0" unit="[pps]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			buffer = tmc5130_readInt(motor, TMC5130_TCOOLTHRS);
			*value = MIN(0xFFFFF, (1 << 24) / ((buffer)? buffer : 1));
		} else if(readWrite == WRITE) {
			buffer = MIN(0xFFFFF, (1<<24) / ((*value)? *value : 1));
			tmc5130_writeInt(motor, TMC5130_TCOOLTHRS, buffer);
		}
		break;
	case 184:
		// \xml <tmc:axis_parameter type="184" name="Random TOff mode" identifier="par::RandomTOffMode">
		// \xml   <desc>Enable / disable random TOff mode.</desc>
		// \xml   <tmc:value_bool desc_false="Chopper off time is fixed" desc_true="Chopper off time is random" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_RNDTF_MASK, TMC5130_RNDTF_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_RNDTF_MASK, TMC5130_RNDTF_SHIFT, *value);
		}
		break;
	case 185:
		// \xml <tmc:axis_parameter type="185" name="Chopper synchronization" identifier="par::ChopperSynchronization">
		// \xml   <desc>This parameter allows synchronization of the chopper for both phases of a two phase motor in order to avoid the occurrence of a beat, especially at low velocities. 0: chopper sync function chopSync off 1. . . 15: chopper synchronization</desc>
		// \xml   <tmc:value_unsigned min="0" max="15" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_CHOPCONF, TMC5130_SYNC_MASK, TMC5130_SYNC_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_CHOPCONF, TMC5130_SYNC_MASK, TMC5130_SYNC_SHIFT, *value);
		}
		break;
	case 186:
		// \xml <tmc:axis_parameter type="186" name="PWM threshold speed" identifier="par::PWMThresholdSpeed">
		// \xml   <desc>The stealthChop feature will be switched on when the actual velocity falls below this value.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u20_MAX#" default="0" unit="[pps]" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			buffer = tmc5130_readInt(motor, TMC5130_TPWMTHRS);
			*value = MIN(0xFFFFF, (1<<24) / ((buffer)? buffer : 1));
		} else if(readWrite == WRITE) {
			*value = MIN(0xFFFFF, (1<<24) / ((*value)? *value : 1));
			tmc5130_writeInt(motor, TMC5130_TPWMTHRS, *value);
		}
		break;
	case 187:
		// \xml <tmc:axis_parameter type="187" name="PWM gradient" identifier="par::PWMGrad">
		// \xml   <desc>Velocity dependent gradient for PWM amplitude (stealthChop). Setting this value to 0 turns off stealthChop.</desc>
		// \xml   <tmc:value_unsigned min="0" max="15" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_PWMCONF, TMC5130_PWM_GRAD_MASK, TMC5130_PWM_GRAD_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_PWMCONF, TMC5130_PWM_GRAD_MASK, TMC5130_PWM_GRAD_SHIFT, *value);
			TMC5130_FIELD_UPDATE(motor, TMC5130_GCONF, TMC5130_EN_PWM_MODE_MASK, TMC5130_EN_PWM_MODE_SHIFT, (*value)? 1:0);
		}
		break;
	case 188:
		// \xml <tmc:axis_parameter type="188" name="PWM amplitude" identifier="par::PWMAmplitude">
		// \xml   <desc>Maximum PWM amplitude when switching to stealthChop mode. Do not set too low. Values above 64 recommended.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u8_MAX#" default="0" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_PWMCONF, TMC5130_PWM_AMPL_MASK, TMC5130_PWM_AMPL_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_PWMCONF, TMC5130_PWM_AMPL_MASK, TMC5130_PWM_AMPL_SHIFT, *value);
		}
		break;
	case 191:
		// \xml <tmc:axis_parameter type="191" name="PWM frequency" identifier="par::PWMFrequency">
		// \xml   <desc>PWM frequency selection for StealthChop.</desc>
		// \xml   <tmc:value_choice default="0" unit="" access="RW">
		// \xml     <choice_element value="0" desc="fpwm = 1/1024 fclk"></choice_element>
		// \xml     <choice_element value="1" desc="fpwm = 1/683 fclk"></choice_element>
		// \xml     <choice_element value="2" desc="fpwm = 1/512 fclk"></choice_element>
		// \xml     <choice_element value="3" desc="fpwm = 1/410 fclk"></choice_element>
		// \xml   </tmc:value_choice>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_PWMCONF, TMC5130_PWM_FREQ_MASK, TMC5130_PWM_FREQ_SHIFT);
		} else if(readWrite == WRITE) {
			if(*value >= 0 && *value < 4)
			{
				TMC5130_FIELD_UPDATE(motor, TMC5130_PWMCONF, TMC5130_PWM_FREQ_MASK, TMC5130_PWM_FREQ_SHIFT, *value);
			}
			else
			{
				errors |= ERROR_VALUE;
			}
		}
		break;
	case 192:
		// \xml <tmc:axis_parameter type="192" name="PWM autoscale" identifier="par::PWMAutoscale">
		// \xml   <desc>PWM automatic amplitude scaling for StealthChop.</desc>
		// \xml   <tmc:value_bool desc_false="User defined PWM amplitude. The current settings have no influence" desc_true="enable automatic current control" default="false" access="RW"></tmc:value_bool>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_PWMCONF, TMC5130_PWM_AUTOSCALE_MASK, TMC5130_PWM_AUTOSCALE_SHIFT);
		} else if(readWrite == WRITE) {
			if(*value >= 0 && *value < 2)
			{
				TMC5130_FIELD_UPDATE(motor, TMC5130_PWMCONF, TMC5130_PWM_AUTOSCALE_MASK, TMC5130_PWM_AUTOSCALE_SHIFT, *value);
			}
			else
				errors |= ERROR_VALUE;
		}
		break;
	case 204:
		// \xml <tmc:axis_parameter type="204" name="Freewheeling mode" identifier="par::FreewheelingMode">
		// \xml   <desc>Stand still option when the standby current (parameter 7) is set to zero.</desc>
		// \xml   <tmc:value_choice default="0" unit="" access="RWE">
		// \xml     <choice_element value="0" desc="normal operation"></choice_element>
		// \xml     <choice_element value="1" desc="freewheeling"></choice_element>
		// \xml     <choice_element value="2" desc="coil shorted using low side drivers"></choice_element>
		// \xml     <choice_element value="3" desc="coil shorted using high side drivers"></choice_element>
		// \xml   </tmc:value_choice>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_PWMCONF, TMC5130_FREEWHEEL_MASK, TMC5130_FREEWHEEL_SHIFT);
		} else if(readWrite == WRITE) {
			TMC5130_FIELD_UPDATE(motor, TMC5130_PWMCONF, TMC5130_FREEWHEEL_MASK, TMC5130_FREEWHEEL_SHIFT, *value);
		}
		break;
	case 206:
		// \xml <tmc:axis_parameter type="206" name="Load value" identifier="par::LoadValue">
		// \xml   <desc>Actual current control scaling for monitoring smart energy current scaling or automatic current scaling.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u10_MAX#" default="0" unit="" access="R"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = TMC5130_FIELD_READ(motor, TMC5130_DRVSTATUS, TMC5130_SG_RESULT_MASK, TMC5130_SG_RESULT_SHIFT);
		} else if(readWrite == WRITE) {
			errors |= ERROR_TYPE;
		}
		break;
	case 209:
		// \xml <tmc:axis_parameter type="209" name="Encoder position" identifier="par::EncoderPosition">
		// \xml   <desc>The current encoder position</desc>
		// \xml   <tmc:value_signed min="#s32_MIN#" max="#s32_MAX#" default="0" unit="[usteps]" access="RW"></tmc:value_signed>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_XENC);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_XENC, *value);
		}
		break;
	case 210:
		// \xml <tmc:axis_parameter type="210" name="Encoder Resolution" identifier="par::EncoderResolution">
		// \xml   <desc>Resolution of the encoder.</desc>
		// \xml   <tmc:value_unsigned min="0" max="#u16_MAX#" default="32768" unit="" access="RW"></tmc:value_unsigned>
		// \xml </tmc:axis_parameter>
		if(readWrite == READ) {
			*value = tmc5130_readInt(motor, TMC5130_ENC_CONST);
		} else if(readWrite == WRITE) {
			tmc5130_writeInt(motor, TMC5130_ENC_CONST, *value);
		}
		break;
	default:
		errors |= ERROR_TYPE;
		break;
	}
	// \xml </axis_parameters>
	return errors;
}
// \xml </tmc:product_module>

static uint32 SAP(uint8 type, uint8 motor, int32 value)
{
	return handleParameter(WRITE, motor, type, &value);
}

static uint32 GAP(uint8 type, uint8 motor, int32 *value)
{
	return handleParameter(READ, motor, type, value);
}

static uint32 getMeasuredSpeed(uint8 motor, int32 *value)
{
	if(motor >= MOTORS)
		return ERROR_MOTOR;

	*value = TMC5130[motor].velocity;
	return ERROR_NONE;
}

static void writeRegister(uint8 address, int32 value)
{
	tmc5130_writeInt(DEFAULT_MOTOR, address, value);
}

static void readRegister(uint8 address, int32 *value)
{
	*value = tmc5130_readInt(DEFAULT_MOTOR, address);
}

static void periodicJob(uint32 tick)
{
	int motor;
	for(motor = 0; motor < MOTORS; motor++)
		tmc5130_periodicJob(motor, tick, &TMC5130[motor], TMC5130_config[motor]);
}

static void checkErrors(uint32 tick)
{
	UNUSED(tick);
	Evalboards.ch1.errors = 0;
}


static uint32 userFunction(uint8 type, uint8 motor, int32 *value)
{
	uint32 buffer;
	uint32 errors = 0;

	UNUSED(motor);

	switch(type)
	{
	case 0:  // simulate reference switches, set high to support external ref swiches
		/*
		 * The the TMC5130 ref switch input is pulled high by external resistor an can be pulled low either by
		 * this µC or external signal. To use external signal make sure the signals from µC are high or floating.
		 */
		if(!(*value & ~3))
		{
			if(*value & (1<<0))
			{
				HAL.IOs->config->toInput(Pins.REFR_UC); // pull up -> set it to floating causes high
			}
			else
			{
				HAL.IOs->config->toOutput(Pins.REFR_UC);
				HAL.IOs->config->setLow(Pins.REFR_UC);
			}

			if(*value & (1<<1))
			{
				HAL.IOs->config->toInput(Pins.REFL_UC); // pull up -> set it to floating causes high
			}
			else
			{
				HAL.IOs->config->toOutput(Pins.REFL_UC);
				HAL.IOs->config->setLow(Pins.REFL_UC);
			}
		}
		else
		{
			errors |= ERROR_VALUE;
		}
		break;
	case 1:  // set analogue current duty
		/*
		 * Current will be defined by analogue *value voltage or current signal. In any case this function
		 * will generate a analogue voltage by PWM for up to 50% duty and a switch for the other 50%.
		 * The reference voltage will be AIN_REF = VCC_IO * *value/20000 with *value = {0..20000}
		 */

		if(*value <= 20000)
		{
			if(*value > 10000)
				HAL.IOs->config->setHigh(Pins.AIN_REF_SW);
			else
				HAL.IOs->config->setLow(Pins.AIN_REF_SW);

			Timer.setDuty(*value % 10001);
		}
		else
			errors |= ERROR_VALUE;
		break;
	case 2:  // Use internal clock
		/*
		 * Internal clock will be enabled by calling this function with a *value != 0 and unpower and repower the motor supply while keeping usb connected.
		 */
		if(*value)
		{
			HAL.IOs->config->toOutput(&HAL.IOs->pins->CLK16);
			HAL.IOs->config->setLow(&HAL.IOs->pins->CLK16);
		}
		else
			HAL.IOs->config->reset(&HAL.IOs->pins->CLK16);
		break;
	case 3:  // writing a register at address = motor with *value = *value and reading back the *value
		// DO NOT USE!
		// todo BUG 3: using the motor for both address and motor value can lead to trying to get struct pointer from an array outside its bounds
		//             and then loading a function pointer from that. That is pretty a much guaranteed crash and/or hard fault!!! (LH) #2

		//tmc5130_writeInt(motor, motor, *value);
		//*value = tmc5130_readInt(motor, motor);
		break;
	case 4:  // set or release/read ENCB_[DCEN_CFG4]
		switch(buffer = *value)
		{
		case 0:
			HAL.IOs->config->toOutput(Pins.ENCB_DCEN_CFG4);
			HAL.IOs->config->setLow(Pins.ENCB_DCEN_CFG4);
			break;
		case 1:
			HAL.IOs->config->toOutput(Pins.ENCB_DCEN_CFG4);
			HAL.IOs->config->setHigh(Pins.ENCB_DCEN_CFG4);
			break;
		default:
			HAL.IOs->config->toInput(Pins.ENCB_DCEN_CFG4);
			buffer = HAL.IOs->config->isHigh(Pins.ENCB_DCEN_CFG4);;
			break;
		}
		*value = buffer;
		break;
	case 5:  // read interrupt pin SWN_DIAG0
		*value = (HAL.IOs->config->isHigh(Pins.SWN_DIAG0))? 1 : 0;
		break;
	case 6:  // read interrupt pin SWP_DIAG1
		*value = (HAL.IOs->config->isHigh(Pins.SWP_DIAG1))? 1 : 0;
		break;
	case 7:  // enable single wire interface (SWSEL)
		if(*value == 1)
			HAL.IOs->config->setHigh(Pins.SWSEL);
		else
			HAL.IOs->config->setLow(Pins.SWSEL);
		break;
	case 252:
		if(*value)
		{
			HAL.IOs->config->toOutput(Pins.ENCB_DCEN_CFG4);
			HAL.IOs->config->setLow(Pins.ENCB_DCEN_CFG4);
		}
		else
		{
			HAL.IOs->config->toInput(Pins.ENCB_DCEN_CFG4);
		}
		break;
	default:
		errors |= ERROR_TYPE;
		break;
	}
	return errors;
}

static void deInit(void)
{
	HAL.IOs->config->setLow(Pins.DRV_ENN_CFG6);
	HAL.IOs->config->reset(Pins.AIN_REF_PWM);
	HAL.IOs->config->reset(Pins.AIN_REF_SW);
	HAL.IOs->config->reset(Pins.ENCA_DCIN_CFG5);
	HAL.IOs->config->reset(Pins.ENCB_DCEN_CFG4);
	HAL.IOs->config->reset(Pins.ENCN_DCO);
	HAL.IOs->config->reset(Pins.REFL_UC);
	HAL.IOs->config->reset(Pins.REFR_UC);
	HAL.IOs->config->reset(Pins.SWN_DIAG0);
	HAL.IOs->config->reset(Pins.SWP_DIAG1);
	HAL.IOs->config->reset(Pins.SWSEL);
	HAL.IOs->config->reset(Pins.DRV_ENN_CFG6);

	Timer.deInit();
};

static uint8 reset()
{
	if(!tmc5130_readInt(0, TMC5130_VACTUAL))
		tmc5130_reset(TMC5130_config[0]);

	HAL.IOs->config->setLow(Pins.AIN_REF_SW);
	HAL.IOs->config->toInput(Pins.REFL_UC);
	HAL.IOs->config->toInput(Pins.REFR_UC);
	return 1;
}

static uint8 restore()
{
	return tmc5130_restore(TMC5130_config[0]);
}


static void enableDriver(DriverState state)
{
	if(state == DRIVER_USE_GLOBAL_ENABLE)
		state = Evalboards.driverEnable;

	if(state ==  DRIVER_DISABLE)
		HAL.IOs->config->setHigh(Pins.DRV_ENN_CFG6);
	else if((state == DRIVER_ENABLE) && (Evalboards.driverEnable == DRIVER_ENABLE))
		HAL.IOs->config->setLow(Pins.DRV_ENN_CFG6);
}

void TMC5130_init(void)
{
	int i;
	for(i = 0; i < MOTORS; i++)
		tmc5130_initConfig(&TMC5130[i]);

	Pins.DRV_ENN_CFG6    = &HAL.IOs->pins->DIO0;
	Pins.ENCN_DCO        = &HAL.IOs->pins->DIO1;
	Pins.ENCA_DCIN_CFG5  = &HAL.IOs->pins->DIO2;
	Pins.ENCB_DCEN_CFG4  = &HAL.IOs->pins->DIO3;
	Pins.REFL_UC         = &HAL.IOs->pins->DIO6;
	Pins.REFR_UC         = &HAL.IOs->pins->DIO7;
	Pins.AIN_REF_SW      = &HAL.IOs->pins->DIO10;
	Pins.AIN_REF_PWM     = &HAL.IOs->pins->DIO11;
	Pins.SWSEL           = &HAL.IOs->pins->DIO14;
	Pins.SWP_DIAG1       = &HAL.IOs->pins->DIO15;
	Pins.SWN_DIAG0       = &HAL.IOs->pins->DIO16;

	HAL.IOs->config->toInput(Pins.ENCN_DCO);
	HAL.IOs->config->toInput(Pins.ENCB_DCEN_CFG4);
	HAL.IOs->config->toInput(Pins.ENCA_DCIN_CFG5);

	HAL.IOs->config->toInput(Pins.SWN_DIAG0);
	HAL.IOs->config->toInput(Pins.SWP_DIAG1);
	HAL.IOs->config->toOutput(Pins.SWSEL);
	HAL.IOs->config->toInput(Pins.REFL_UC);
	HAL.IOs->config->toInput(Pins.REFR_UC);
	HAL.IOs->config->toOutput(Pins.DRV_ENN_CFG6);
	HAL.IOs->config->toOutput(Pins.AIN_REF_SW);
	HAL.IOs->config->toOutput(Pins.AIN_REF_PWM);

	HAL.IOs->config->setLow(Pins.SWSEL);

	for( i = 0; i < MOTORS; i++)
	{
		TMC5130_SPIChannel[i] = &HAL.SPI->ch1;
		TMC5130_SPIChannel[i]->CSN = &HAL.IOs->pins->SPI1_CSN;

		TMC5130_config[i] = Evalboards.ch1.config;
	}

	Evalboards.ch1.config->reset        = reset;
	Evalboards.ch1.config->restore      = restore;
	Evalboards.ch1.config->state        = CONFIG_RESET;
	Evalboards.ch1.config->configIndex  = 0;

	Evalboards.ch1.rotate               = rotate;
	Evalboards.ch1.right                = right;
	Evalboards.ch1.left                 = left;
	Evalboards.ch1.stop                 = stop;
	Evalboards.ch1.GAP                  = GAP;
	Evalboards.ch1.SAP                  = SAP;
	Evalboards.ch1.moveTo               = moveTo;
	Evalboards.ch1.moveBy               = moveBy;
	Evalboards.ch1.writeRegister        = writeRegister;
	Evalboards.ch1.readRegister         = readRegister;
	Evalboards.ch1.periodicJob          = periodicJob;
	Evalboards.ch1.userFunction         = userFunction;
	Evalboards.ch1.getMeasuredSpeed     = getMeasuredSpeed;
	Evalboards.ch1.enableDriver         = enableDriver;
	Evalboards.ch1.checkErrors          = checkErrors;
	Evalboards.ch1.numberOfMotors       = MOTORS;
	Evalboards.ch1.VMMin                = VM_MIN;
	Evalboards.ch1.VMMax                = VM_MAX;
	Evalboards.ch1.deInit               = deInit;

	enableDriver(DRIVER_USE_GLOBAL_ENABLE);

//	Pins.AIN_REF_PWM->configuration.GPIO_Mode = GPIO_Mode_AF;
//	GPIO_PinAFConfig(Pins.AIN_REF_PWM->port, Pins.AIN_REF_PWM->bit, GPIO_AF_TIM1);


	HAL.IOs->config->set(Pins.AIN_REF_PWM);
	Timer.init();
	Timer.setDuty(0);
};
