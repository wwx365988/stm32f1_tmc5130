#ifndef SPI_H
#define SPI_H

	#include "derivative.h"
	#include "IOs.h"

	typedef struct
	{
		SPI_TypeDef *periphery; // pointer to ST SPI configuration structure

		IOPinTypeDef *CSN;
		unsigned char (*readWrite) (unsigned char data, unsigned char lastTransfer);
		void (*reset) (void);
	} SPIChannelTypeDef;

	typedef struct
	{
		SPIChannelTypeDef ch1;
		SPIChannelTypeDef ch2;
		void (*init) (void);
	} SPITypeDef;

extern	SPITypeDef SPI;

	// read/write 32 bit value at address
	int32 spi_readInt(SPIChannelTypeDef *SPIChannel, uint8 address);
	void spi_writeInt(SPIChannelTypeDef *SPIChannel, uint8 address, int value);

	// read/write 40 bit datagram (RW, address, value)
	void spi_readDatagram(SPIChannelTypeDef *SPIChannel, TDatagram *datagram);
	void spi_writeDatagram(SPIChannelTypeDef *SPIChannel, TDatagram *datagram);

	// read/write lower/higher 16 bit value at address
	u16 spi_readRegister16BitValue(SPIChannelTypeDef *SPIChannel, uint8 address, u8 channel);
	void spi_writeRegister16BitValue(SPIChannelTypeDef *SPIChannel, u8 address, u8 channel, u16 value);

	// for default channels
	uint8 spi_ch1_readWriteByte(uint8 data, uint8 lastTransfer);

	int32 spi_ch1_readInt(uint8 address);
	void spi_ch1_writeInt(uint8 address, int value);

	void spi_ch1_readDatagram(TDatagram *datagram);
	void spi_ch1_writeDatagram(TDatagram *datagram);

	u16 spi_ch1_readRegister16BitValue(u8 address, u8 channel);
	void spi_ch1_writeRegister16BitValue(u8 address, u8 channel, u16 value);

	int32 spi_ch2_readInt(uint8 address);
	void spi_ch2_writeInt(uint8 address, int value);

	void spi_ch2_readDatagram(TDatagram *datagram);
	void spi_ch2_writeDatagram(TDatagram *datagram);

	u16 spi_ch2_readRegister16BitValue(u8 address, u8 channel);
	void spi_ch2_writeRegister16BitValue(u8 address, u8 channel, u16 value);

#endif /* SPI_H */
