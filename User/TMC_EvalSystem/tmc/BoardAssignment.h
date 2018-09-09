#ifndef BOARD_ASSIGNMENT_H
#define BOARD_ASSIGNMENT_H

#include "boards/Board.h"

	typedef enum {
		FOUND_BY_NONE,
		FOUND_BY_MONOFLOP,
		FOUND_BY_EEPROM
	} IDFinder;

	typedef struct
	{
		uint8 state;          // detection state of this board
		uint8 id;             // id of board
		IDFinder detectedBy;  // Holds the method used to detect the ID (Monoflop or EEPROM)
		uint32 counter_1;     // Timer cycles elapsed on ID pulse rising edge
		uint32 counter_2;     // Timer cycles elapsed on ID pulse falling edge
		uint32 timer_1;       // Current timer value on ID pulse rising edge
		uint32 timer_2;       // Current timer value on ID pulse falling edge
	} IdStateTypeDef;         // interface for id and detection state of a board

	typedef struct
	{
		IdStateTypeDef ch1;  // interface for id and detection state for the driver board
		IdStateTypeDef ch2;  // interface for id and detection state for the motion controller board
	} IdAssignmentTypeDef;   // interface for id and detection state of driver and motion controller board

	extern IdAssignmentTypeDef IdState;

	int32 Board_assign(IdAssignmentTypeDef *ids);     // ids and states of assigned driver and motion controller board
	int32 Board_supported(IdAssignmentTypeDef *ids);  // ids and states of supported driver and motion controller board

	//#include "boards/SelfTest.h"

	// ids for channel 0
	#define ID_ERROR           0
	#define ID_TMC5031         2
	#define ID_TMC4361         4
	#define ID_TMC5130         5
	#define ID_TMC5041         6
	#define ID_TMC5072         7
	#define ID_TMC4670         9
	#define ID_TMC4331         10
	#define ID_TMC4361A        11
	#define ID_TMC8690         12
	#define ID_TMC4671         13
	#define ID_TMC0490         14
	#define ID_TMC4330         15
	#define ID_TMC5160         16
	#define ID_TMC4672         17
	#define ID_TMC5062         25
	#define ID_TMC2130_TQFP48  0xFE
	#define ID_SELFTEST        255

	// ids for channel 1
	#define ID_TMC2660         1
	#define ID_TMC2130         3
	#define ID_TMC2100         4
	#define ID_TMC2041         5
	#define ID_TMC2208         6
	#define ID_TMC2224         7
	#define ID_TMCC160         9

extern void TMC5130_init(void);

#endif /* BOARD_ASSIGNMENT_H */
