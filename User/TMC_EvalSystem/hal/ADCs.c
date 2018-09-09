#include <math.h>
#include "HAL.h"
#include "ADCs.h"

#define ADC1_DR_ADDRESS  ((uint32)0x4001204C)
static volatile unsigned short ADCValue[N_O_ADC_CHANNELS];
static void init(void);
static void deInit(void);

ADCTypeDef ADCs =
{
	.AIN0   = &ADCValue[0],
	.AIN1   = &ADCValue[1],
	.AIN2   = &ADCValue[2],
	.DIO4   = &ADCValue[3],
	.DIO5   = &ADCValue[4],
	.VM     = &ADCValue[5],
	.init   = init,
	.deInit  = deInit
};

void init(void)
{
	ADC_InitTypeDef ADC_InitStructure;
	//ADC_CommonInitTypeDef ADC_CommonInitStructure;
	DMA_InitTypeDef DMA_InitStructure;

	//ADC_DeInit(ADC1);

	/* Enable peripheral clocks *************************************************/
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

//	HAL.IOs->config->reset(&HAL.IOs->pins->AIN0);
//	HAL.IOs->config->reset(&HAL.IOs->pins->AIN1);
//	HAL.IOs->config->reset(&HAL.IOs->pins->AIN2);
//	HAL.IOs->config->reset(&HAL.IOs->pins->VM_MEAS);


	/* DMA2_Stream0 channel0 configuration **************************************/
	DMA_DeInit(DMA2_Channel1);
	//DMA_InitStructure.DMA_Channel             = DMA_Channel_0;
	DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32)ADC1_DR_ADDRESS;
	DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32)&ADCValue;
	DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize          = N_O_ADC_CHANNELS;
	DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode                = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority            = DMA_Priority_High;
//	DMA_InitStructure.DMA_FIFOMode            = DMA_FIFOMode_Disable;
//	DMA_InitStructure.DMA_FIFOThreshold       = DMA_FIFOThreshold_HalfFull;
//	DMA_InitStructure.DMA_MemoryBurst         = DMA_MemoryBurst_Single;
//	DMA_InitStructure.DMA_PeripheralBurst     = DMA_PeripheralBurst_Single;
  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA2_Channel1, &DMA_InitStructure);
	/* DMA2_Stream0 enable */
	DMA_Cmd(DMA2_Channel1, ENABLE);

	/* ADC1 Init ****************************************************************/
	/* ADC1 configuration */	
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;			//
	ADC_InitStructure.ADC_ScanConvMode = ENABLE ; 	 				//
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;			//
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 	//
	ADC_InitStructure.ADC_NbrOfChannel = 4;	 								//
	ADC_Init(ADC1, &ADC_InitStructure);
	
	RCC_ADCCLKConfig(RCC_PCLK2_Div8); 

	/* Enable ADC1 DMA */
	ADC_DMACmd(ADC1, ENABLE);

	/* ADC1 regular channel configuration ******************************/
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 3, ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 4, ADC_SampleTime_55Cycles5);

	/* Enable ADC1 DMA */
	ADC_DMACmd(ADC1, ENABLE);
	
	/* Enable ADC1 */
	ADC_Cmd(ADC1, ENABLE);
	
  
	ADC_ResetCalibration(ADC1);

	while(ADC_GetResetCalibrationStatus(ADC1));
	
	ADC_StartCalibration(ADC1);
	
	while(ADC_GetCalibrationStatus(ADC1));
	
	
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

static void deInit(void)
{
	 ADC_DeInit(ADC1);
}
