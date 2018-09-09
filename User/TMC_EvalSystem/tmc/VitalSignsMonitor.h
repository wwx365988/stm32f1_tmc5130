#ifndef VITAL_SIGNS_MONITOR_H
#define VITAL_SIGNS_MONITOR_H

	#include "API_Header.h"

	typedef struct
	{
		int8    debugMode;    // while debugMode is set, the error LED does not get set by VSM and status LED heartrate does not get updated
		int8    busy;         // if module is busy, the status LED is blinking fast
		uint8   brownOut;     // Undervoltage condition flags
		uint8   overVoltage;  // Overvoltage condition flags
		int32   errorMask;    // error mask, each bit stands for one error bit, 1 means error will be reported
		int32   errors;       // actual error bits
		uint32  heartRate;    // status LED blinking frequency
		uint32  VM;           // actual measured motor supply VM
	} VitalSignsMonitorTypeDef;

	extern VitalSignsMonitorTypeDef VitalSignsMonitor; // global implementation of interface for system

	void vitalsignsmonitor_checkVitalSigns(void);
	void vitalsignsmonitor_clearOvervoltageErrors(void);

#endif /* VITAL_SIGNS_MONITOR_H */
