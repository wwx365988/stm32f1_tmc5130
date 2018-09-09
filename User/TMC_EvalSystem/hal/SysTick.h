#ifndef SysTick_H
#define SysTick_H

	#include "helpers/API_Header.h"

	void systick_init(void);
	u32 systick_getTick(void);
	void wait(uint32 delay);
	uint32 timeSince(uint32 tick);

#endif /* SysTick_H */
