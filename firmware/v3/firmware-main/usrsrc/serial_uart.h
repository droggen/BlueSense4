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
extern volatile unsigned long Serial1Int;					// Total interrupt
extern volatile unsigned long Serial1EvtWithinInt;


#define SERIAL_UART_RX_BUFFERSIZE 512
//#define SERIAL_UART_TX_BUFFERSIZE 512
#define SERIAL_UART_TX_BUFFERSIZE 512

//#define USART_FLAG_CTS   ((uint16_t)0x0200)
//#define USART_FLAG_LBD   ((uint16_t)0x0100)
#define USART_FLAG_TXE		0x80
//#define USART_FLAG_TC   ((uint16_t)0x0040)
#define USART_FLAG_RXNE		0x20
//#define USART_FLAG_IDLE   ((uint16_t)0x0010)
#define USART_FLAG_ORE		0x08
#define USART_FLAG_NF   	0x04
#define USART_FLAG_FE   	0x02
#define USART_FLAG_PE   	0x01


/*typedef struct
{
	SERIALPARAM serialparam;
	USART_TypeDef *h;
} SERIALUARTPARAM;*/


int serial_uart_init(USART_TypeDef *h,int interrupt);
SERIALPARAM *serial_uart_getserialparam(int p);

void USART2_IRQHandler(void);
//void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);


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
void serial_usart_interruptenable(USART_TypeDef *h);
int serial_uart_putchar_block_int(char c, int p);
int serial_uart_getchar_nonblock_int(int p);
int serial_uart_putbuf_int(char *data,unsigned char n,int p);
//int serial_uart_cookiewrite_int(void *__cookie, char *__buf, size_t __nbytes);
ssize_t serial_uart_cookiewrite_int(void *__cookie, const char *__buf, size_t __nbytes);
ssize_t serial_uart_cookieread_int(void *__cookie, char *__buf, size_t __n);

void serial_uart_init_ll(int speed);

// Buffer manip
unsigned char serial_uart_putbuf(SERIALPARAM *sp,char *data,unsigned short n);
unsigned char serial_uart_fischar(SERIALPARAM *sp);
int serial_uart_txbufferfree(int p);
void serial_uart_clearbuffers(void);

void serial_uart_printreg(FILE *f);
void serial_uart_printevents(FILE *f);

#endif // __SERIAL_UART_H



