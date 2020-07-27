/*
 * stmi2c-isr.c

 *
 *  Created on: 30 sept. 2019
 *      Author: droggen
 */
#include <stdio.h>
#include <string.h>
#include "usrmain.h"
#include "global.h"
#include "i2c.h"
#include "stmi2c-isr.h"

#include "system.h"

// Debug options: #define I2CDBG_SM
// Debug options: #define I2CDBG_SM_ERR
//#define I2CDBG_SM 1
#define I2CDBG_SM_ERR 1

// Caching of current transaction parameters
volatile unsigned char _i2c_transaction_idle=1;																	// Indicates whether a transaction is in progress or idle. Modified by interrupt; read in queue to detect end of transaction
I2C_TRANSACTION *_i2c_current_transaction;																	// Current transaction for the interrupt routine.
unsigned char _i2c_current_transaction_n;																		// Modified by interrupt, but only used in interrupt
unsigned char _i2c_current_transaction_state;																	// Modified by interrupt, but only used in interrupt
unsigned char _i2c_current_transaction_dodata;																	// Modified by interrupt, but only used in interrupt
unsigned char _i2c_current_transaction_address;																	// Modified by interrupt, but only used in interrupt
unsigned char _i2c_current_transaction_rw;																		// Modified by interrupt, but only used in interrupt
unsigned char _i2c_current_transaction_dostop;																	// Modified by interrupt, but only used in interrupt
I2C_CALLBACK  _i2c_current_transaction_callback;																// Modified by interrupt, but only used in interrupt
volatile unsigned char *_i2c_current_transaction_status,*_i2c_current_transaction_i2cerror;
unsigned char *_i2c_current_transaction_data;


volatile unsigned char status;											// Status:	Lower 4 bit indicate status.
volatile unsigned char i2cerror;										// Indicate the AVR I2C error code, when status is non-null


volatile unsigned long int i2c_intctr=0;



void I2C1_EV_IRQHandler()
{
#if I2CDBG_SM==1
	//itmprintf("Iint\n");
	fprintf(file_pri,"Iint %08X\n",(unsigned)I2C1->ISR);
#endif

	//I2C_statemachine_internal();
	I2C_statemachine_outer();
}
void I2C1_ER_IRQHandler()
{
#if I2CDBG_SM_ERR==1
	//itmprintf("Ierr\n");
	fprintf(file_pri,"Ierr %08X\n",(unsigned)I2C1->ISR);
#endif

	//I2C_statemachine_internal();
	//I2C_statemachine_outer();
}



/******************************************************************************
	I2C_statemachine_outer
*******************************************************************************
	Outer state machine loading transactions to be executed by I2C_statemachine_internal
******************************************************************************/
void I2C_statemachine_outer()
{
	I2C_statemachine_outer_start:

	//#if I2CDBG
		//fprintf(file_pri,"i %d %d\n",_i2c_current_transaction,_i2c_transaction_idle);
	//#endif
	if(_i2c_transaction_idle==1)
	{
		// Idle: check if next transaction to queue
		_i2c_current_transaction = _i2c_transaction_getnext();	// Current transaction
		#if I2CDBG
			//fprintf(file_pri,"D %08X\n",_i2c_current_transaction);
		#endif
		//fprintf(file_pri,"try load new trans %d\n",_i2c_current_transaction);
		if(_i2c_current_transaction==0)
		{
			//fprintf(file_pri,"D return\n");
			return;																			// No next transaction
		}
		//fprintf(file_pri,"loading new trans %d\n",_i2c_current_transaction);
		// Initialize internal stuff
		_i2c_current_transaction_n = 0;														// Number of data bytes transferred so far
		_i2c_current_transaction_state = 0;													// State of the transaction
		_i2c_transaction_idle=0;															// Non idle
		// Cache transaction to speedup access
		_i2c_current_transaction_dodata=_i2c_current_transaction->dodata;
		_i2c_current_transaction_address=_i2c_current_transaction->address;
		_i2c_current_transaction_rw=_i2c_current_transaction->rw;
		_i2c_current_transaction_dostop=_i2c_current_transaction->dostop;
		_i2c_current_transaction_callback=_i2c_current_transaction->callback;
		_i2c_current_transaction_status=&_i2c_current_transaction->status;
		_i2c_current_transaction_i2cerror=&_i2c_current_transaction->i2cerror;
		if(!_i2c_current_transaction->extdata)
			_i2c_current_transaction_data=_i2c_current_transaction->data;
		else
			_i2c_current_transaction_data=_i2c_current_transaction->extdata;
	}

	//fprintf(file_pri,"calling sm\n");
	I2C_statemachine_internal();

	//fprintf(file_pri,"aft int sm: i: %d\n",_i2c_transaction_idle);

	if(_i2c_transaction_idle==1)
		goto I2C_statemachine_outer_start;
}
void _I2C_statemachine_clearint()
{
	I2C1->CR1&=(0xFFFFFFFF & 0b00000001);		// Deactivate interrupts: clear bit 1-7
	I2C1->ICR|=0xffff;							// Clear any pending interrupts
}
/******************************************************************************
	I2C_statemachine_internal
*******************************************************************************
	Handle errors.
******************************************************************************/
void _I2C_statemachine_handleerror(unsigned char error, unsigned isr)
{
	// Error
	_i2c_transaction_idle=1;								// Return to idle
	*_i2c_current_transaction_status = error;				// Indicate failure in start condition
	*_i2c_current_transaction_i2cerror = isr;				// Indicate I2C failure code

	_I2C_statemachine_clearint();

	// Software reset of the I2C hardware - this may be suboptimal as error conditions could be recovered without reset.
	// This takes 3 APB clock cycles which is slow.
	/*I2C1->CR1&=0xFFFFFFFE;		// Clear PE
	while(I2C1->CR1&1);			// Loop until PE is clear - ensures a delay of 3 APB clock cycles according to datasheet
	I2C1->CR1|=0x1;				// Set PE*/

	// Callback if any
	if(_i2c_current_transaction_callback)
		_i2c_current_transaction_callback(_i2c_current_transaction);
	 // Remove next linked transactions
	_i2c_transaction_removelinked(_i2c_current_transaction);
}
/******************************************************************************
	I2C_statemachine_internal
*******************************************************************************
	Internal state machine executing the innards of one transaction including start, address, data, stop.
******************************************************************************/
void I2C_statemachine_internal()
{
	if(_i2c_current_transaction_rw==I2C_WRITE)
		I2C_statemachine_internal_tx();
	else
		I2C_statemachine_internal_rx();
}
void I2C_statemachine_internal_tx()
{

	// This is a working state machine for transfer
	unsigned int isr;


	isr = I2C1->ISR;		// This is always needed

#if I2CDBG_SM==1
	fprintf(file_pri,"SMtx s=%d ISR=%04X\n",(unsigned)_i2c_current_transaction_state,(unsigned)isr);
#endif

	if(_i2c_transaction_idle)
	{
		// If we are idle we should not be called - flag this as error
#ifdef I2CDBG_SM_ERR
		fprintf(file_pri,"I2C ISR TX Error idle\n");
#endif
		_I2C_statemachine_clearint();
		return;
	}

	// Process the state machine.
	switch(_i2c_current_transaction_state)
	{
		// ------------------------------------------------------
		// State 0:	Always called when executing the transaction.
		// 			Setup interrupt
		// 			Generate the start condition if needed. Otherwise, go to next state.
		// ------------------------------------------------------
		case 0:
		{
			_i2c_transaction_idle = 0;											// Mark as non-idle
			_i2c_current_transaction_state=1;									// Set next state


			I2C1->ICR|=0xffff;			// Clear any pending interrupts (should not be needed)
			I2C1->CR1|=0b11110111;		// Enable some interrupts: STOPIE RXIE NACKIE TCIE ERRIE

			// Reference manual sec. 39.4.8
			// Master communication initialization (address phase)
			/*I2C1->CR2 = ((_i2c_current_transaction_address&0x7f)<<1);		// Set slave address (bit 7:1)
			I2C1->CR2 |= ((_i2c_current_transaction_dodata&0b1111)<<16);	// Set number of bytes
			if(_i2c_current_transaction_dostop)
				I2C1->CR2 |= (1<<25);		// Auto-end=1 for stop condition after transfer
			else
				;							// No stop (restart condition): software end
			// Bit 10 is clear: I2C write
			// Other CR2 parameters will be: no PEC transfer, reload disabled (max 255 bytes transfer), addr is 7 bit
			I2C1->CR2 |= (1<<13);			// Start*/


			I2C1->CR2 = ((_i2c_current_transaction_address&0x7f)<<1)	|	// Set slave address (bit 7:1)
						((_i2c_current_transaction_dodata&0b1111)<<16)	|	// Set number of bytes
						((_i2c_current_transaction_dostop?1:0)<<25) 	|	// Autoend=1 for stop condition, otherwise autoend=0 for restart
						(0<<10) 										|	// I2C write (i.e. we don't change a bit here)
						(1<<13);											// I2C start


			// Return - ISR will be called back upon TX, NACK, etc.
			return;
		}

		// ------------------------------------------------------
		// State 1:	entered after the start condition.
		// The actions in state are driven by the hardware state machine.
		// ------------------------------------------------------
		case 1:

			if(isr & (1<<1))
			{
				// TXIS set
#if I2CDBG_SM==1
				fprintf(file_pri,"TXIS\n");
#endif

				// No checks on _i2c_current_transaction_n: the number of reads is stored in the I2C hardware
				I2C1->TXDR = _i2c_current_transaction_data[_i2c_current_transaction_n];
				_i2c_current_transaction_n++;

				// Don't return: there could be TXIC and STOPF/TC simultaneously

			}

			if(isr & (1<<4))
			{
				// NACKF set
#if I2CDBG_SM==1
				fprintf(file_pri,"NACKF\n");
#endif
				// This signals an error. Clear bit
				I2C1->ICR|=(1<<4);		// Clear NACKF

				// Some kind of failure (e.g. start fail, device not existent)
				_I2C_statemachine_handleerror(2,isr);		// Handles callbacks, removes transactions from queue.

				return;
			}
			// Here only called if end of transmission
			if(isr & (1<<6))
			{
				// TC set. Occurs when no "autoend". Signals end of transaction for those followed by RESTART.
#if I2CDBG_SM==1
				fprintf(file_pri,"TC\n");
#endif

				// End of transaction: no break to flow to next state.
			}
			if(isr & (1<<5))
			{
				// STOPF set. Occurs when "autoend". Signals end of transactions for those terminating by STOP.
#if I2CDBG_SM==1
				fprintf(file_pri,"STOPF\n");
#endif


				// In restart transactions a STOP condition occurs at the start of the reception and another at the end of the reception.
				// Undocumented behaviour.
				// False: this only happens if CR1 is not written in a single operation to 'restart'
				/*if(_i2c_current_transaction_n!=_i2c_current_transaction_dodata)
				{
					// STOPF due to restart - if it's not the last byte of transfer we ignore
					//fprintf(file_pri,"DISCARD STOPF with n %d\n",_i2c_current_transaction_n);
					// Clear the STOPF bit to prevent re-entering
					I2C1->ICR|=(1<<5);
					return;
				}
				else
				{*/
					// Legitimate STOPF
					// Clear the STOPF bit to prevent re-entering
					I2C1->ICR|=(1<<5);
					// End of transaction: no return to go to end of transaction
				//}

			}
			if((isr&0x8000)==0)
			{
#if I2CDBG_SM==1
				fprintf(file_pri,"BUSY#\n");
#endif
			}
			// Only if TC or STOPF we finish the transaction, or if busy is clear
			// Note: busy check not needed, only was needed when CR1 was written in multiple steps which led to spurious STOPF
			//if((isr & (1<<6)) || (isr & (1<<5)) || ((isr&0x8000)==0))
			if((isr & (1<<6)) || (isr & (1<<5)) )
			{
#if I2CDBG_SM==1
				fprintf(file_pri,"Trans end\n");
#endif
				// We've completed successfully the task
				_i2c_transaction_idle=1;															// Return to idle
				*_i2c_current_transaction_status = 0;												// Indicate success
				*_i2c_current_transaction_i2cerror = 0;												// Indicate success


				_I2C_statemachine_clearint();

				// Callback only if not linked to next
				if(!_i2c_current_transaction->link2next)
					if(_i2c_current_transaction_callback)
						_i2c_current_transaction_callback(_i2c_current_transaction);

				// Here must dealloc transacton
				i2c_transaction_pool_free1(_i2c_current_transaction);
				return;
			}
			return;


		default:
			#if I2CDBG_SM_ERR==1
			fprintf(file_pri,"I2C ISR unknown state %d\n",_i2c_current_transaction_state);
			#endif
	}

}

/*
	State machine to handle RX

*/
void I2C_statemachine_internal_rx()
{
	unsigned int isr;


	isr = I2C1->ISR;		// This is always needed

#if I2CDBG_SM==1
	fprintf(file_pri,"SMrx s=%d ISR=%04X\n",(unsigned)_i2c_current_transaction_state,(unsigned)isr);
#endif

	if(_i2c_transaction_idle)
	{
		// If we are idle we should not be called - flag this as error
#ifdef I2CDBG_SM_ERR
		fprintf(file_pri,"I2C ISR RX Error idle\n");
#endif
		_I2C_statemachine_clearint();
		return;
	}

	// Process the state machine.
	switch(_i2c_current_transaction_state)
	{
		// ------------------------------------------------------
		// State 0:	Always called when executing the transaction.
		// 			Setup interrupt
		// 			Generate the start condition if needed. Otherwise, go to next state.
		// ------------------------------------------------------
		case 0:
		{
			_i2c_transaction_idle = 0;											// Mark as non-idle
			_i2c_current_transaction_state=1;									// Set next state


			I2C1->ICR|=0xffff;			// Clear any pending interrupts (should not be needed)
			I2C1->CR1|=0b11110111;		// Enable some interrupts: STOPIE RXIE NACKIE TCIE ERRIE

			// Reference manual sec. 39.4.8
			// Master communication initialization (address phase)
			/*I2C1->CR2 = ((_i2c_current_transaction_address&0x7f)<<1);		// Set slave address (bit 7:1)
			I2C1->CR2 |= ((_i2c_current_transaction_dodata&0b1111)<<16);	// Set number of bytes
			if(_i2c_current_transaction_dostop)
				I2C1->CR2 |= (1<<25);		// Auto-end=1 for stop condition after transfer
			else
				;							// No stop (restart condition): software end
			I2C1->CR2 |= (1<<10);			// I2C read
			// Other CR2 parameters will be: no PEC transfer, reload disabled (max 255 bytes transfer), addr is 7 bit
			I2C1->CR2 |= (1<<13);			// Start*/

			// CR2 bits must be set all at onces, otherwise a spurious STOP condition is issued when a RESTART is desired

			I2C1->CR2 = ((_i2c_current_transaction_address&0x7f)<<1)	|	// Set slave address (bit 7:1)
						((_i2c_current_transaction_dodata&0b1111)<<16)	|	// Set number of bytes
						((_i2c_current_transaction_dostop?1:0)<<25) 	|	// Autoend=1 for stop condition, otherwise autoend=0 for restart
						(1<<10) 										|	// I2C read
						(1<<13);											// I2C start




			// Return - ISR will be called back upon RX, NACK, etc.
			return;
		}

		// ------------------------------------------------------
		// State 1:	entered after the start condition.
		// The actions in state are driven by the hardware state machine.
		// ------------------------------------------------------
		case 1:
			if(isr & (1<<2))
			{
				// RXNE set: read byte
#if I2CDBG_SM==1
				fprintf(file_pri,"RXNE\n");
#endif

				// No checks on _i2c_current_transaction_n: the number of reads is stored in the I2C hardware
				_i2c_current_transaction_data[_i2c_current_transaction_n] = I2C1->RXDR;
				_i2c_current_transaction_n++;

				// Don't return: there could be RXNE and TC simultaneously

			}
			if(isr & (1<<4))
			{
				// NACKF set
#if I2CDBG_SM==1
				fprintf(file_pri,"NACKF\n");
#endif
				// This signals an error. Clear bit
				I2C1->ICR|=(1<<4);		// Clear NACKF

				// Some kind of failure (e.g. start fail, device not existent)
				_I2C_statemachine_handleerror(1,isr);		// Handles callbacks, removes transactions from queue.

				return;
			}
			// Here only called if end of transmission
			if(isr & (1<<6))
			{
				// TC set. Occurs when no "autoend". Signals end of transaction for those followed by RESTART.
#if I2CDBG_SM==1
				fprintf(file_pri,"TC\n");
#endif

				// End of transaction: no break to flow to next state.
			}
			if(isr & (1<<5))
			{
				// STOPF set. Occurs when "autoend". Signals end of transactions for those terminating by STOP.
#if I2CDBG_SM==1
				fprintf(file_pri,"STOPF\n");
#endif


				// In restart transactions a STOP condition occurs at the start of the reception and another at the end of the reception.
				// Undocumented behaviour.
				// False: this only happens if CR1 is not written in a single operation to 'restart'
				/*if(_i2c_current_transaction_n!=_i2c_current_transaction_dodata)
				{
					// STOPF due to restart - if it's not the last byte of transfer we ignore
					fprintf(file_pri,"DISCARD STOPF with n %d\n",_i2c_current_transaction_n);
					// Clear the STOPF bit to prevent re-entering
					I2C1->ICR|=(1<<5);
					return;
				}
				else
				{*/
					// Legitimate STOPF
					// Clear the STOPF bit to prevent re-entering
					I2C1->ICR|=(1<<5);
					// End of transaction: no return to go to end of transaction
				//}

			}
			if((isr&0x8000)==0)
			{
#if I2CDBG_SM==1
				fprintf(file_pri,"BUSY#\n");
#endif
			}

			// Only if TC or STOPF we finish the transaction, or if busy is clear
			//if((isr & (1<<6)) || (isr & (1<<5)) || ((isr&0x8000)==0))
			if((isr & (1<<6)) || (isr & (1<<5)) )
			{
#if I2CDBG_SM==1
				fprintf(file_pri,"End trans\n");
#endif
				// We've completed successfully the task
				_i2c_transaction_idle=1;															// Return to idle
				*_i2c_current_transaction_status = 0;												// Indicate success
				*_i2c_current_transaction_i2cerror = 0;												// Indicate success


				_I2C_statemachine_clearint();

				// Callback only if not linked to next
				if(!_i2c_current_transaction->link2next)
					if(_i2c_current_transaction_callback)
						_i2c_current_transaction_callback(_i2c_current_transaction);

				// Here must dealloc transacton
				i2c_transaction_pool_free1(_i2c_current_transaction);
				return;
			}
			return;

		default:
			#if I2CDBG_SM_ERR==1
			fprintf(file_pri,"I2C ISR unknown state %d\n",_i2c_current_transaction_state);
			#endif

	}

}


//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------




