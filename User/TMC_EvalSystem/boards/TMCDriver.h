#ifndef TMCDRIVER_H_
#define TMCDRIVER_H_

	#include "Board.h"

	typedef struct
	{
		ConfigurationTypeDef config;
	} EvalBoardDriverTypeDef;



	void tmcdriver_init(void);

#endif /* TMCDRIVER_H_ */
