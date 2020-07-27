/*
 * i2c_transaction.c
 *
 *  Created on: 3 oct. 2019
 *      Author: droggen
 */

#include "i2c_transaction.h"
#include "stmi2c-isr.h"
#include "atomicop.h"
#include <stdarg.h>
#include <string.h>

// Internal stuff
I2C_TRANSACTION *i2c_transaction_buffer[I2C_TRANSACTION_MAX];										// Not volatile, only modified in usermode
unsigned char _i2c_transaction_buffer_wr, _i2c_transaction_buffer_rd;						// rd modified by int, wr modified by int if queue called from int, but user function using these deactivate interrupts

// Transaction pool
I2C_TRANSACTION _i2c_transaction_pool[I2C_TRANSACTION_MAX];
unsigned char _i2c_transaction_pool_alloc[I2C_TRANSACTION_MAX];
unsigned char _i2c_transaction_pool_wr, _i2c_transaction_pool_rd;
unsigned char _i2c_transaction_pool_reserved;


/******************************************************************************
*******************************************************************************
INTERRUPT-DRIVEN ACCESS   INTERRUPT-DRIVEN ACCESS   INTERRUPT-DRIVEN ACCESS
*******************************************************************************
******************************************************************************/

/******************************************************************************
	i2c_transaction_setup
*******************************************************************************
	Initialise a transaction structure with the specified parameters and sane defaults.
******************************************************************************/
void i2c_transaction_setup(I2C_TRANSACTION *t,unsigned char addr7,unsigned char mode,unsigned char stop,unsigned char n)
{
	t->address=addr7;
	t->rw=mode;
	t->extdata=0;
//	t->dostart=1;
	t->dostop=stop;
//	t->doaddress=1;
	t->dodata=n;
	t->callback=0;
	t->user=0;
	t->link2next=0;
}

/******************************************************************************
	i2c_transaction_setup_dbg
*******************************************************************************
	Initialise a transaction structure with the specified parameters and sane defaults.
******************************************************************************/
//void i2c_transaction_setup(I2C_TRANSACTION *t,unsigned char addr7,unsigned char mode,unsigned char stop,unsigned char n)
// mode: 0=write, 1=read.
#define I2CDATASIZE 32
unsigned char i2c_data[I2CDATASIZE];
void i2c_transaction_setup_dbg(unsigned char addr7,unsigned char mode,unsigned char stop,unsigned char n)
{
	// Initialize internal stuff
	_i2c_current_transaction_n = 0;													// Number of data bytes transferred so far
	_i2c_current_transaction_state = 0;											// State of the transaction
#warning "will have to be changed when multiple transactions implemented"
	_i2c_transaction_idle=1;																// idle

	// User params
	_i2c_current_transaction_address=addr7;
	_i2c_current_transaction_rw=mode;			// write: 0. read: 1.
	_i2c_current_transaction_data=i2c_data;
	memset(i2c_data,0x55,I2CDATASIZE);
//	t->dostart=1;			// Not used anymore
	_i2c_current_transaction_dostop=stop;
//	t->doaddress=1;			// Not used anymore
	_i2c_current_transaction_dodata=n;			// Number of data
	_i2c_current_transaction_callback=0;
	_i2c_current_transaction_status=&status;
	_i2c_current_transaction_i2cerror=&i2cerror;
//	t->callback=0;			// no callback
	//t->user=0;				// no user var
	//t->link2next=0;			// no link to next
}


/******************************************************************************
	i2c_transaction_execute
*******************************************************************************
	Execute the transaction.
	Two execution modes are possible: wait for completion/error and return, or call the callback upon completion/error
	In both case interrupts are used to execute the transaction

	Return value:
		0:			success
		1:			error
******************************************************************************/
#if BOOTLOADER==0
unsigned char i2c_transaction_execute(I2C_TRANSACTION *trans,unsigned char blocking)
{
	return i2c_transaction_queue(1,blocking,trans);
}
#endif

/******************************************************************************
	i2c_transaction_queue
*******************************************************************************
	Enqueues the specified transactions for later execution. Execution is
	scheduled immediately if no other transactions are in the queue, otherwise it
	happens as soon as the queue is empty.

	n:				Number of transactions to enqueue
	blocking:	Whether to block until all enqueued transactions are completed
	...				variable number of pointers to I2C_TRANSACTION structures

		Return value:
		0:			success
		1:			error
******************************************************************************/
/*
TODO
i2c_transaction_queue blocking is not working if interrupt routine pushes transactions, as idle may not be reached
(usually it will be reached, but it may wait longer than needed)
idea: "done" filed in i2c transaction, set when transaction completed.
transactio setup must clear this
wait until done by checking this field
*/
unsigned char i2c_transaction_queue(unsigned char n,unsigned char blocking,...)
{
	va_list args;
//	I2C_TRANSACTION *tlast;

	// No transaction to queue: success.
	if(n==0)
		return 0;


	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check enough space to store n transactions
		if(_i2c_transaction_getfree()<n)
		{
			//printf("i2c No free\n");
			return 1;
		}

		va_start(args,blocking);

		for(unsigned char i=0;i<n;i++)
		{
			I2C_TRANSACTION *t = va_arg(args,I2C_TRANSACTION *);

			// Initialise link2next: links to next if not the last transaction to push
			if(i!=n-1)
				t->link2next=1;
			else
				t->link2next=0;

//			tlast=t;
			//printf("Transaction %d: %p\n",i,t);
			_i2c_transaction_put(t);
		}
		//printf("State of transactions\n");
		//i2c_transaction_printall(file_usb);
		/*I2C_TRANSACTION *t = _i2c_transaction_findnext();
		printf("FindNext: %p\n",t);
		printf("Current: %p\n",_i2c_current_transaction);
		t=_i2c_transaction_findnext();
		printf("FindNext2: %p\n",t);
		t=_i2c_transaction_findnext();
		printf("FindNext3: %p\n",t);
		t=_i2c_transaction_getnext();
		printf("GetNext: %p\n",t);
		t=_i2c_transaction_getnext();
		printf("GetNext2: %p\n",t);
		t=_i2c_transaction_getnext();
		printf("GetNext3: %p\n",t);*/

		// If idle, call IV to execute next; if not idle, the IV will automatically proceed to next transaction
		if(_i2c_transaction_idle)
			I2C_statemachine_outer();
	}
	//printf("I2C wait\n");

	if(blocking)
		while(!_i2c_transaction_idle);

	return 0;

}


/*void i2c_transaction_cancel(void)
{
	cli();
	TWCR&=~(1<<TWIE);											// Deactivate interrupt
	sei();
	// Resets the hardware - to take care of some challenging error situations
	TWCR=0;														// Deactivate i2c
	TWCR=(1<<TWEN);											// Activate i2c
	if(!_i2c_transaction_idle)								// Transaction wasn't idle -> call the callback in cancel mode
	{
		TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);		// Deactivate interrupt, generate a stop condition.
		*_i2c_current_transaction_status=255;			// Mark transaction canceled
		if(_i2c_current_transaction_callback)			// Call callback if available
			_i2c_current_transaction_callback(_i2c_current_transaction);

	}
	_i2c_transaction_idle = 1;

}
*/








/******************************************************************************
	_i2c_transaction_init
*******************************************************************************
	Initialise the data structures for i2c transactions.
	Internally called by i2c_init
******************************************************************************/
void i2c_transaction_init(void)
{
	_i2c_transaction_buffer_wr=0;
	_i2c_transaction_buffer_rd=0;
	_i2c_current_transaction=0;
	_i2c_transaction_pool_wr=0;
	_i2c_transaction_pool_rd=0;
	_i2c_transaction_pool_reserved=0;
	for(unsigned char i=0;i<I2C_TRANSACTION_MAX;i++)
		_i2c_transaction_pool_alloc[i]=0;
}


/******************************************************************************
	i2c_transaction_getqueued
*******************************************************************************
	Returns the number of transaction queued
******************************************************************************/
unsigned char i2c_transaction_getqueued(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return (_i2c_transaction_buffer_wr-_i2c_transaction_buffer_rd)&I2C_TRANSACTION_MASK;
	}
	return 0;	// To avoid compiler complaints
}



/******************************************************************************
	i2c_transaction_removenext
*******************************************************************************
	Removes the next transaction, if any.
******************************************************************************/
#if BOOTLOADER==0
void i2c_transaction_removenext(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(_i2c_transaction_buffer_rd!=_i2c_transaction_buffer_wr)
			_i2c_transaction_buffer_rd=(_i2c_transaction_buffer_rd+1)&I2C_TRANSACTION_MASK;
	}
}
#endif
/******************************************************************************
	_i2c_transaction_removelinked
*******************************************************************************
	Internal use, ensure called atomically.
	trans: transaction from which the next linked ones must be removed.

	This function removes the transaction from the transaction queue.
	If a transaction has been allocated from the transaction pool (including trans)
	it is freed.

	Removes all the subsequent linked transactions, if any.
******************************************************************************/
void _i2c_transaction_removelinked(I2C_TRANSACTION *trans)
{
	// Get the current transaction
	I2C_TRANSACTION *cur=trans;

	// Free if from pool
	i2c_transaction_pool_free1(cur);

	while(cur->link2next)
	{
		cur = _i2c_transaction_getnext();
		if(!cur)
			break;

		// Free if from pool
		i2c_transaction_pool_free1(cur);
	}
}



/******************************************************************************
	_i2c_transaction_getfree
*******************************************************************************
	Returns the number of available transaction slots
******************************************************************************/
unsigned char _i2c_transaction_getfree(void)
{
	return I2C_TRANSACTION_MAX-i2c_transaction_getqueued()-1;
}
/******************************************************************************
	_i2c_transaction_put
*******************************************************************************
	Internal use. Ensure atomic use.
******************************************************************************/
void _i2c_transaction_put(I2C_TRANSACTION *trans)
{
	i2c_transaction_buffer[_i2c_transaction_buffer_wr]=trans;
	_i2c_transaction_buffer_wr=(_i2c_transaction_buffer_wr+1)&I2C_TRANSACTION_MASK;
}
/******************************************************************************
	_i2c_transaction_getnext
*******************************************************************************
	Internal use. Ensure atomic use.

	Returns the next active transaction or zero; the transaction pointer is updated.

	Return value:
		0:				If no next transaction
		nonzero:	pointer to next transaction
******************************************************************************/
I2C_TRANSACTION *_i2c_transaction_getnext(void)
{

	if(_i2c_transaction_buffer_rd!=_i2c_transaction_buffer_wr)
	{

		I2C_TRANSACTION *t=i2c_transaction_buffer[_i2c_transaction_buffer_rd];
		_i2c_transaction_buffer_rd=(_i2c_transaction_buffer_rd+1)&I2C_TRANSACTION_MASK;
		return t;
	}
	return 0;
}

#if BOOTLOADER==0
void i2c_transaction_print(I2C_TRANSACTION *trans,FILE *file)
{
	fprintf(file,"I2C transaction %p\n",trans);
	fprintf(file,"Address %02X rw %d\n",trans->address,trans->rw);
	for(unsigned char i=0;i<16;i++)
		fprintf(file,"%02X ",trans->data[i]);
	fprintf(file,"\n");
//	for(unsigned char i=0;i<16;i++)
//		fprintf(file,PSTR("%02X "),trans->dataack[i]);
	fprintf(file,"\n");
	fprintf(file,"CB: %p User: %p\n",trans->callback,trans->user);
	fprintf(file,"Status: %02X Error: %02X\n",trans->status,trans->i2cerror);

}

void i2c_transaction_printall(FILE *file)
{
	fprintf(file,"I2C transaction buffer. rd: %02d wr: %02d\n",_i2c_transaction_buffer_rd,_i2c_transaction_buffer_wr);
	for(unsigned char i=0;i<I2C_TRANSACTION_MAX;i++)
		fprintf(file,"%p ",i2c_transaction_buffer[i]);
	fprintf(file,"\n");
}
#endif


/******************************************************************************
*******************************************************************************
TRANSACTION POOL
*******************************************************************************
******************************************************************************/
/******************************************************************************
	i2c_transaction_pool_reserve
*******************************************************************************
	Reserves (allocates) n transactions from the global transaction pool.
	Transactions must be freed with i2c_transaction_pool_free or i2c_transaction_pool_free1.

	Parameters ... must be of type I2C_TRANSACTION **, i.e. pointer to a variable
	that will hold the pointer to the transaction.


	Return value:
		0:				Success
		nonzero:	Error
******************************************************************************/
#if BOOTLOADER==0
unsigned char i2c_transaction_pool_reserve(unsigned char n,...)
{
	va_list args;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(I2C_TRANSACTION_MAX-_i2c_transaction_pool_reserved<n)
			return 1;

		va_start(args,n);
		for(unsigned char i=0;i<n;i++)
		{
			unsigned char j;
			// Search for first free
			for(j=0;j<I2C_TRANSACTION_MAX;j++)
			{
				//printf("%d\n",_i2c_transaction_pool_alloc[j]);
				if(!_i2c_transaction_pool_alloc[j])
					break;
			}

			// Free transaction at j
			//printf("Trans %d free at %p\n",j,&_i2c_transaction_pool[j]);
			_i2c_transaction_pool_alloc[j]=1;

			I2C_TRANSACTION **t = va_arg(args,I2C_TRANSACTION **);

			*t = &_i2c_transaction_pool[j];
		}
		_i2c_transaction_pool_reserved+=n;
	}
	return 0;
}
#endif
/******************************************************************************
	i2c_transaction_pool_free
*******************************************************************************
	Frees (deallocates) n transactions from the global transaction pool.
	Transactions should be allocated with i2c_transaction_pool_reserve.
	However, if a transaction that has not been allocated with
	i2c_transaction_pool_reserve is passed that transaction is ignored.
	This makes it safe to call this function regardless of how the transaction
	has been allocated (e.g. global, stack, or allocated from the pool).


	Parameters ... must be of type I2C_TRANSACTION *, i.e. pointers to
	the transaction.

	Return value:
		0:				Success
		nonzero:	Error
******************************************************************************/
void i2c_transaction_pool_free(unsigned char n,...)
{
	#if BOOTLOADER==0
	va_list args;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		va_start(args,n);
		for(unsigned char i=0;i<n;i++)
		{
			// Get the transaction
			I2C_TRANSACTION *t = va_arg(args,I2C_TRANSACTION *);

			// Find the offset of the transaction in the pool
			unsigned short off = (unsigned short)(t-_i2c_transaction_pool);

			// Skip if it is not a transaction allocated from the pool
			if(off>=I2C_TRANSACTION_MAX)
				continue;

			//printf("remove at off: %d\n",off);
			_i2c_transaction_pool_alloc[off] = 0;
			_i2c_transaction_pool_reserved--;
		}
	}
	#endif
}

/******************************************************************************
	i2c_transaction_pool_free1
*******************************************************************************
	Frees (deallocates) 1 transactions from the global transaction pool.
	The transaction should be allocated with i2c_transaction_pool_reserve.
	However, if a transaction that has not been allocated with
	i2c_transaction_pool_reserve is passed that transaction is ignored.
	This makes it safe to call this function regardless of how the transaction
	has been allocated (e.g. global, stack, or allocated from the pool).



	Return value:
		0:				Success
		nonzero:	Error
******************************************************************************/
void i2c_transaction_pool_free1(I2C_TRANSACTION *t)
{
	#if BOOTLOADER==0

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Find the offset of the transaction in the pool
		unsigned short off = (unsigned short)(t-_i2c_transaction_pool);

		// Skip if it is not a transaction allocated from the pool
		if(off>=I2C_TRANSACTION_MAX)
			continue;

		//printf("remove at off: %d\n",off);
		_i2c_transaction_pool_alloc[off] = 0;
		_i2c_transaction_pool_reserved--;
	}
	#endif
}
