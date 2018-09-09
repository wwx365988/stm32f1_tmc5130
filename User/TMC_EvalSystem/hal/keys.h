#ifndef KEYS_H_
#define KEYS_H_

#define KEY_ON()            *HAL.IOs->pins->KEY_STAT.resetBitRegister   = HAL.IOs->pins->KEY_STAT.bitWeight
#define KEY_OFF()           *HAL.IOs->pins->KEY_STAT.setBitRegister     = HAL.IOs->pins->KEY_STAT.bitWeight
#define KEY_TOGGLE()        HAL.IOs->pins->KEY_STAT.port->ODR           ^= HAL.IOs->pins->KEY_STAT.bitWeight

#define KEY_ERROR_ON()      *HAL.IOs->pins->KEY_ERROR.resetBitRegister  = HAL.IOs->pins->KEY_ERROR.bitWeight
#define KEY_ERROR_OFF()     *HAL.IOs->pins->KEY_ERROR.setBitRegister    = HAL.IOs->pins->KEY_ERROR.bitWeight
#define KEY_ERROR_TOGGLE()  HAL.IOs->pins->KEY_ERROR.port->ODR          ^= HAL.IOs->pins->KEY_ERROR.bitWeight

#include "IOs.h"

typedef struct
{
	void (*on)(void);
	void (*off)(void);
	void (*toggle)(void);
} KEYTypeDef;

typedef struct
{
	void (*init)(void);
	KEYTypeDef stat;
	KEYTypeDef error;
} KEYsTypeDef;

extern	KEYsTypeDef KEYs;

unsigned char Key_Scan(GPIO_TypeDef* GPIOx,u16 GPIO_Pin);


#endif /* KEYS_H_ */
