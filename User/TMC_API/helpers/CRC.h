/*
 * CRC.h
 *
 *  Created on: 04.12.2017
 *      Author: LH
 */

#ifndef TMC_HELPERS_CRC_H_
#define TMC_HELPERS_CRC_H_

	#include "TypeDefs.h"

	// Amount of CRC tables available
	#define CRC_TABLE_COUNT 2

	uint8 fillCRC8Table(uint8 polynomial, bool isReflected, uint8 index);
	uint8 CRC8(uint8 *data, uint32 bytes, uint8 index);

	uint8 tableGetPolynomial(uint8 index);
	bool tableIsReflected(uint8 index);

#endif /* TMC_HELPERS_CRC_H_ */
