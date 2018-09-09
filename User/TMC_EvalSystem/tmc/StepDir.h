#ifndef STEP_DIR_H_
#define STEP_DIR_H_

	#include "helpers/API_Header.h"

	// todo API 2: move higher level StepDir functions to API!? (ED)

	#include "../hal/HAL.h"

	#define STEPDIR_MAX_VELOCITY 33554432

	typedef enum {SD_RAMP_MODE_POSITIONING, SD_RAMP_MODE_VELOCITY} TRampMode;
	typedef enum {RAMP_IDLE, RAMP_ACCELERATE, RAMP_DRIVING, RAMP_DECELERATE} TRampState;

	typedef struct
	{
		int                     actualVelocity;
		volatile uint32         actualAcceleration;
		volatile int            actualPosition;
		volatile int            targetVelocity;
		volatile int            targetPosition;
		volatile int            maxPositioningSpeed;
		volatile TRampMode      rampMode;
		volatile unsigned char  targetReached;
		volatile int            stopOnStallVelocityTreshold;
		volatile int            coolStepVelocityTreshold;
		IOPinTypeDef            *stepOut;
		IOPinTypeDef            *dirOut;
		IOPinTypeDef            *stallSignalIn;
		volatile uint8          stallFlagIn;
		volatile uint8          haltMask;
		volatile uint32         coolStepActiveValue;
		volatile uint32         coolStepInactiveValue;
		volatile uint32         coolStepActualValue;
		volatile uint8          stopOnStallActive;
		volatile int32          profileFrom;
		volatile int32          profileTo;
		volatile uint8          useProfile;
		void (*coolStepActivationChanged) (uint32 active);
	} StepDirectionChannelType;

	typedef struct
	{
		int32 positionAccu;  // position accumulator for computing velocity
		int32 velocityAccu;  // velocity accumulator for computing acceleration
		TRampState rampState;
		int32 accelerationSteps;
		StepDirectionChannelType channel;
	} StepDirectionCtrlType;

	typedef struct
	{
		StepDirectionChannelType *ch1;
		StepDirectionChannelType *ch2;
	} StepDirectionTypeDef;

	void StepDir_init(uint32 motorCount);
	void StepDir_deInit(void);

	extern StepDirectionTypeDef StepDir;

#endif /* STEP_DIR_H_ */
