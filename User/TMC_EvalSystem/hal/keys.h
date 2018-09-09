#ifndef KEYS_H_
#define KEYS_H_

#define KEY_ON()            *HAL.IOs->pins->KEY_V_ZERO.resetBitRegister   = HAL.IOs->pins->KEY_V_ZERO.bitWeight
#define KEY_OFF()           *HAL.IOs->pins->KEY_V_ZERO.setBitRegister     = HAL.IOs->pins->KEY_V_ZERO.bitWeight
#define KEY_TOGGLE()        HAL.IOs->pins->KEY_V_ZERO.port->ODR           ^= HAL.IOs->pins->KEY_V_ZERO.bitWeight

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

void handle_all_key(void);


#endif /* KEYS_H_ */
