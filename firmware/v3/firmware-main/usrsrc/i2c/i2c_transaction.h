/*
 * i2c_transaction.h
 *
 *  Created on: 3 oct. 2019
 *      Author: droggen
 */

#ifndef I2C_I2C_TRANSACTION_H_
#define I2C_I2C_TRANSACTION_H_

#include <stdio.h>

#define I2C_READ			1
#define I2C_WRITE			0

#define I2C_TRANSACTION_MAX		8
#define I2C_TRANSACTION_MASK	(I2C_TRANSACTION_MAX-1)

struct  _I2C_TRANSACTION;
typedef unsigned char(*I2C_CALLBACK)(struct _I2C_TRANSACTION *);

// Only entries modified by interrupts are marked as volatile
typedef struct  _I2C_TRANSACTION {
	unsigned char address;													// 7-bit address
	unsigned char rw;														// write: 0. read: 1.
	unsigned char data[16];													// data to send/receive
	unsigned char *extdata;													// If nonzero, use this buffer for send/receive, otherwise data is used
	//volatile unsigned char dataack[16];									// acknowledge to send/receive
	unsigned char dodata;													// Send/read the data (otherwise, only the rest is done): specifies the number of bytes, or 0.
	unsigned char dostop;													// Send I2C stop
	I2C_CALLBACK callback;													// Callback to call upon completion or error
	void *user;																// User data

	unsigned char link2next;												// Linked: failure in this transaction cancels the next one; success in this transaction do not call callback, only last one

	// Status information
	volatile unsigned char status;											// Status:	Lower 4 bit indicate status.
																			//				0: success.
																			//				1: failed in start condition.
																			//				2: failed in address.
																			//				3: failed in data (high 4 bit indicated how many bytes were transmitted successfully before the error)
	volatile unsigned char i2cerror;										// Indicate the AVR I2C error code, when status is non-null
} I2C_TRANSACTION;



// Internal stuff
extern I2C_TRANSACTION *i2c_transaction_buffer[I2C_TRANSACTION_MAX];
extern unsigned char _i2c_transaction_buffer_wr, _i2c_transaction_buffer_rd;


// Transaction pool
extern I2C_TRANSACTION _i2c_transaction_pool[I2C_TRANSACTION_MAX];
extern unsigned char _i2c_transaction_pool_alloc[I2C_TRANSACTION_MAX];
extern unsigned char _i2c_transaction_pool_wr, _i2c_transaction_pool_rd;
extern unsigned char _i2c_transaction_pool_reserved;

/******************************************************************************
*******************************************************************************
INTERRUPT-DRIVEN ACCESS   INTERRUPT-DRIVEN ACCESS   INTERRUPT-DRIVEN ACCESS
*******************************************************************************
******************************************************************************/
void i2c_transaction_setup(I2C_TRANSACTION *t,unsigned char addr7,unsigned char mode,unsigned char stop,unsigned char n);
void i2c_transaction_setup_dbg(unsigned char addr7,unsigned char mode,unsigned char stop,unsigned char n);
unsigned char i2c_transaction_execute(I2C_TRANSACTION *trans,unsigned char blocking);
void i2c_transaction_cancel(void);
//
unsigned char i2c_transaction_queue(unsigned char n,unsigned char blocking,...);
unsigned char i2c_transaction_getqueued(void);
void i2c_transaction_removenext(void);

void _i2c_transaction_put(I2C_TRANSACTION *trans);
I2C_TRANSACTION *_i2c_transaction_getnext(void);
void _i2c_transaction_removelinked(I2C_TRANSACTION *);
unsigned char _i2c_transaction_getfree(void);

void i2c_transaction_print(I2C_TRANSACTION *trans,FILE *file);
void i2c_transaction_printall(FILE *file);


/******************************************************************************
*******************************************************************************
TRANSACTION POOL
*******************************************************************************
******************************************************************************/
unsigned char i2c_transaction_pool_reserve(unsigned char n,...);
void i2c_transaction_pool_free(unsigned char n,...);
void i2c_transaction_pool_free1(I2C_TRANSACTION *t);
void i2c_transaction_pool_print(void);
void i2c_transaction_pool_test(void);
void i2c_transaction_pool_test2(void);
void i2c_transaction_pool_test3(void);


#endif /* I2C_I2C_TRANSACTION_H_ */
