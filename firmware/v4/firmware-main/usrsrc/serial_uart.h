/*
 * serial_uart.h
 *
 *  Created on: 8 Aug 2019
 *      Author: droggen
 */

#ifndef __SERIAL_UART_H
#define __SERIAL_UART_H

//#include <usart.h.not>
//#include "stm32l4xx_hal_uart.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "main.h"
#include "serial.h"

// Define the number of serial uarts
extern SERIALPARAM _serial_uart_param[];
#define SERIALUARTNUMBER	1

extern int __serialuartallocated;

extern volatile unsigned long Serial1DOR;
extern volatile unsigned long Serial1ERR;
extern volatile unsigned long Serial1NR;					// Data overrun
extern volatile unsigned long Serial1FE;					// Data overrun
extern volatile unsigned long Serial1PE;					// Data overrun
extern volatile unsigned long Serial1TXE;					// Data overrun
extern volatile unsigned long Serial1RXNE;					// Data overrun
extern volatile unsigned long Serial1DMARXInt;
extern volatile unsigned long Serial1DMARXOverrun;
extern volatile unsigned long Serial1DMATXInt;
extern volatile unsigned long Serial1Int;					// Total interrupt
extern volatile unsigned long Serial1EvtWithinInt;

// Circular buffer size
#define SERIAL_UART_RX_BUFFERSIZE 1024
// Should be large enough to hold all the text written in non-blocking mode (e.g. help screen): 2048 is enough.
// Should also be large enough to handle several putbuf packets when streaming data: e.g. audio frame in ascii can be 256*8bytes
// Therefore use 8192.
#define SERIAL_UART_TX_BUFFERSIZE 8192

// DMA buffer size. RTS is toggled when RX buffer level reaches SERIAL_UART_RX_BUFFERSIZE-SERIAL_UART_DMA_RX_BUFFERSIZE.
// A larger DMA buffer reduces the interrupt load and gives more advance warning to the peripheral to stop transmitting data
#define SERIAL_UART_DMA_RX_BUFFERSIZE 128
#define SERIAL_UART_DMA_TX_BUFFERSIZE 128

extern volatile unsigned char _serial_uart_rx_dma_buffer[SERIAL_UART_DMA_RX_BUFFERSIZE];
extern volatile unsigned char _serial_uart_tx_dma_buffer[SERIAL_UART_DMA_TX_BUFFERSIZE];

#define USART_FLAG_TXE		0x80
#define USART_FLAG_RXNE		0x20
#define USART_FLAG_ORE		0x08
#define USART_FLAG_NF   	0x04
#define USART_FLAG_FE   	0x02
#define USART_FLAG_PE   	0x01




int serial_uart_init(USART_TypeDef *h,int dma1_or_int0,int dma1_or_int0_tx);
SERIALPARAM *serial_uart_getserialparam(int p);


int serial_usart_txempty(USART_TypeDef *h);
void serial_usart_waittxempty();

// Non-interrupt functions
void serial_usart_putchar_block(USART_TypeDef *h, char c);
int serial_usart_rxnotempty(USART_TypeDef *h);
unsigned char serial_usart_getchar_block(USART_TypeDef *h);
int serial_usart_getchar_nonblock(USART_TypeDef *h);
void serial_usart_putbuff_block(USART_TypeDef *h, char *buffer,int n);
int serial_usart_getbuff_nonblock(USART_TypeDef *h, char *buffer,int maxn,int timeoutms);

ssize_t serial_uart_cookiewrite_block(void *__cookie, const char *__buf, size_t __nbytes);

int serial_uart_gettxbuflevel(int p);
int serial_uart_getrxbuflevel(int p);

// Interrupt driven functions
void serial_usart_irq(USART_TypeDef *h);
void serial_usart_dma_rx_irq();
void serial_usart_dma_tx_irq();
//void serial_usart_interruptenable(USART_TypeDef *h);
int serial_uart_putchar_block_int(char c, int p);
int serial_uart_getchar_nonblock_int(int p);
int serial_uart_putbuf_int(char *data,unsigned char n,int p);
ssize_t serial_uart_cookiewrite_int(void *__cookie, const char *__buf, size_t __nbytes);
ssize_t serial_uart_cookieread_int(void *__cookie, char *__buf, size_t __n);

void serial_uart_dma_rx(unsigned char serialid);
void serial_uart_dma_tx(unsigned char serialid);

void _serial_usart_rts_set();
void _serial_usart_rts_clear();
unsigned char _serial_usart_cts();
void _serial_usart_enable_debug_irq(unsigned char en);

// Initialisation and change speed
void serial_uart_init_ll(int dma1_or_int0_rx,int dma1_or_int0_tx);
void serial_uart_changespeed(int speed);

// Buffer manip
unsigned char serial_uart_putbuf(SERIALPARAM *sp,char *data,unsigned short n);
unsigned char serial_uart_fischar(SERIALPARAM *sp);
int serial_uart_txbufferfree(int p);
void serial_uart_clearbuffers(void);

void serial_uart_printreg(FILE *f);
void serial_uart_printevents(FILE *f);
void serial_uart_clearevents();

#endif // __SERIAL_UART_H



