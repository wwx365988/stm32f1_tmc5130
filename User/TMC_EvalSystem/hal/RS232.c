#include "HAL.h"
#include "RS232.h"

#define BUFFER_SIZE  1024
#define INTR_PRI     6

static void init(void);
static void deInit(void);
static void tx(uint8 ch);
static uint8 rx(uint8 *ch);
static void txN(uint8 *str, unsigned char number);
static uint8 rxN(uint8 *ch, unsigned char number);
static void clearBuffers(void);
static uint32 bytesAvailable(void);

static volatile uint8
	rxBuffer[BUFFER_SIZE],
	txBuffer[BUFFER_SIZE];

static uint32 available = 0;

RXTXTypeDef RS232 =
{
	.init            = init,
	.deInit          = deInit,
	.rx              = rx,
	.tx              = tx,
	.rxN             = rxN,
	.txN             = txN,
	.clearBuffers    = clearBuffers,
	//.baudRate        = 115200,//波特率设置过高，串口助手第一个字符显示乱码
	.baudRate        = 9600,
	.bytesAvailable  = bytesAvailable
};

static RXTXBufferingTypeDef buffers =
{
	.rx =
	{
		.read    = 0,
		.wrote   = 0,
		.buffer  = rxBuffer
	},

	.tx =
	{
		.read    = 0,
		.wrote   = 0,
		.buffer  = txBuffer
	}
};

void __attribute__ ((interrupt)) USART1_IRQHandler(void);

static void init(void)
{
	USART_InitTypeDef UART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

    USART_DeInit(USART1);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	HAL.IOs->config->reset(&HAL.IOs->pins->RS232_RX);
	HAL.IOs->config->reset(&HAL.IOs->pins->RS232_TX);

	USART_StructInit(&UART_InitStructure);
	UART_InitStructure.USART_BaudRate = RS232.baudRate;
	USART_Init(USART1,&UART_InitStructure);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
    /* Enable the USARTy Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;	 
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
//  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = INTR_PRI;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);


	USART_ClearFlag(USART1, USART_FLAG_CTS | USART_FLAG_LBD  | USART_FLAG_TXE  |
	                        USART_FLAG_TC  | USART_FLAG_RXNE | USART_FLAG_IDLE |
	                        USART_FLAG_ORE | USART_FLAG_NE   | USART_FLAG_FE   |
	                        USART_FLAG_PE);
	USART_ITConfig(USART1,USART_IT_PE  ,DISABLE);
	USART_ITConfig(USART1,USART_IT_TXE ,ENABLE);
	USART_ITConfig(USART1,USART_IT_TC  ,ENABLE);
	USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);
	USART_ITConfig(USART1,USART_IT_IDLE,DISABLE);
	USART_ITConfig(USART1,USART_IT_LBD ,DISABLE);
	USART_ITConfig(USART1,USART_IT_CTS ,DISABLE);
	USART_ITConfig(USART1,USART_IT_ERR ,DISABLE);
                      
	USART_Cmd(USART1, ENABLE);
}

static void deInit()
{
	NVIC_InitTypeDef	NVIC_InitStructure;
	USART_Cmd(USART1, DISABLE);
	NVIC_InitStructure.NVIC_IRQChannel     = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd  = DISABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ClearFlag
	(
		USART1,
		0
		| USART_FLAG_CTS
		| USART_FLAG_LBD
		| USART_FLAG_TXE
		| USART_FLAG_TC
		| USART_FLAG_RXNE
		| USART_FLAG_IDLE
		| USART_FLAG_ORE
		| USART_FLAG_NE
		| USART_FLAG_FE
		| USART_FLAG_PE
	);

	clearBuffers();
}

void USART1_IRQHandler(void)
{
	if(USART1->SR & USART_FLAG_RXNE)
	{
		buffers.rx.buffer[buffers.rx.wrote] = USART1->DR;
		buffers.rx.wrote = (buffers.rx.wrote + 1) % BUFFER_SIZE;
		available++;
	}

	//if(USART1->SR & USART_FLAG_TXE)
    if(USART_GetFlagStatus(USART1, USART_FLAG_TC) != SET)
	{
		if(buffers.tx.read != buffers.tx.wrote)
		{
			USART1->DR	= buffers.tx.buffer[buffers.tx.read];
			buffers.tx.read = (buffers.tx.read + 1) % BUFFER_SIZE;
		}
		else
		{
			USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
		}
	}

	if(USART1->SR & USART_FLAG_TC)
		USART_ClearITPendingBit(USART1, USART_IT_TC);
}

static void tx(uint8 ch)
{
	buffers.tx.buffer[buffers.tx.wrote] = ch;
	buffers.tx.wrote = (buffers.tx.wrote + 1) % BUFFER_SIZE;
    
    USART_ClearFlag(USART1,USART_FLAG_TC);
	USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
}

static uint8 rx(uint8 *ch)
{
	if(buffers.rx.read == buffers.rx.wrote)
		return 0;

	*ch = buffers.rx.buffer[buffers.rx.read];
	buffers.rx.read = (buffers.rx.read + 1) % BUFFER_SIZE;
	available--;

	return 1;
}

static void txN(uint8 *str, unsigned char number)
{
	int32 i;
	for( i = 0; i < number; i++)
		tx(str[i]);
}

static uint8 rxN(uint8 *str, unsigned char number)
{
	int32 i;
	if(bytesAvailable() < number)
		return 0;

	for( i = 0; i < number; i++)
		rx(&str[i]);

	return 1;
}

static void clearBuffers(void)
{
	__disable_irq();
	available         = 0;
	buffers.rx.read   = 0;
	buffers.rx.wrote  = 0;

	buffers.tx.read   = 0;
	buffers.tx.wrote  = 0;
	__enable_irq();
}

static uint32 bytesAvailable()
{
	return available;
}

