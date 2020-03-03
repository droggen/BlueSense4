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


#ifndef __CIRCBUF_H
#define __CIRCBUF_H

/******************************************************************************
*******************************************************************************
BUFFER MANAGEMENT   BUFFER MANAGEMENT   BUFFER MANAGEMENT   BUFFER MANAGEMENT  
*******************************************************************************
******************************************************************************/

typedef struct 
{
	unsigned char volatile *buffer;
	unsigned int volatile wrptr,rdptr;
	unsigned int size,mask;
} CIRCULARBUFFER;

void buffer_init(CIRCULARBUFFER *io, unsigned char *buffer, unsigned int sz);
void buffer_put(volatile CIRCULARBUFFER *io, unsigned char c);
unsigned char buffer_get(volatile CIRCULARBUFFER *io);
unsigned char buffer_unget(volatile CIRCULARBUFFER *io,unsigned char c);
unsigned char buffer_isempty(volatile CIRCULARBUFFER *io);
void buffer_clear(volatile CIRCULARBUFFER *io);
unsigned char buffer_isfull(volatile CIRCULARBUFFER *io);
unsigned short buffer_level(volatile CIRCULARBUFFER *io);
unsigned short buffer_freespace(volatile CIRCULARBUFFER *io);

#endif
