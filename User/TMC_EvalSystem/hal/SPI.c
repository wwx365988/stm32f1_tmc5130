#include "HAL.h"
#include "SPI.h"

static void init(void);
static void reset_ch1(void);
static void reset_ch2(void);

static unsigned char readWrite(SPIChannelTypeDef *SPIChannel, uint8 data, uint8 lastTransfer);
static unsigned char spi_ch1_readWrite(uint8 data, uint8 lastTransfer);
static unsigned char spi_ch2_readWrite(uint8 data, uint8 lastTransfer);

SPIChannelTypeDef *SPIChannel_1_default;
SPIChannelTypeDef *SPIChannel_2_default;

static IOPinTypeDef IODummy = { .bitWeight = DUMMY_BITWEIGHT };

SPITypeDef SPI=
{
	.ch1 =
	{
		.periphery  = SPI3,
		.CSN        = &IODummy,
		.readWrite  = spi_ch1_readWrite,
		.reset      = reset_ch1
	},

	.ch2 =
	{
		.periphery  = SPI2,
		.CSN        = &IODummy,
		.readWrite  = spi_ch2_readWrite,
		.reset      = reset_ch2
	},
	.init = init
};


static void init(void)
{
	SPI_InitTypeDef SPIInit;

	//-------- SPI2 initialisieren und mit den Pins verbinden ---------
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

	//SPI initialisieren
	SPIInit.SPI_Direction          = SPI_Direction_2Lines_FullDuplex;
	SPIInit.SPI_Mode               = SPI_Mode_Master;
	SPIInit.SPI_DataSize           = SPI_DataSize_8b;
	SPIInit.SPI_CPOL               = SPI_CPOL_High;
	SPIInit.SPI_CPHA               = SPI_CPHA_2Edge;
	SPIInit.SPI_NSS                = SPI_NSS_Soft;
	SPIInit.SPI_BaudRatePrescaler  = SPI_BaudRatePrescaler_8;
	SPIInit.SPI_FirstBit           = SPI_FirstBit_MSB;
	SPIInit.SPI_CRCPolynomial      = 0;
	SPI_Init(SPI2, &SPIInit);
	SPI_Cmd(SPI2, ENABLE);

//	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2);
//	GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_SPI2);
//	GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2);

	//-------- SPI3 initialisieren und mit den Pins verbinden ---------
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);

	//SPI initialisieren
	SPIInit.SPI_Direction          = SPI_Direction_2Lines_FullDuplex;
	SPIInit.SPI_Mode               = SPI_Mode_Master;
	SPIInit.SPI_DataSize           = SPI_DataSize_8b;
	SPIInit.SPI_CPOL               = SPI_CPOL_High;
	SPIInit.SPI_CPHA               = SPI_CPHA_2Edge;
	SPIInit.SPI_NSS                = SPI_NSS_Soft;
	SPIInit.SPI_BaudRatePrescaler  = SPI_BaudRatePrescaler_32;
	SPIInit.SPI_FirstBit           = SPI_FirstBit_MSB;
	SPIInit.SPI_CRCPolynomial      = 0;
	SPI_Init(SPI3, &SPIInit);
	SPI_Cmd(SPI3, ENABLE);

//	GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_SPI3);
//	GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_SPI3);
//	GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_SPI3);

	reset_ch1();
	reset_ch2();

	// configure default SPI channel_1
	SPIChannel_1_default = &HAL.SPI->ch1;
	SPIChannel_1_default->CSN = &HAL.IOs->pins->SPI1_CSN;
	// configure default SPI channel_2
	SPIChannel_2_default = &HAL.SPI->ch2;
	SPIChannel_2_default->CSN = &HAL.IOs->pins->SPI2_CSN0;
}

static void reset_ch1()
{
	SPI.ch1.CSN        = &IODummy;
	SPI.ch1.periphery  = SPI3;
	SPI.ch1.readWrite  = spi_ch1_readWrite;
}

static void reset_ch2()
{
	SPI.ch2.CSN        = &IODummy;
	SPI.ch2.periphery  = SPI2;
	SPI.ch2.readWrite  = spi_ch2_readWrite;
}

int32 spi_readInt(SPIChannelTypeDef *SPIChannel, uint8 address)
{
	// clear write bit
	int value;
	address &= 0x7F;

	SPIChannel->readWrite(address, FALSE);
	value = SPIChannel->readWrite(0, FALSE);
	value <<= 8;
	value |= SPIChannel->readWrite(0, FALSE);
	value <<= 8;
	value |= SPIChannel->readWrite(0, FALSE);
	value <<= 8;
	value |= SPIChannel->readWrite(0, TRUE);

	return value;
}

int32 spi_ch1_readInt(uint8 address)
{
	return spi_readInt(SPIChannel_1_default, address);
}

int32 spi_ch2_readInt(uint8 address)
{
	return spi_readInt(SPIChannel_2_default, address);
}

void spi_writeInt(SPIChannelTypeDef *SPIChannel, uint8 address, int value)
{
	SPIChannel->readWrite(address | 0x80, FALSE);
	SPIChannel->readWrite(0xFF & (value>>24), FALSE);
	SPIChannel->readWrite(0xFF & (value>>16), FALSE);
	SPIChannel->readWrite(0xFF & (value>>8), FALSE);
	SPIChannel->readWrite(0xFF & (value>>0), TRUE);
}

void spi_ch1_writeInt(uint8 address, int value)
{
	spi_writeInt(SPIChannel_1_default, address, value);
}

void spi_ch2_writeInt(uint8 address, int value)
{
	spi_writeInt(SPIChannel_2_default, address, value);
}

void spi_readDatagram(SPIChannelTypeDef *SPIChannel, TDatagram *datagram)
{
	// clear write bit
	datagram->address &= 0x7F;
	datagram->value.int32 = spi_readInt(SPIChannel, datagram->address);
}

void spi_ch1_readDatagram(TDatagram *datagram)
{
	// clear write bit
	datagram->address &= 0x7F;
	datagram->value.int32 = spi_readInt(SPIChannel_1_default, datagram->address);
}

void spi_ch2_readDatagram(TDatagram *datagram)
{
	// clear write bit
	datagram->address &= 0x7F;
	datagram->value.int32 = spi_readInt(SPIChannel_2_default, datagram->address);
}

void spi_writeDatagram(SPIChannelTypeDef *SPIChannel, TDatagram *datagram)
{
	spi_writeInt(SPIChannel, datagram->address, datagram->value.int32);
}

void spi_ch1_writeDatagram(TDatagram *datagram)
{
	spi_writeInt(SPIChannel_1_default, datagram->address, datagram->value.int32);
}

void spi_ch2_writeDatagram(TDatagram *datagram)
{
	spi_writeInt(SPIChannel_2_default, datagram->address, datagram->value.int32);
}

u16 spi_readRegister16BitValue(SPIChannelTypeDef *SPIChannel, u8 address, u8 channel)
{
	TDatagram datagram;
	datagram.address = address;
	datagram.value.int32 = 0;

	spi_readDatagram(SPIChannel, &datagram);

	// read one channel
	switch(channel)
	{
	case 0:
		return datagram.value.int32 & 0xFFFF;
		break;
	case 1:
		return (datagram.value.int32 >> 16) & 0xFFFF;
		break;
	}
	return 0;
}

u16 spi_ch1_readRegister16BitValue(u8 address, u8 channel)
{
	return spi_readRegister16BitValue(SPIChannel_1_default, address, channel);
}

u16 spi_ch2_readRegister16BitValue(u8 address, u8 channel)
{
	return spi_readRegister16BitValue(SPIChannel_2_default, address, channel);
}

void spi_writeRegister16BitValue(SPIChannelTypeDef *SPIChannel, u8 address, u8 channel, u16 value)
{
	// read actual register content
	TDatagram datagram;
	datagram.address = address;
	datagram.value.int32 = 0;
	spi_readDatagram(SPIChannel, &datagram);

	// update one channel
	switch(channel)
	{
	case 0:
		datagram.value.int32 &= 0xFFFF0000;
		datagram.value.int32 |= value;
		break;
	case 1:
		datagram.value.int32 &= 0x0000FFFF;
		datagram.value.int32 |= (value << 16);
		break;
	}

	// update the address
	datagram.address = address;

	// write the register
	spi_writeDatagram(SPIChannel, &datagram);
}

void spi_ch1_writeRegister16BitValue(u8 address, u8 channel, u16 value)
{
	spi_writeRegister16BitValue(SPIChannel_1_default, address, channel, value);
}

void spi_ch2_writeRegister16BitValue(u8 address, u8 channel, u16 value)
{
	spi_writeRegister16BitValue(SPIChannel_2_default, address, channel, value);
}

static unsigned char spi_ch1_readWrite(unsigned char data, unsigned char lastTransfer)
{
	 return readWrite(&SPI.ch1, data, lastTransfer);
}

static unsigned char spi_ch2_readWrite(unsigned char data, unsigned char lastTransfer)
{
	 return readWrite(&SPI.ch2, data,lastTransfer);
}

uint8 spi_ch1_readWriteByte(uint8 data, uint8 lastTransfer)
{
	return readWrite(SPIChannel_1_default, data, lastTransfer);
}

static unsigned char readWrite(SPIChannelTypeDef *SPIChannel, uint8 data, uint8 lastTransfer)
{
	if(IS_DUMMY_PIN(SPIChannel->CSN))
		return 0;

	HAL.IOs->config->setLow(SPIChannel->CSN);

	while(SPI_I2S_GetFlagStatus(SPIChannel->periphery, SPI_I2S_FLAG_TXE) == RESET) {};
	SPI_I2S_SendData(SPIChannel->periphery, data);
	while(SPI_I2S_GetFlagStatus(SPIChannel->periphery, SPI_I2S_FLAG_RXNE) == RESET) {};
	if(lastTransfer)
		HAL.IOs->config->setHigh(SPIChannel->CSN);

	return SPI_I2S_ReceiveData(SPIChannel->periphery);
}
