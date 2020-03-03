/*
   MEGALOL - ATmega LOw level Library
   Circular Buffer Module
*/
/*
Copyright (C) 2009-2016:
         Daniel Roggen, droggen@gmail.com
		 
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
/*
	file: circbuf
	
	Implements a circular buffer, typically used for buffering in communication pipes. The buffer size must be a power of 2.
	
	The key data structure is CIRCULARBUFFER which holds a pointer to a buffer, read and write pointers, and additional management information.
	
	The CIRCULARBUFFER structure members must be initialised as follows:
			CIRCULARBUFFER.buffer: point to a buffer to hold the data of size power of 2.
			CIRCULARBUFFER.size: indicate the size; it must be a power of 2.
			CIRCULARBUFFER.mask: bitmask to efficiently implement wraparound operations; it must be CIRCULARBUFFER.size-1, with CIRCULARBUFFER.size a power of2.

			The function buffer_init helps setting up these parameters.
		
	*Usage in interrupts*
	
	Safe to use in interrupts.
	
	*Notes*
	
	Must analyse the code and possibly introduce ATOMIC_BLOCK(ATOMIC_RESTORESTATE) for all the functions which use both the rdptr and wrptr (e.g. buffer_isfull, buffer_clear, buffer_isempty, buffer_level, etc).
	
	
	
	
*/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "circbuf.h"
#include "atomicop.h"

/******************************************************************************
*******************************************************************************
*******************************************************************************
BUFFER MANAGEMENT   BUFFER MANAGEMENT   BUFFER MANAGEMENT   BUFFER MANAGEMENT  
*******************************************************************************
*******************************************************************************
******************************************************************************/

/******************************************************************************
   function: buffer_init
*******************************************************************************
	Helper function to initialises the circular buffer.


	Parameters:
		io	-		CIRCULARBUFFER buffer
		sz	-		Size of the circular buffer - must be a power of 2
******************************************************************************/
void buffer_init(CIRCULARBUFFER *io, unsigned char *buffer, unsigned int sz)
{
	// Set buffer
	io->buffer = buffer;
	// Clear buffer
	io->wrptr=io->rdptr=0;
	// Set size and mask
	io->size=sz;
	io->mask=sz-1;


}

/******************************************************************************
   function: buffer_put
*******************************************************************************
	Adds a character to the circular buffer. 
	
	This must only be called if the buffer is not full, as no check is done on the buffer capacity.
	
	Parameters:
		io	-		CIRCULARBUFFER buffer
		c	-		Character to add to the buffer
******************************************************************************/
void buffer_put(volatile CIRCULARBUFFER *io, unsigned char c)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
	io->buffer[io->wrptr]=c;
	io->wrptr=(io->wrptr+1)&(io->mask);
	}
}
/******************************************************************************
   function: buffer_get
*******************************************************************************
	Returns a character from the circular buffer. 
	
	This must only be called if the buffer is not empty, as no check is done on the buffer capacity.
	
	Returns:
		Character from the buffer
******************************************************************************/
unsigned char buffer_get(volatile CIRCULARBUFFER *io)
{
	unsigned char c;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
	c = io->buffer[io->rdptr];
	io->rdptr=(io->rdptr+1)&(io->mask);	
	}
	return c;
}

/******************************************************************************
   function: buffer_unget
*******************************************************************************
	Ungets a character into the circular buffer.
	
	This function keeps byte ordering by ungetting the byte at 'rdptr-1'.
	
	This must only be called if the buffer is not full, as no check is done on the buffer capacity.
	
	Parameters:
		io	-		CIRCULARBUFFER buffer
		c	-		Character to unget to the buffer
		
	Returns:
		c
******************************************************************************/
unsigned char buffer_unget(volatile CIRCULARBUFFER *io,unsigned char c)
{
	io->rdptr=(io->rdptr-1)&(io->mask);
	io->buffer[io->rdptr]=c;	
	return c;
}


/******************************************************************************
   function: buffer_isempty
*******************************************************************************
	Checks if the circular buffer is empty.
	
	Parameters:
		io	-		CIRCULARBUFFER buffer
		
	Returns:
		0	-		Not empty
		1	-		Empty
******************************************************************************/
unsigned char buffer_isempty(volatile CIRCULARBUFFER *io)
{
	unsigned char v;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
	if(io->rdptr==io->wrptr)
		v=1;
	else
		v=0;
	}
	return v;
}

/******************************************************************************
   function: buffer_isempty
*******************************************************************************
	Clears the content of the buffer
	
	Parameters:
		io	-		CIRCULARBUFFER buffer
******************************************************************************/
void buffer_clear(volatile CIRCULARBUFFER *io)
{
	io->wrptr=io->rdptr=0;
}

/******************************************************************************
   function: buffer_isfull
*******************************************************************************
	Checks if the circular buffer is full.
	
	Parameters:
		io	-		CIRCULARBUFFER buffer
		
	Returns:
		0	-		Not full
		1	-		Full
******************************************************************************/
unsigned char buffer_isfull(volatile CIRCULARBUFFER *io)
{
	unsigned char v;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
	// We loose 1 character in the buffer because rd=wr means empty buffer, and 
	// wr+1=rd means buffer full (whereas it would actually mean that one more byte can be stored).
	if( ((io->wrptr+1)&io->mask) == io->rdptr )
		v=1;
	else
		v=0;
	}
	return v;
}
/******************************************************************************
   function: buffer_level
*******************************************************************************
	Indicates how many bytes are in the buffer
	
	Parameters:
		io	-		CIRCULARBUFFER buffer
		
	Returns:
		Number of bytes in the buffer
******************************************************************************/
unsigned short buffer_level(volatile CIRCULARBUFFER *io)
{
	unsigned short l;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
	l = ((io->wrptr-io->rdptr)&io->mask);
	}
	return l;
}
/******************************************************************************
   function: buffer_freespace
*******************************************************************************
	Indicates how much free space is in the buffer.
	
	Parameters:
		io	-		CIRCULARBUFFER buffer
		
	Returns:
		Number of free bytes in the buffer
******************************************************************************/
unsigned short buffer_freespace(volatile CIRCULARBUFFER *io)
{
	return io->size-buffer_level(io)-1;
}
