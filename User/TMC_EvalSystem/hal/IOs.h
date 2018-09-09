#ifndef _IO_H_
	#define _IO_H_

	#include "derivative.h"

	typedef enum {
		IOS_LOW,
		IOS_HIGH,
		IOS_OPEN,
		IOS_NOCHANGE
	} IO_States;

	enum IOsHighLevelFunctions {IO_DEFAULT, IO_DI, IO_AI, IO_DO, IO_PWM, IO_SD, IO_CLK16, IO_SPI};

	typedef struct
	{
		const uint8 DEFAULT;
		const uint8 DI;
		const uint8 AI;
		const uint8 DO;
		const uint8 PWM;
		const uint8 SD;
		const uint8 CLK16;
		const uint8 SPI;
	} IOsHighLevelFunctionTypeDef;

	typedef struct
	{
		GPIOMode_TypeDef   GPIO_Mode;
		GPIOSpeed_TypeDef  GPIO_Speed;
//		GPIOOType_TypeDef  GPIO_OType;
//		GPIOPuPd_TypeDef   GPIO_PuPd;
	} IOPinInitTypeDef;

	typedef struct
	{
		GPIO_TypeDef            *port;
//		__IO uint16             *setBitRegister;
//		__IO uint16             *resetBitRegister;	
		__IO uint32_t             *setBitRegister;
		__IO uint32_t             *resetBitRegister;	
		uint32                      bitWeight;
		unsigned char               bit;
		IOPinInitTypeDef            configuration;
		IOPinInitTypeDef            resetConfiguration;
		enum IOsHighLevelFunctions  highLevelFunction;
	} IOPinTypeDef;

	typedef struct
	{
		void (*set)(IOPinTypeDef *pin);
		void (*copy)(IOPinInitTypeDef *from, IOPinTypeDef*to);
		void (*reset)(IOPinTypeDef *pin);
		void (*toOutput)(IOPinTypeDef *pin);
		void (*toInput)(IOPinTypeDef *pin);

		void (*setHigh)(IOPinTypeDef *pin);
		void (*setLow)(IOPinTypeDef *pin);
		void (*setToState)(IOPinTypeDef *pin, IO_States state);
		unsigned char (*isHigh)(IOPinTypeDef *pin);
		void (*init)(void);
		IOsHighLevelFunctionTypeDef HIGH_LEVEL_FUNCTIONS;
	} IOsTypeDef;

	extern IOsTypeDef IOs;

	// A bit weight of 0 is used to indicate a nonexitant pin
	#define DUMMY_BITWEIGHT 0
	#define IS_DUMMY_PIN(pin) (pin->bitWeight == DUMMY_BITWEIGHT)

#endif /* _IO_H_ */
