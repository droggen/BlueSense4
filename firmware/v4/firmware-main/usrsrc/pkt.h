/*
   pkt - Packet helper functions
   Copyright (C) 2008,2015:
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
   This file provides helper functions to create packets for streaming data transfer.
   In particular it provides functions to append data into a packet. Data can be of
   various sizes, including custom-sized bitfields.
   Checksumming is also supported.

   Usage:
      1. Declare a packet once (e.g. as a global variable)
      2. Before filling the packet, call packet_init
      3. Fill the packet with the various packet_add??? functions
	  	4a.If not adding a checksum, call packet_end to complete the bit aligment in the last data byte
      4b.Otherwise append a checksum with packet_checksum_*** functions which will complete the bit alignment and add teh checksum
      5. Stream the packet. The size of the packet in bytes is provided by packet_size.

*/

#ifndef PKT_H
#define PKT_H

#define __PKT_NEWLITTLE

//#define __PKT_DATA_MAXSIZE 64
#define __PKT_DATA_MAXSIZE 1024

typedef struct
{
   unsigned char data[__PKT_DATA_MAXSIZE];
   unsigned char bitptr;							// Between 0..7, indicate which bit in the current byte
   unsigned char *dptr;								// Pointer to current byte
   unsigned char hdrsize;

} PACKET;

void packet_init(PACKET *packet,const char *hdr,unsigned char hdrsize);
void packet_init_old(PACKET *packet);
void packet_reset(PACKET *packet);
void packet_end(PACKET *packet);
//void packet_addbits(PACKET *packet,unsigned long data,unsigned char nbits);
void packet_addbits_little(PACKET *packet,unsigned long data,unsigned char nbits);
void packet_addbits_little_old(PACKET *packet,unsigned long data,unsigned char nbits);
void packet_add8(PACKET *packet,unsigned char data);
//void packet_add16(PACKET *packet,unsigned short data);
void packet_add16_little(PACKET *packet,unsigned short data);
//void packet_add32(PACKET *packet,unsigned long data);
void packet_add32_little(PACKET *packet,unsigned long data);
void packet_addchecksum_8(PACKET *packet);
//void packet_addchecksum_fletcher16(PACKET *packet);
void packet_addchecksum_fletcher16_little(PACKET *packet);
unsigned short packet_size(PACKET *packet);
unsigned short packet_CheckSum(unsigned char *ptr,unsigned n);
unsigned short packet_fletcher16(unsigned char *data, int len );


#endif // PKT_H
