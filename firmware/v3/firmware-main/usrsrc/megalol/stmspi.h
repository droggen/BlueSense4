#ifndef __SPI_USART0_H
#define __SPI_USART0_H

#if 0
// STM32F401
#define SPIUSART0_SELECT		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
#define SPIUSART0_DESELECT		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
#endif
#if 1
// STM32L4
#define SPIUSART0_SELECT		HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET);
#define SPIUSART0_DESELECT		HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
#endif


extern unsigned cr2_deflt;

#define SPI_WAITXFER while(!(SPI1->SR&1));				// RXNE	(works)
//#define SPI_WAITXFER while(!(SPI1->SR&2));			// TXE (does not work - TX register is emptied into shift register before data reception is completed)

extern volatile unsigned char *_spiusart0_bufferptr;
extern volatile unsigned short _spiusart0_n;
extern volatile unsigned char _spiusart0_ongoing;
extern void (*_spiusart0_callback)(void);

void stmspi1_stm32l4_init(int spd);
void stmspi1_stm32f4_init(void);
void spiusart0_stm32f4_deinit(void);
void _spiusart0_waitavailandreserve(void);
unsigned char _spiusart0_tryavailandreserve(void);
unsigned char spiusart0_rw_wait_poll_block(unsigned char d);
void spiusart0_rwn_wait_poll_block(unsigned char *ptr,unsigned char n);
unsigned char spiusart0_rwn_try_poll_block(unsigned char *ptr,unsigned char n);
void spiusart0_rwn_wait_int_block(unsigned char *ptr,unsigned char n);
unsigned char spiusart0_rwn_try_int_cb(unsigned char *ptr,unsigned char n,void (*cb)(void));
unsigned char spiusart0_isbusy(void);



#endif
