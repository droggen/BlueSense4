/*
 * stmi2c-isr.h
 *
 *  Created on: 30 sept. 2019
 *      Author: droggen
 */

#ifndef I2C_STMI2C_ISR_H_
#define I2C_STMI2C_ISR_H_

#include "i2c_transaction.h"

#define I2C_READ			1
#define I2C_WRITE			0

// Caching of current transaction parameters
extern volatile unsigned char _i2c_transaction_idle;																	// Indicates whether a transaction is in progress or idle. Modified by interrupt; read in queue to detect end of transaction
extern I2C_TRANSACTION *_i2c_current_transaction;																		// Current transaction for the interrupt routine.
extern unsigned char _i2c_current_transaction_n;																		// Modified by interrupt, but only used in interrupt
extern unsigned char _i2c_current_transaction_state;																	// Modified by interrupt, but only used in interrupt
extern unsigned char _i2c_current_transaction_dodata;																	// Modified by interrupt, but only used in interrupt
extern unsigned char _i2c_current_transaction_address;																	// Modified by interrupt, but only used in interrupt
extern unsigned char _i2c_current_transaction_rw;																		// Modified by interrupt, but only used in interrupt
extern unsigned char _i2c_current_transaction_dostop;																	// Modified by interrupt, but only used in interrupt
extern I2C_CALLBACK  _i2c_current_transaction_callback;																	// Modified by interrupt, but only used in interrupt
extern volatile unsigned char *_i2c_current_transaction_status,*_i2c_current_transaction_i2cerror;
extern unsigned char *_i2c_current_transaction_data;
//extern I2C_TRANSACTION *i2c_transaction_buffer[I2C_TRANSACTION_MAX];
extern volatile unsigned char _i2c_transaction_idle;


extern volatile unsigned long int i2c_intctr;

extern unsigned char i2c_data[];
extern volatile unsigned char status;											// Status:	Lower 4 bit indicate status.
extern volatile unsigned char i2cerror;										// Indicate the AVR I2C error code, when status is non-null

void I2C_statemachine_outer();
void _I2C_statemachine_handleerror(unsigned char error, unsigned sr1);
void I2C_statemachine_internal();
void I2C_statemachine_internal_tx();
void I2C_statemachine_internal_rx();

#endif /* I2C_STMI2C_ISR_H_ */
