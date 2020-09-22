#include <stdio.h>
#include <string.h>

#include "main.h"
#include "wait.h"
#include "helper.h"
#include "stmspi.h"
#include "global.h"
#include "atomicop.h"
#include "math.h"
/*
	File: stmspi
	
	Provides functions to handle SPI communication using SPI1.
	
	
	The key functions are:		
	
	* spiusart0_init: 					setups usart0 for SPI communication
	* spiusart0_rw:						selects the peripheral and exchanges 1 byte. Waits for peripheral available before start, waits for completion.
	 									Do not use in interrupts.
	* spiusart0_rw_wait_poll_block:		selects the peripheral and exchanges n byte. Waits for peripheral available before start, waits for completion.
	 									Do not use in interrupts.
	* spiusart0_rwn_try_poll_block:		Selects the peripheral and exchanges n byte. Tries for peripheral availability before start, waits for completion.
	 									Do not use in interrupts.
	* spiusart0_rwn_wait_int_block:		selects the peripheral and exchanges n byte using interrupt-driven transfer.  Waits for peripheral available before start, waits for completion.
	* 									Do not use in interrupts.
	* spiusart0_rwn_int_cb:				selects the peripheral and exchanges n byte using interrupt-driven transfer. Returns if peripheral not available, calls a callback on transfer completion. Interrupt safe.
	
	Naming convention:
	spiusart0_rwn_<wait|try>_<poll|int>_<block|cb>
		<wait|try>: Waits for peripheral to be available to do operation, or try to reserve peripheral and fail if peripheral not available
		<poll|int>: Main mode of transfer is polling or interrupt-driven
		<block|cb>: Function blocks until the transaction is completed, otherwise returns immediately with a callback called upon completion


	The only function safe to call from an interrupt routing is spiusart0_rwn_int_cb. The other functions can cause deadlocks and must not be used in interrupts.
	
	All functions will wait for the peripheral to be available, except spiusart0_rwn_int_cb which will return immediately with an error if the peripheral is used. This
	allows calls to all functions to be mixed from main code or interrupts (within interrupts only spiusart0_rwn_int_cb is allowed). 
	
	The peripheral is selected with macros SPIUSART0_SELECT and SPIUSART0_DESELECT. When the assembly ISR is used, the file spi-usart0-isr.S must be modified to handle the select/deselect.
	
*/



/*
	Note on interrupts:
	UDR empty interrupt:			executes as long as UDR is empty; must write to UDR or clear UDRIE
	RX complete interrupt:		executes as long as RX complete flag is set; must read UDR to clear interrupt
	TX complete interrupt:		executes once when TX complete flag set; flag automatically cleared
	
	In SPI mode, a transmission and reception occur simultaneously. 
	RXC or TXC could equally be used. Here we use TXC.
	
*/




volatile unsigned char *_spiusart0_bufferptr;
volatile unsigned short _spiusart0_n;
volatile unsigned char _spiusart0_ongoing=0;
void (*_spiusart0_callback)(void);
volatile unsigned int _spiusart0_txint=0;
unsigned cr2_deflt;

extern void spiisr(void);


void SPI1_IRQHandler(void)
{
	// Assumption: triggered on RX
	_spiusart0_txint++;

	// Do something
	//fprintf(file_pri,"SPI %d %d: %08X ",_spiusart0_txint,_spiusart0_n,SPI1->SR);
	//fprintf(file_pri,"spiirq %08X\n",SPI1->SR);
	//unsigned d = SPI1->DR;
	//fprintf(file_pri,"(%08X)\n",d);
	//(void)SPI1->DR;

	// Try
	*_spiusart0_bufferptr = SPI1->DR;
	//fprintf(file_pri,"RD %02X ",*_spiusart0_bufferptr);

	_spiusart0_n--;

	// No more data
	if(_spiusart0_n==0)
	{
		// Deselect
		SPIUSART0_DESELECT;

		// Deactivate int
		SPI1->CR2 = cr2_deflt;

		// Indicate done
		_spiusart0_ongoing=0;

		//dbg_fputchar('I',0);
		//dbg_fputchar('D',0);
		//dbg_fputchar('A'+_spiusart0_bufferptr,0);
		//dbg_fputchar(' ',0);
		//dbg_fputchar('A'+_spiusart0_n,0);
		//dbg_fputchar('\n',0);
		//printf_P("i d %d\n",_spiusart0_txint);

		// Call the callback;
		if(_spiusart0_callback)
			_spiusart0_callback();



		return;
	}

	_spiusart0_bufferptr++;


	// Write next data
	//fprintf(file_pri,"WR %02X\n",*_spiusart0_bufferptr);
	//SPI1->DR = *_spiusart0_bufferptr;
	*(unsigned char *)&(SPI1->DR) = *_spiusart0_bufferptr;			// Force 8-bit write


}


/******************************************************************************
	function: spiusart0_init
*******************************************************************************	
	Initialise USART0 in SPI Master mode.		
******************************************************************************/

void stmspi1_stm32l4_init(int spd)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Enable clocks
	__HAL_RCC_SPI1_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/**SPI1 GPIO Configuration
	PA5     ------> SPI1_SCK
	PA6     ------> SPI1_MISO
	PA7     ------> SPI1_MOSI
	PB0     ------> SPI1_NSS
	*/

	// Activate pins PA5-PA7 as SPI alternate function
	GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


	// Activate pin PB0 (NSS) as GPIO output (SS# under software control)
	GPIO_InitStruct.Pin = SPI1_NSS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(SPI1_NSS_GPIO_Port, &GPIO_InitStruct);

	/*
	// Code of cube: NSS as hardware control
	GPIO_InitStruct.Pin = SPI1_NSS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(SPI1_NSS_GPIO_Port, &GPIO_InitStruct);
	*/

	// Deselect
	SPIUSART0_SELECT;
	HAL_Delay(1);
	SPIUSART0_DESELECT;
	HAL_Delay(1);


/*
	// Cube initialisation
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 7;
	hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
*/




	SPI1->CR1 = 0x0000;			// Deactivate SPI / reset to defaults
	SPI1->CR2 = 0x0700;			// Reset to defaults

	// Invensense datasheet:
	//	2. Data is latched on the rising edge of SCLK
	//	3. Data should be transitioned on the falling edge of SCLK
	// CPOL=0:		CK=0 when idle
	// CPOL=1:		CK=1 when idle
	// CPHA=0:		First clock transition is first data capture edge
	// CPHA=1:		Second clock transition is first data capture edge
	unsigned cpol=1,cpha=1,mstr=1;
	//unsigned br = 0b010;			// divider: 010 = fPCLK/8		20MHz -> 2.5MHz
	//unsigned br = 0b011;			// divider: 011 = fPCLK/16		20MHz -> 1.25MHz
	//unsigned br = 0b100;			// divider: 100 = fPCLK/32		20MHz -> 625KHz
	//unsigned br = 0b101;			// divider: 101 = fPCLK/64
	//unsigned br = 0b111;			// divider: 111 = fPCLK/256

	// Find highest frequency below spd
	// Frequency: f = pclk/2^(1+br)
	// spd = pclk/2^(1+br) -> pclk/spd = 2^(1+br)
	unsigned br;
	br = ceil(log2((float)HAL_RCC_GetPCLK2Freq()/(float)spd)) - 1;
	br&=0b111;		// Sanity check
	//fprintf(file_pri,"br: %d\n",br2);
	//fprintf(file_pri,"PCLK2 frq: %u\n",HAL_RCC_GetPCLK2Freq());




	// CR2: Cube: 	170C		0001 0111 0000 1100

	// First set CR2 as some bits can only be changed if interface disabled.
	// DMA disabled             --
	// SS enabled              - I
	// No NSS pulse           -  I
	// Motorola              -I  I
	// No int             --- I  I
	// 8-bit          ----    I  I
	// FIFO 1/4      -        I  I
	// DMA OFF    ---         I  I
	cr2_deflt = 0b0001011100000100;
	SPI1->CR2 = cr2_deflt;

	// Set CR1 and enable interface
	//			SSM		SSI		SPE
	SPI1->CR1 = (1<<9)|(1<<8)|(1<<6)|(br<<3)|(mstr<<2)|(cpol<<1)|(cpha<<0);




	_spiusart0_ongoing=0;


	//SPI1->CR2 = 0x40;	// Enable interrupts RX
	//SPI1->CR2 = 0x80;	// Enable interrupts TX


	// Enable NVIC interrupt
	HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(SPI1_IRQn);


	fprintf(file_pri,"SPI fPCLK: %lu SCK: %lu\n",HAL_RCC_GetPCLK2Freq(),HAL_RCC_GetPCLK2Freq()/(2<<br));

	//fprintf(file_pri,"SPI1: CR1: %028X CR2: %08X\n",SPI1->CR1,SPI1->CR2);
}
void stmspi1_stm32f4_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};


	__HAL_RCC_SPI1_CLK_ENABLE();

	//__HAL_RCC_GPIOA_CLK_ENABLE();		// This is already enabled

	// Activate pins PA5-PA7 as SPI alternate function
	GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	// Activate pin PA4 (NSS) as output
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Deselect
	SPIUSART0_SELECT;
	HAL_Delay(1);
	SPIUSART0_DESELECT;
	HAL_Delay(1);



	SPI1->CR1 = 0x00000000;		// Deactivate SPI / reset to defaults
	SPI1->CR2 = 0x00000000;

	unsigned cpol=1,cpha=1,mstr=1;
	unsigned br = 0b011;			// divider: 011 = fPCLK/16		20MHz -> 1.25MHz
	//unsigned br = 0b100;			// divider: 100 = fPCLK/32		20MHz -> 625KHz
	//unsigned br = 0b101;			// divider: 101 = fPCLK/64
	//unsigned br = 0b111;			// divider: 111 = fPCLK/256


	//			SSM		SSI		SPE
	SPI1->CR1 = (1<<9)|(1<<8)|(1<<6)|(br<<3)|(mstr<<2)|(cpol<<1)|(cpha<<0);

	_spiusart0_ongoing=0;
	

	//SPI1->CR2 = 0x40;	// Enable interrupts RX
	//SPI1->CR2 = 0x80;	// Enable interrupts TX


	// Enable NVIC interrupt
	HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(SPI1_IRQn);


	fprintf(file_pri,"SPI fPCLK: %lu SCK: %lu\n",HAL_RCC_GetPCLK2Freq(),HAL_RCC_GetPCLK2Freq()/(2<<br));

	//fprintf(file_pri,"SPI1: CR1: %028X CR2: %08X\n",SPI1->CR1,SPI1->CR2);
}

void spiusart0_stm32f4_deinit(void)
{
	// TOFIX: should wait for last transfer to complete

	// Deinitialise the SPI interface
	SPI1->CR1 = 0x00000000;
	SPI1->CR2 = 0x00000000;
}

/******************************************************************************
	function: _spiusart0_waitavailandreserve
*******************************************************************************	
	Waits until no SPI transaction occurs (_spiusart0_ongoing=0) and 
	immediately indicates a transaction is ongoing (_spiusart0_ongoing=1).
	Interrupts are managed to ensure no interrupt-initiated transaction can sneak
	in.	
******************************************************************************/
void _spiusart0_waitavailandreserve(void)
{
	unsigned cnt = 0;
	// Wait for end of interrupt transfer if any transfer is ongoing
	spiusart0_waitavailandreserve_wait:
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(!_spiusart0_ongoing)
		{
			_spiusart0_ongoing=1;
			goto spiusart0_waitavailandreserve_start;
		}
	}
	// Ensures opportunities to manage interrupts
	// TOFIX: alternative to _delay_us needed
	//_delay_us(1);				// 1 byte at 1MHz is 1uS.
	cnt++;						// hack - ideally should not be optimised out
	goto spiusart0_waitavailandreserve_wait;
	
	// Start of transmission
	spiusart0_waitavailandreserve_start:
	return;
}
/******************************************************************************
	function: _spiusart0_tryavailandreserve
*******************************************************************************	
	Checks availability of SPI peripheral and reserves it, otherwise returns error.
	
	Checks that no SPI transaction occurs (_spiusart0_ongoing=0) and 
	immediately indicates a transaction is ongoing (_spiusart0_ongoing=1) and returns
	success. Otherwise returns error.
	
	Interrupts are managed to ensure no interrupt-initiated transaction can sneak
	in.	
	
	Return:
		0:		OK, peripheral reserved
		1:		Error reserving peripheral
******************************************************************************/
unsigned char _spiusart0_tryavailandreserve(void)
{	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if any transfer is ongoing: if yes, return error
		if(_spiusart0_ongoing)
		{
			return 1;
		}
		// No transfer ongoing: reserve and return success.
		_spiusart0_ongoing=1;
	}
	return 0;
}


/******************************************************************************
	function: spiusart0_rw_wait_poll_block
*******************************************************************************	
	Select peripheral, read/write 1 byte, deselect peripheral.
	
	Waits until the SPI interface is available before initiating the transfer,
	and waits until completion of the transfer before returning.	
	
	Do not call in an interrupt routine as this can lead to a deadlock.
	
	Parameters:
		data	-	Data to send to peripheral
	
	Returns:
		Data read from peripheral
******************************************************************************/
unsigned char spiusart0_rw_wait_poll_block(unsigned char data)
{
	// Wait for end of interrupt transfer if any transfer is ongoing
	_spiusart0_waitavailandreserve();

	SPIUSART0_SELECT;

	// Put data into buffer, sends the data
	//SPI1->DR = data;
	*(unsigned char *)&(SPI1->DR) = data;			// Force 8-bit write
	

	// Wait for data to be received
	//while(!(SPI1->SR&1));			// Wait until RXNE (rx not empty) is set
	SPI_WAITXFER;

	// Get the data
	data = SPI1->DR;
	
	SPIUSART0_DESELECT;

	// Available again
	_spiusart0_ongoing=0;
	return data;			
}

/******************************************************************************
	function: spiusart0_rwn_block_poll
*******************************************************************************	
	Read/write n bytes in the provided buffer. 
	
	Waits until the SPI interface is available before initiating the transfer,
	and waits until completion of the transfer before returning.			
	
	Do not call in an interrupt routine as this can lead to a deadlock.
	
	Parameters:
		ptr					-	Pointer where the data to send and receive is stored.
		n					-	Number of bytes to exchange

	Returns:
		-
******************************************************************************/
void spiusart0_rwn_wait_poll_block(unsigned char *ptr,unsigned char n)
{
	//fprintf(file_pri,"spiusart0_rwn_wait_poll_block\n");
	// Wait for end of interrupt transfer if any transfer is ongoing
	_spiusart0_waitavailandreserve();
	
	SPIUSART0_SELECT;
	//SPIUSART0_DESELECT;
	
	while(n!=0)
	{
		// Put data into buffer, sends the data
		// In STM32L4 by default a 16-bit write occurs. Casting to ensure only 8-bits are read.
		//SPI1->DR = *ptr;
		*(unsigned char *)&(SPI1->DR) = *ptr;			// Force 8-bit write
		

		// Wait for data to be received
		//fprintf(1,file_pri,"wait RXNE %04X\n",SPI1->SR);
		SPI_WAITXFER;
		
		//unsigned t = SPI1->DR;
		//fprintf(file_pri,"%08X ",t);
		//*ptr=*(unsigned char *)&(SPI1->DR);				// Force 8-bit read
		*ptr=SPI1->DR;										// 8-bit read is not necessary
		//*ptr=t;
		//*ptr=SPI1->DR;
		ptr++;
		n--;
	}
	//fprintf(file_pri,"\n");
	
	SPIUSART0_DESELECT;
	
	_spiusart0_ongoing=0;

	return;				// To avoid compiler complaining
}
/******************************************************************************
	function: spiusart0_rwn_try
*******************************************************************************	
	Read/write n bytes in the provided buffer. 
	
	Check if the SPI interface is available, if not returns an error.
	If the peripheral is available, mark is as used to prevent other functions using it
	and performs a blocking transfer. 
	The function returns once all the data has been transferred.
	
	
	Suitable for use in interrupts.
	
	Parameters:
		ptr					-	Pointer where the data to send and receive is stored.
		n					-	Number of bytes to exchange

	Returns:
		0					-	Success
		1					-	Error, peripheral not available
******************************************************************************/
unsigned char spiusart0_rwn_try_poll_block(unsigned char *ptr,unsigned char n)
{
	// Wait for end of interrupt transfer if any transfer is ongoing
	if(_spiusart0_tryavailandreserve())
		return 1;
	
	SPIUSART0_SELECT;
	
	while(n!=0)
	{
		// Put data into buffer, sends the data
		*(unsigned char *)&(SPI1->DR) = *ptr;			// Force 8-bit write
		
		// Wait for data to be received
		//while(!(SPI1->SR&1));			// Wait until RXNE (rx not empty) is set
		SPI_WAITXFER;
		
		*ptr=SPI1->DR;
		ptr++;
		n--;
	}
	
	SPIUSART0_DESELECT;
	
	_spiusart0_ongoing=0;

	return 0;
}


/******************************************************************************
	function: spiusart0_rwn_block_int
*******************************************************************************	
	Read/write n bytes in the provided buffer using interrupts. 
	
	Waits until the SPI interface is available before initiating the transfer,
	and waits until completion of the transfer before returning.					
	
	Do not call in an interrupt routine as this can lead to a deadlock.
	
	Parameters:
		ptr					-	Pointer where the data to send and receive is stored.
		n					-	Number of bytes to exchange
		keepfirstrx			-	1 to keep the first received byte. This byte is generally meaningless.
								0 to discard the first received byte. 
								By default the first received byte is discarded.
								Discarding the first received byte is a convenience function but
								is slightly slower as additional memory copies are incurred.
	Returns:
		-
	
	
******************************************************************************/
void spiusart0_rwn_wait_int_block(unsigned char *ptr,unsigned char n)
{
	//fprintf(file_pri,"spiusart0_rwn_wait_int_block\n");
	// Wait for end of interrupt transfer if any transfer is ongoing
	_spiusart0_waitavailandreserve();
		
	_spiusart0_txint=0;
	_spiusart0_bufferptr = ptr;
	_spiusart0_n=n;
	_spiusart0_ongoing=1;
	_spiusart0_callback=0;
	
	SPI1->CR2 |= 0x40;	// Enable interrupts RX

	// Start the transfer
	SPIUSART0_SELECT;
	
	// Send the first byte
	//SPI1->DR = *ptr;
	*(unsigned char *)&(SPI1->DR) = *ptr;			// Force 8-bit write
	//fprintf(file_pri,"WR %02X\n",*ptr);

	// Wait completion. BUG: this is suboptimal as an interrupt could occur and restart another transaction, making this wait longer than needed.
	//fprintf(file_pri,"spiusart0_rwn_wait_int_block: wait\n");
	while(_spiusart0_ongoing);
		//HAL_Delay(1);
	
	//if(!keepfirstrx)
	//{
		// We wish to discard the first byte - shift left by one the exchange buffer
		//for(unsigned char i=1;i<n;i++)
			//ptr[i-1]=ptr[i];
	//}
	
	// Interrupts are automatically deactivated by interrupt handler
	
}
/******************************************************************************
	spiusart0_rwn_try_int_cb
*******************************************************************************	
	Read/write n bytes in the provided buffer using interrupts. 
	
	Does return immediately if the SPI interface is busy. A callback is called 
	upon completion of the transfer.
	
	This function can be called in an interrupt safely.
	
	Return:
			0:		Transaction initiated
			1:		Error initiating transaction
******************************************************************************/
/*unsigned char spiusart0_rwn_int_cb(unsigned char *ptr,unsigned char n,void (*cb)(void))
{
	// TOFIX
	spiusart0_rwn(ptr,n);
	cb();
	return 0;
}*/
unsigned char spiusart0_rwn_try_int_cb(unsigned char *ptr,unsigned char n,void (*cb)(void))
{
	//if(_spiusart0_ongoing)
		//return 1;
	if(_spiusart0_tryavailandreserve())
		return 1;

	_spiusart0_txint=0;
	_spiusart0_bufferptr = ptr;
	_spiusart0_n=n;
	_spiusart0_ongoing=1;
	_spiusart0_callback=cb;
	//_spiusart0_callback=dummycb;
	//_spiusart0_callback=cb;
	
	SPI1->CR2 |= 0x40;	// Enable interrupts RX

	// Start the transfer
	SPIUSART0_SELECT;
	
	// Send the first byte
	//SPI1->DR = *ptr;
	*(unsigned char *)&(SPI1->DR) = *ptr;			// Force 8-bit write

	// Transfer handled by interrupt

	return 0;
}

/******************************************************************************
	function: spiusart0_isbusy
*******************************************************************************	
	Indicates if an interrupt-driven transaction is ongoing		
******************************************************************************/
unsigned char spiusart0_isbusy(void)
{
	return _spiusart0_ongoing;
}





