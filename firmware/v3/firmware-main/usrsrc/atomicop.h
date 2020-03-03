/*
 * atomic.h
 *
 *  Created on: 23 sept. 2019
 *      Author: droggen
 */

#ifndef ATOMICOP_H_
#define ATOMICOP_H_


#include "main.h"
#include "cmsis_gcc.h"
#include "usrmain.h"

static inline uint32_t __disable_irq_retval()
{
	__disable_irq();
	return 1;
}

static __inline__ void __irq_restore(const  uint32_t *__s)
{
	//itmprintf("irq restore s: %p %d\n",__s,*__s);
	if((*__s)&1)
		__disable_irq();
	else
		__enable_irq();
}


//
// AVR compatible ATOMIC_BLOC(ATOMIC_RESTORESTATE) { ... }
//


#define ATOMIC_RESTORESTATE uint32_t __primsk_save __attribute__((__cleanup__(__irq_restore)))= __get_PRIMASK()

#define ATOMIC_BLOCK(type) for ( type, __ToDo = __disable_irq_retval(); \
	                       __ToDo ; __ToDo = 0 )


#endif /* ATOMICOP_H_ */
