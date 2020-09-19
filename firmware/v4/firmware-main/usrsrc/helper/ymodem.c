/*
 * ymodem.c
 *
 *  Created on: 19 sept. 2020
 *      Author: droggen
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ymodem.h"
#include "global.h"
#include "serial.h"
#include "ufat.h"
#include "fatfs.h"
#include "wait.h"


#define YMODEM_DEBUG_PROTOCOL	0
#define YMODEM_DEBUG_OTHER		0

/******************************************************************************
	file: ymodem
*******************************************************************************
	Key functions:

		* unsigned char ymodem_send(const char *filename,unsigned char *data,unsigned len,FILE *fintf);
		* unsigned char ymodem_send_multi(unsigned nfiles,const char *filename[],unsigned char *data[],unsigned len[],FILE *fintf);
		* unsigned char ymodem_send_multi_file(unsigned nfiles,const char *filename[],FILE *fintf);





	XMODEM packet: 132 bytes total; 128 bytes data
		SOH	blk# ~blk# data CKSUM
	After sending block, wait for answer:
		ACK: send next block
		NAK: repeat block
		CAN: abort transmission
		timeout: resend packet



******************************************************************************/

/******************************************************************************
	function: ymodem_crc16
*******************************************************************************
	Computes the YMODEM CRC16.

	Parameters:
		buf 	-		Pointer to the buffer, typically of length 128 or 1024
		size	-		Size of the buffer

	Returns:
//		crc		-		16-bit CRC

******************************************************************************/
unsigned short ymodem_crc16(const unsigned char *buf, unsigned size)
{
	// Computation on 32-bit faster on ARM.
	//unsigned short crc = 0;
	unsigned crc = 0;
	int i;

	while(size--) {
		crc = crc ^ *buf++ << 8;

		for (i=0; i<8; i++) {
			if (crc & 0x8000) {
				crc = crc << 1 ^ 0x1021;
			} else {
				crc = crc << 1;
			}
		}
	}
	//return crc;
	return (unsigned short)crc;
}

/******************************************************************************
	function: ymodem_send_packet
*******************************************************************************
	Sends a YMODEM packet and get response, potentially re-sending if needed

	This function assumes that packet 0 (containing the file name and size) is
	128 bytes, and subsequent packets are 1024 bytes.

	The packet buffer

	Parameters:
		fintf 		-		Interface over which the packet is sent
		packet		-		Pointer to the packet, which must be 128 bytes bytes long for the packet 0, or 1024 bytes long for subsequent packets.
		blobk_no	-		Packet number


	Returns:
		0			-		Success
		Nonzero		-		Error

******************************************************************************/
unsigned char ymodem_send_packet(FILE *fintf,unsigned char *packet, int block_no)
{
	int ch;
	//unsigned t1,t2,t3;

	// XMODEM: After DATA packet, wait for reply:
	// ACK: ok, next packet
	// NAK: repeat packet
	// timeout: resend (once)
	// CAN: abort transfer

	for(int i=0;i<2;i++)
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Sending packet %d (attempt %d)\n",block_no,i);
#endif

		//t1 = timer_us_get();

		// Send packet
		_ymodem_send_packet(fintf,packet,block_no);

		//t2 = timer_us_get();

		// Get response
		ch = fgetc_timeout(fintf,YMODEM_PACKET_TIMEOUT);

		//t3 = timer_us_get();

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Response to packet %d: %d\n",block_no,ch);
#endif

		if(ch==-1)
			continue;		// Timeout: try again once
		if(ch==YMODEM_ACK)
		{
			//fprintf(file_dbg,"Packet total time: %u us. Send: %u. Response: %u\n",t3-t1,t2-t1,t3-t2);
			return 0;		// Success
		}
		if(ch==YMODEM_NAK)
		{
			i--;
			continue;		// Repeat packet, as long as needed (i--)
		}
		// All other cases: error
		return 1;
	}



	// Two timeouts: error
	return 2;
}

/******************************************************************************
	function: _ymodem_send_packet
*******************************************************************************
	Low-level send of a YMODEM packet.

	This function assumes that packet 0 (containing the file name and size) is
	128 bytes, and subsequent packets are 1024 bytes.

	The packet buffer

	Parameters:
		fintf 		-		Interface over which the packet is sent
		packet		-		Pointer to the packet, which must be 128 bytes bytes long for the packet 0, or 1024 bytes long for subsequent packets.
		blobk_no	-		Packet number


	Returns:
		0			-		Success
		Nonzero		-		Error

******************************************************************************/
void _ymodem_send_packet(FILE *fintf,unsigned char *packet, int block_no)
{
	unsigned short crc;
	unsigned packet_size;
	unsigned char buf[4];

	// Short packet for block 0 - all others are 1K
	if(block_no == 0)
		packet_size = YMODEM_PACKET_SIZE;
	else
		packet_size = YMODEM_PACKET_1K_SIZE;

#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Packet size is: %d\n",packet_size);
#endif

	//unsigned t1 = timer_us_get();
	crc = ymodem_crc16(packet, packet_size);
	//unsigned t2 = timer_us_get();

#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Checksum is: %u\n",crc);
#endif

	// Optimise the writes
	buf[0] = (block_no==0)?YMODEM_SOH:YMODEM_STX;					// Select packet start: SOH for 128 bytes (packet 0) or STX for 1024 bytes (subsequent packets)
	buf[1] = block_no & 0xFF;										// Send block number: block followed by complement of block.
	buf[2] = (~block_no) & 0xFF;

	fwrite(buf,3,1,fintf);

	//unsigned t3 = timer_us_get();

	/*fputc((block_no==0)?YMODEM_SOH:YMODEM_STX,fintf);
	fputc(block_no & 0xFF,fintf);
	fputc(~block_no & 0xFF,fintf);*/

	// Send the data
	/*for(unsigned i=0;i<packet_size;i++)
		fputc(packet[i],fintf);
		*/
	fwrite(packet,packet_size,1,fintf);

	//unsigned t4 = timer_us_get();

	// Send the CRC
	//fputc((crc >> 8) & 0xFF,fintf);
	//fputc(crc & 0xFF,fintf);

	buf[0] = (crc >> 8) & 0xFF;
	buf[1] = crc & 0xFF;
	fwrite(buf,2,1,fintf);

	//unsigned t5 = timer_us_get();

	// Flush the device, as otherwise the data stays in our buffers
	fflush(fintf);

	//unsigned t6 = timer_us_get();

	//fprintf(file_dbg,"->CRC %u -> open %u -> data %u -> close %u -> flush %u\n",t2-t1,t3-t1,t4-t1,t5-t1,t6-t1);


}
unsigned char ymodem_notify_eot(FILE *fintf)
{
	// All the data is transferred
#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Notify EOT and wait confirmation\n");
#endif
	int ch;
	do
	{
		fputc(YMODEM_EOT,fintf);
		fflush(fintf);	// Flush otherwise data stays local
		ch = fgetc_timeout(fintf,YMODEM_PACKET_TIMEOUT);
	}
	while((ch != YMODEM_ACK) && (ch != -1));
	if(ch==YMODEM_ACK)
		return 0;

#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Notify EOT: error\n");
#endif

	return 1;
}
// Return 0: ok. nonzero: error
unsigned char ymodem_send_data_packets(FILE *fintf,unsigned char* data, unsigned size)
{
	int blockno = 1;
	unsigned send_size;

	while (size > 0)
	{
		// Amount to send is minimum between remaining data or 1K size
		if (size > YMODEM_PACKET_1K_SIZE)
			send_size = YMODEM_PACKET_1K_SIZE;
		else
			send_size = size;

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Blockno: %d. Sending block of size: %u\n",blockno,send_size);
#endif

		// Send the packet - handles repetition, etc.
		if(ymodem_send_packet(fintf,data, blockno))
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Blockno: %d. error\n",blockno);
#endif
			return 1;									// Error in sending the packet
		}

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Blockno %d: OK\n",blockno);
#endif
		blockno++;
		data += send_size;
		size -= send_size;

	}
	return 0;
}
// Return 0: ok. nonzero: error
// Send the data to fintf reading from a file
unsigned char ymodem_send_data_packets_file(FILE *fintf,unsigned size,FIL *fil)
{
	int blockno = 1;
	unsigned send_size;
	// Buffer
	unsigned char data[YMODEM_PACKET_1K_SIZE];
	UINT br;


	while (size > 0)
	{
		// Amount to send is minimum between remaining data or 1K size
		if (size > YMODEM_PACKET_1K_SIZE)
			send_size = YMODEM_PACKET_1K_SIZE;
		else
			send_size = size;

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Blockno: %d. Sending block of size: %u\n",blockno,send_size);
#endif


		// Read the data from the file into the buffer
		//unsigned t1 = timer_us_get();
		FRESULT res = f_read(fil, data, send_size, &br);
		//unsigned t2 = timer_us_get();
		//fprintf(file_dbg,"Read time: %u us %u B/S\n",t2-t1,send_size*1000000/(t2-t1));
		if(res)
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Read error: abort\n");
#endif

			return 1;
		}

		// Send the packet - handles repetition, etc.
		if(ymodem_send_packet(fintf,data, blockno))
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Blockno: %d. error\n",blockno);
#endif
			return 1;									// Error in sending the packet
		}

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Blockno %d: OK\n",blockno);
#endif

		blockno++;
		size -= send_size;

	}
	return 0;
}


unsigned char ymodem_send_packet0(FILE *fintf,const char *filename,unsigned len)
{
	unsigned char block[YMODEM_PACKET_SIZE];
	unsigned idx=0;

	memset(block,0,YMODEM_PACKET_SIZE);					// Only entire packets can be sent; set all to 0 .

	if(filename)										// If filename is a null pointer create an empty packet.
	{
		// Create a block with the filename and length: <filename>[00]<asciisize>[00][00][00][00]....
		memcpy(block,filename,strlen(filename));			// Copy the filename
		idx = strlen(filename)+1;							// Point to after the file and null-terminator
		sprintf((char*)&block[idx],"%u",len);						// Convert size to ascii
	}

#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Before sending packet. Dumping first 64 bytes:\n");
	for(int i=0;i<64;i++)
		fprintf(file_dbg,"%02X ",i);
	fprintf(file_dbg,"\n");
	for(int i=0;i<64;i++)
		fprintf(file_dbg,"%02X ",block[i]);
	fprintf(file_dbg,"\n");
#endif

	// Send the packet; possibly repeating if needed
	return ymodem_send_packet(fintf,block,0);					// Send packet 0 - implicitly packet 0 defines the size (128 bytes)
}


// Return 0 on success
unsigned char ymodem_sync(FILE *fintf)
{
	int ch;
	unsigned timeoutsync = 10;				// Overall timeout of the sync in seconds
	unsigned timoutcrc=200;					// Timeout waiting for a reply after sending the CRC in milliseconds


	fprintf(file_pri,"YMODEM transfer: initiate YMODEM reception on the host within the next %d seconds\n",timeoutsync);

	// Empty the receive buffers
	while(fgetc_timeout(fintf,100)!=-1);


	// Sending CRC should return CRC - timeout after 10 seconds
	for(unsigned i=0;i<timeoutsync*1000;i+=timoutcrc)
	{
		fputc(YMODEM_CRC,fintf);
		// Flush otherwise data stays in our buffers
		fflush(fintf);
		ch = fgetc_timeout(fintf,timoutcrc);
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Sent CRC, received %d\n",ch);
#endif

		if(ch == YMODEM_CRC)			// OK
			return 0;

	}

	// Could not sync - error
	return 1;
}
void ymodem_end(FILE *fintf)
{
	fputc(YMODEM_CAN,fintf);
	fputc(YMODEM_CAN,fintf);
	fflush(fintf);
}
/******************************************************************************
	function: ymodem_send
*******************************************************************************
	Send one file by YMODEM; the payload must be available in memory

	Parameters:
		filename 	-		Name of the file
		data		-		Pointers to the file data
		len			-		File length
		fints		-		Interface over which to send the data

	Returns:
		0			-		Success
		Nonzero		-		Failure

******************************************************************************/
unsigned char ymodem_send(const char *filename,unsigned char *data,unsigned len,FILE *fintf)
{
	int ch;
	// Send the data to interface fintf


	// Change the interface to blocking write, to avoid overflowing buffers (in principle our buffers are larger than 1024)
	serial_setblockingwrite(fintf,1);


#if YMODEM_DEBUG_OTHER==1
	fprintf(file_dbg,"Sending file: name '%s' size %d\n",filename,len);
#endif

#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Sync\n");
#endif

	if(ymodem_sync(fintf))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error sync - aborting\n");
#endif
		ymodem_end(fintf);
		return 1;		// Fail.
	}



	if(ymodem_send_packet0(fintf,filename,len))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error sending packet 0 - aborting\n");
#endif
		ymodem_end(fintf);
		return 1;
	}

	// Should get a CRC confirming the file can be created by the receiver
	ch = fgetc_timeout(fintf, YMODEM_PACKET_TIMEOUT);
#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Response after packet 0: %d\n",ch);
#endif

	if(ch!=YMODEM_CRC)
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error after packet 0: %d\n",ch);
#endif
		ymodem_end(fintf);
		return 1;
	}

	// Receiver could create file: send data of the file
	if(ymodem_send_data_packets(fintf,data,len))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error sending data packets\n");
#endif
		ymodem_end(fintf);
		return 1;
	}

	// Notify EOT
	if(ymodem_notify_eot(fintf))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error after EOT: %d\n",ch);
#endif
		ymodem_end(fintf);
		return 1;
	}

	// Should get a CRC asking for info on the next file
	ch = fgetc_timeout(fintf, YMODEM_PACKET_TIMEOUT);

#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Response after file transfer: %d\n",ch);
#endif

	if(ch!=YMODEM_CRC)
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error didn't get next file request\n");
#endif
		ymodem_end(fintf);
		return 1;
	}

	// Send next file to notify end of transfer
	if(ymodem_send_packet0(fintf,0, 0))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error sending terminating empty packet\n");
#endif
		ymodem_end(fintf);
		return 1;
	}




	// Restore non-blocking writes
	serial_setblockingwrite(fintf,0);

	return 0;
}

/******************************************************************************
	function: ymodem_send_multi
*******************************************************************************
	Send multiple files by YMODEM; the payload must be available in memory.

	Parameters:
		nfiles		-		Number of files
		filename 	-		Array of strings with the names of each files
		data		-		Array of pointers to the data corresponding to the files
		len			-		Array of file lengths
		fints		-		Interface over which to send the data

	Returns:
		0			-		Success
		Nonzero		-		Failure

******************************************************************************/
unsigned char ymodem_send_multi(unsigned nfiles,const char *filename[],unsigned char *data[],unsigned len[],FILE *fintf)
{
	int ch;

#if YMODEM_DEBUG_OTHER==1
	// Check the parameters received
	fprintf(file_dbg,"Number of files: %d\n",nfiles);
	for(int i=0;i<nfiles;i++)
	{
		fprintf(file_dbg,"File: %d. '%s' %d\n",i,filename[i],len[i]);
		fprintf(file_dbg,"Data:\n");
		for(int j=0;j<10;j++)
		{
			fprintf(file_dbg,"%02X ",data[i][j]);
		}
		fprintf(file_dbg,"\n\n");
	}
#endif




	// Change the interface to blocking write, to avoid overflowing buffers (in principle our buffers are larger than 1024)
	serial_setblockingwrite(fintf,1);

#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Sync\n");
#endif

	if(ymodem_sync(fintf))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error sync - aborting\n");
#endif
		ymodem_end(fintf);
		return 1;		// Fail.
	}


	// Send all files
	for(int fi=0;fi<nfiles;fi++)
	{
#if YMODEM_DEBUG_OTHER==1
		fprintf(file_dbg,"Sending file %d. %s len %d\n",fi,filename[fi],len[fi]);
#endif


		if(ymodem_send_packet0(fintf,filename[fi],len[fi]))
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error sending packet 0 - aborting\n");
#endif
			ymodem_end(fintf);
			return 1;
		}

		// Should get a CRC confirming the file can be created by the receiver
		ch = fgetc_timeout(fintf, YMODEM_PACKET_TIMEOUT);

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Response after packet 0: %d\n",ch);
#endif

		if(ch!=YMODEM_CRC)
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error after packet 0: %d\n",ch);
#endif
			ymodem_end(fintf);
			return 1;
		}

		// Receiver could create file: send data of the file
		if(ymodem_send_data_packets(fintf,data[fi],len[fi]))
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error sending data packets\n");
#endif
			ymodem_end(fintf);
			return 1;
		}

		// Notify EOT
		if(ymodem_notify_eot(fintf))
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error after EOT: %d\n",ch);
#endif
			ymodem_end(fintf);
			return 1;
		}

		// Should get a CRC asking for info on the next file
		ch = fgetc_timeout(fintf, YMODEM_PACKET_TIMEOUT);

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Response after file transfer: %d\n",ch);
#endif

		if(ch!=YMODEM_CRC)
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error didn't get next file request\n");
#endif
			ymodem_end(fintf);
			return 1;
		}
	}

	// Send next file to notify end of transfer
	if(ymodem_send_packet0(fintf,0, 0))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error sending terminating empty packet\n");
#endif
		ymodem_end(fintf);
		return 1;
	}




	// Restore non-blocking writes
	serial_setblockingwrite(fintf,0);

	return 0;
}



/******************************************************************************
	function: ymodem_send_multi_file
*******************************************************************************
	Send multiple files by YMODEM; the payload is loaded from the file during transfer

	Parameters:
		nfiles		-		Number of files
		filename 	-		Array of strings with the names of each files
		fints		-		Interface over which to send the data

	Returns:
		0			-		Success
		Nonzero		-		Failure

******************************************************************************/
unsigned char ymodem_send_multi_file(unsigned nfiles,const char *filename[],FILE *fintf)
{
	int ch;
	unsigned time_1,time_2;
	unsigned totsize=0;
	unsigned success=0;

#if YMODEM_DEBUG_OTHER==1
	// Check the parameters received
	fprintf(file_dbg,"Number of files: %d\n",nfiles);
	for(int i=0;i<nfiles;i++)
	{
		fprintf(file_dbg,"File: %d. '%s'\n",i,filename[i]);
	}
#endif

	// Check the filesystem
	if(ufat_initcheck())
		return 1;


	// Change the interface to blocking write, to avoid overflowing buffers (in principle our buffers are larger than 1024)
	serial_setblockingwrite(fintf,1);


#if YMODEM_DEBUG_PROTOCOL==1
	fprintf(file_dbg,"Sync\n");
#endif

	if(ymodem_sync(fintf))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error sync - aborting\n");
#endif
		ymodem_end(fintf);
		_ufat_unmount();
		return 1;		// Fail.
	}

	// Start benchmarking
	time_1 = timer_ms_get();


	// Send all files
	for(int fi=0;fi<nfiles;fi++)
	{
#if YMODEM_DEBUG_OTHER==1
		fprintf(file_dbg,"File %d: %s\n",fi,filename[fi]);
#endif

		// Open the file
		FIL fil;            // File object
		int fsize;
		FRESULT res = f_open(&fil, filename[fi], FA_READ);
		if (res)
		{
#if YMODEM_DEBUG_OTHER==1
			fprintf(file_dbg,"Error opening file '%s': skipping file\n",filename[fi]);
#endif
			continue;
		}

		// File has been opened - get size
		fsize = f_size(&fil);

#if YMODEM_DEBUG_OTHER==1
		fprintf(file_dbg,"File %d: size: %d\n",fi,fsize);
#endif


		if(ymodem_send_packet0(fintf,filename[fi],fsize))
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error sending packet 0 - aborting\n");
#endif
			ymodem_end(fintf);
			f_close(&fil);
			_ufat_unmount();
			return 1;
		}

		// Should get a CRC confirming the file can be created by the receiver
		ch = fgetc_timeout(fintf, YMODEM_PACKET_TIMEOUT);

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Response after packet 0: %d\n",ch);
#endif

		if(ch!=YMODEM_CRC)
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error after packet 0: %d\n",ch);
#endif
			ymodem_end(fintf);
			f_close(&fil);
			_ufat_unmount();
			return 1;
		}

		// Receiver could create file: send data of the file
		if(ymodem_send_data_packets_file(fintf,fsize,&fil))
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error sending data packets\n");
#endif
			ymodem_end(fintf);
			f_close(&fil);
			_ufat_unmount();
			return 1;
		}

		// File can be closed
		f_close(&fil);

		// Notify EOT
		if(ymodem_notify_eot(fintf))
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error after EOT: %d\n",ch);
#endif
			ymodem_end(fintf);
			_ufat_unmount();
			return 1;
		}

		// Should get a CRC asking for info on the next file
		ch = fgetc_timeout(fintf, YMODEM_PACKET_TIMEOUT);

#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Response after file transfer: %d\n",ch);
#endif

		if(ch!=YMODEM_CRC)
		{
#if YMODEM_DEBUG_PROTOCOL==1
			fprintf(file_dbg,"Error didn't get next file request\n");
#endif
			ymodem_end(fintf);
			_ufat_unmount();
			return 1;
		}

		// Update total size sent
		totsize+=fsize;
		success++;
	}

	// Stop benchmarking
	time_2 = timer_ms_get();

	_ufat_unmount();

	// Send next file to notify end of transfer
	if(ymodem_send_packet0(fintf,0, 0))
	{
#if YMODEM_DEBUG_PROTOCOL==1
		fprintf(file_dbg,"Error sending terminating empty packet\n");
#endif
		ymodem_end(fintf);
		return 1;
	}

	// Restore non-blocking writes
	serial_setblockingwrite(fintf,0);

	// Print status
	fprintf(file_pri,"\n");
	fprintf(file_pri,"Transferred %u of %u files in %u ms for %u bytes. Speed: %u KB/s\n\n",success,nfiles,time_2-time_1,totsize,totsize*125/128/(time_2-time_1));

	return 0;
}
