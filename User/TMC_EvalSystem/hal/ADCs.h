#ifndef ADC_H
#define ADC_H

	#define N_O_ADC_CHANNELS 6

	typedef struct
	{
		volatile unsigned short *AIN0;
		volatile unsigned short *AIN1;
		volatile unsigned short *AIN2;
		volatile unsigned short *DIO4;
		volatile unsigned short *DIO5;
		volatile unsigned short *VM;
		void (*init)();
		void (*deInit)();
	} ADCTypeDef;

extern	ADCTypeDef ADCs;

#endif /* ADC_H */
