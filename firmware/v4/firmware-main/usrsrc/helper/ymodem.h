/*
 * ymodem.h
 *
 *  Created on: 19 sept. 2020
 *      Author: droggen
 */

#ifndef HELPER_YMODEM_H_
#define HELPER_YMODEM_H_


#include <stdio.h>
#include "fatfs.h"

#define YMODEM_PACKET_SIZE             	(128)
#define YMODEM_PACKET_1K_SIZE          	(1024)
#define YMODEM_SOH 						(0x01)		// 128-byte data packet
#define YMODEM_STX 						(0x02)  	// start of 1024-byte data packet
#define YMODEM_EOT (0x04)
#define YMODEM_ACK (0x06)
#define YMODEM_NAK (0x15)
#define YMODEM_CAN (0x18)      // two of these in succession aborts transfer
#define YMODEM_PACKET_TIMEOUT          	(1000)		// milliseconds
#define YMODEM_CRC 						(0x43)      // use in place of first NAK for CRC mode


unsigned short ymodem_crc16(const unsigned char *buf, unsigned size);
unsigned char ymodem_send_packet(FILE *fintf,unsigned char *packet, int block_no);
void _ymodem_send_packet(FILE *fintf,unsigned char *packet, int block_no);
unsigned char ymodem_send_data_packets(FILE *fintf,unsigned char* data, unsigned size);
unsigned char ymodem_send_data_packets_file(FILE *fintf,unsigned size,FIL *fil);
unsigned char ymodem_send_packet0(FILE *fintf,const char *filename,unsigned len);
unsigned char ymodem_send(const char *filename,unsigned char *data,unsigned len,FILE *fintf);
unsigned char ymodem_send_multi(unsigned nfiles,const char *filename[],unsigned char *data[],unsigned len[],FILE *fintf);
unsigned char ymodem_send_multi_file(unsigned nfiles,const char *filename[],FILE *fintf);
void ymodem_end(FILE *fintf);
unsigned char ymodem_sync(FILE *fintf);


#endif /* HELPER_YMODEM_H_ */
