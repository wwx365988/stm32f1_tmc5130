#include "StepDir.h"
#include "../hal/derivative.h"


	#define TIMER_INTERRUPT TIM2_IRQHandler


#define STEP_DIR_NO_CHANNELS 2

StepDirectionCtrlType CTRL[STEP_DIR_NO_CHANNELS];

StepDirectionTypeDef StepDir =
{
	.ch1  = &CTRL[0].channel,
	.ch2  = &CTRL[1].channel
};

IOPinTypeDef DummyPin = { .bitWeight = DUMMY_BITWEIGHT };

void TIMER_INTERRUPT()
{
	uint8 ch;
	int32 positionAdd;
	if(TIM_GetITStatus(TIM2, TIM_IT_Update) == RESET)
		return;
	TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // clear pending flag

	for(ch = 0; ch < STEP_DIR_NO_CHANNELS; ch++)
	{
		StepDirectionChannelType *currCh = &CTRL[ch].channel; // Variable just for line readability ("CTRL[ch].channel." -> "currCh->")

		if(currCh->haltMask)               continue;
		if(IS_DUMMY_PIN(currCh->stepOut))  continue;
		if(IS_DUMMY_PIN(currCh->dirOut))   continue;

		*currCh->stepOut->resetBitRegister = currCh->stepOut->bitWeight; // unset step output (falling edge of last pulse)

	// ========================================
	// determine acceleration/deceleration
	// ========================================

		if(currCh->actualVelocity != currCh->targetVelocity)
		{
			int32 velocityAdd;
			// resolution of acceleration is 32 Bit
			// after 2^17 interrupts the velocity got in/decreased by the acceleration value
			CTRL[ch].velocityAccu += currCh->actualAcceleration;
			velocityAdd	= CTRL[ch].velocityAccu  >> 17;
			CTRL[ch].velocityAccu -= velocityAdd << 17;

			// In/Decrease Velocity towards target velocity - but dont overshoot it
			if(currCh->actualVelocity < currCh->targetVelocity)
				currCh->actualVelocity	= MIN(currCh->actualVelocity + velocityAdd, currCh->targetVelocity);
			else if(currCh->actualVelocity > currCh->targetVelocity)
				currCh->actualVelocity	= MAX(currCh->actualVelocity - velocityAdd, currCh->targetVelocity);
		}
		else
		{
			CTRL[ch].velocityAccu = 0;
		}


	// ========================================
	// positioning
	// ========================================
		currCh->targetReached = 0;

		if(currCh->rampMode == SD_RAMP_MODE_POSITIONING)
		{
			switch(CTRL[ch].rampState)
			{
			case RAMP_IDLE:
				if(currCh->useProfile)
					currCh->targetPosition = (currCh->targetPosition == currCh->profileFrom) ? currCh->profileTo : currCh->profileFrom;

				// wait for currCh->targetPosition != currCh->actualPosition (e.g change of currCh->targetPosition)
				if(currCh->actualPosition == currCh->targetPosition)
				{
					currCh->targetReached = 1;
					break;
				}

				currCh->targetVelocity = (currCh->actualPosition < currCh->targetPosition) ? currCh->maxPositioningSpeed : -currCh->maxPositioningSpeed;
				CTRL[ch].accelerationSteps  = currCh->actualPosition;
				CTRL[ch].rampState          = RAMP_ACCELERATE;
				break;
			case RAMP_ACCELERATE:
				if(abs(currCh->actualVelocity) == currCh->maxPositioningSpeed)	// velocity reached
				{
					CTRL[ch].accelerationSteps  = abs(currCh->actualPosition - CTRL[ch].accelerationSteps) + 1;
					CTRL[ch].rampState          = RAMP_DRIVING;
				}
				else if(abs(currCh->actualPosition - CTRL[ch].accelerationSteps) >= abs(currCh->targetPosition - currCh->actualPosition))
				{	// start deceleration because steps left are needed for it
					currCh->targetVelocity = 0;
					CTRL[ch].rampState = RAMP_DECELERATE;
				}
				break;
			case RAMP_DRIVING:
				if((abs(currCh->targetPosition - currCh->actualPosition) <= CTRL[ch].accelerationSteps) ||
				   ((currCh->targetVelocity < 0) && (currCh->actualPosition < currCh->targetPosition)) ||
				   ((currCh->targetVelocity > 0) && (currCh->actualPosition > currCh->targetPosition)))
				{	// start deceleration because steps left are needed for it
					currCh->targetVelocity = 0;
					CTRL[ch].rampState = RAMP_DECELERATE;
				}
				break;
			case RAMP_DECELERATE:
				if(currCh->actualPosition == currCh->targetPosition)
				{
					currCh->targetVelocity = currCh->actualVelocity = 0;
					CTRL[ch].rampState = RAMP_IDLE;
				}
				else
				{
					if(currCh->actualVelocity == 0)
					{
						currCh->targetVelocity = currCh->actualVelocity = currCh->targetPosition - currCh->actualPosition;
						CTRL[ch].rampState = RAMP_ACCELERATE;
					}
				}
				break;
			}
		}

	// ========================================
	// stop on stall
	// ========================================


		if((!currCh->stopOnStallVelocityTreshold) || (abs(currCh->actualVelocity) < currCh->stopOnStallVelocityTreshold))
			currCh->stopOnStallActive = 0;
		else
			currCh->stopOnStallActive = 1;

		if(currCh->stopOnStallActive && !IS_DUMMY_PIN(currCh->stallSignalIn) && IOs.isHigh(currCh->stallSignalIn))
			currCh->stallFlagIn = 1;

		if(currCh->stallFlagIn)
			currCh->actualVelocity = 0;

	// ========================================
	// coolStep
	// ========================================

		if(currCh->coolStepVelocityTreshold)
		{
			uint32 value = (currCh->actualVelocity >= currCh->coolStepVelocityTreshold) ? currCh->coolStepActiveValue : currCh->coolStepInactiveValue;

			if(currCh->coolStepActualValue != value)
			{
				currCh->coolStepActualValue = value;
				if(currCh->coolStepActivationChanged)
					currCh->coolStepActivationChanged(value);
			}
		}


	// ========================================
	// velocity
	// ========================================
		

		// resolution of accumulator is 32 Bit
		// after 2^17 interrupts, position has changed by the velocity value
		CTRL[ch].positionAccu += currCh->actualVelocity;
		positionAdd = CTRL[ch].positionAccu >> 17;
		CTRL[ch].positionAccu -= positionAdd << 17;

		if(positionAdd == 0) // keine Positionsänderung bei diesem Durchlauf -> Fertig mit diesem channel
			continue;

	// ========================================
	// direction
	// ========================================

		if(positionAdd > 0) // Positive direction
		{
			currCh->actualPosition++;
			*currCh->dirOut->resetBitRegister = currCh->dirOut->bitWeight;
		}
		else // Negative direction
		{
			currCh->actualPosition--;
			*currCh->dirOut->setBitRegister = currCh->dirOut->bitWeight;
		}

		*currCh->stepOut->setBitRegister = currCh->stepOut->bitWeight; // rising edge of step pulse
	}
}

	void StepDir_init(uint32 motorCount)	// todo CHECK 3: See if the workaround for TMC2041 is required on Startrampe
	{
		int i;
		TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
		NVIC_InitTypeDef NVIC_InitStructure;
		UNUSED(motorCount);

		for( i = 0; i < STEP_DIR_NO_CHANNELS; i++)
		{
			CTRL[i].channel.stallSignalIn                = &DummyPin;
			CTRL[i].channel.stepOut                      = &DummyPin;
			CTRL[i].channel.dirOut                       = &DummyPin;

			CTRL[i].channel.actualAcceleration           = 0;
			CTRL[i].channel.actualPosition               = 0;
			CTRL[i].channel.actualVelocity               = 0;
			CTRL[i].channel.maxPositioningSpeed          = 0;
			CTRL[i].channel.rampMode                     = SD_RAMP_MODE_VELOCITY;
			CTRL[i].channel.stopOnStallVelocityTreshold  = 0;
			CTRL[i].channel.targetPosition               = 0;
			CTRL[i].channel.targetVelocity               = 0;
			CTRL[i].channel.haltMask                     = 0;
			CTRL[i].channel.stallFlagIn                  = 0;
			CTRL[i].channel.coolStepVelocityTreshold     = 0;
			CTRL[i].channel.coolStepActiveValue          = 0;
			CTRL[i].channel.coolStepInactiveValue        = 0;
			CTRL[i].channel.coolStepActualValue          = 0;
			CTRL[i].channel.coolStepActivationChanged    = 0;
			CTRL[i].channel.profileFrom                  = 0;
			CTRL[i].channel.profileTo                    = 0;
			CTRL[i].channel.useProfile                   = 0;
		}

		// Timer 2 konfigurieren (zum Erzeugen von Geschwindigkeiten)
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
		TIM_DeInit(TIM2);
		TIM_TimeBaseStructure.TIM_Period         = 457; // for 120MHz clock -> 60MHz
		TIM_TimeBaseStructure.TIM_Prescaler      = 0;
		TIM_TimeBaseStructure.TIM_ClockDivision  = 0;
		TIM_TimeBaseStructure.TIM_CounterMode    = TIM_CounterMode_Up;
		TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
		TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
		TIM_Cmd(TIM2, ENABLE);

		// Timer-Interrupt im NVIC freischalten
		NVIC_InitStructure.NVIC_IRQChannel                    = TIM2_IRQn;
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 1;
		NVIC_InitStructure.NVIC_IRQChannelSubPriority         = 1;
		NVIC_InitStructure.NVIC_IRQChannelCmd                 = ENABLE;
		NVIC_Init(&NVIC_InitStructure);
	}

	void StepDir_deInit()
	{
		TIM_DeInit(TIM2);
	}

