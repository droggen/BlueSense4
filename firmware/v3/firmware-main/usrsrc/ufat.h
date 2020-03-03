/*

 * ufat.h
 *
 *  Created on: 10 oct. 2019
 *      Author: droggen
 */


#ifndef __UFAT_H
#define __UFAT_H

#include <stdio.h>
#include "fatfs.h"
#include "serial.h"

#define _UFAT_NUMLOGENTRY 14

typedef struct {
	// Data about card. This data is obtained from the low-level SD card interface
	//unsigned long card_capacity_sector;					// Card capacity as reported by the SD-card
	// Data about partition. This data is obtained from the boot sector of the partition.
	//unsigned long partition_capacity_sector;			// Size of the partition in sectors, including boot, fat, root, data
	//unsigned long partition_sector;						// This is the address of the BOOT sector of the first partition
	//unsigned long fat_sector;							// Sector containing the FAT1
	//unsigned long fat2_sector;							// Sector containing the FAT2 or 0 if there is no FAT2
	//unsigned long cluster_begin;						// Sector where the first cluster is located; the first cluster is cluster 2 (cluster 0,1 don't exist). The fat however has clusters 0,1 in its entries, i.e. the first FAT sector contains cluster 0-127
	//unsigned char sectors_per_cluster;					// Number of sectors per cluster
	//unsigned long root_cluster;							// Location of the root cluster
	//unsigned long numclusters;
	// Data about logs. This information is hidden in last entry of root.
	unsigned char lognum;
	//unsigned long logstartcluster;
	//unsigned long logsizecluster;
	unsigned long long curlogsize;									// Current log size in bytes
	unsigned long long logsizebytes;							// Maximum log file size
	// Filesystem
	// Availability
	unsigned char fs_available;							// Indicates that the card is formatted with the uFAT and therefore files can be written
	//unsigned char card_available;						// Indicates that low-level card initialisation is successful: an sd-card is available
	unsigned char card_isinitialised;					// If zero, indicates that a low-level SD interface and card initialisation sequence must be performed
	// Current log file
	FIL file_current_log;								// File handle
	FILE *file_current_log_file;						// Stdio file handle used to close the file
} FSINFO;

extern FSINFO _fsinfo;												// Summary of key info here

#define UFAT_VBUFMAX	16384							// setvbuf buffer
extern char _ufat_vbuf[];

unsigned char ufat_init();
unsigned char ufat_initcheck();
unsigned char _ufat_mount();
unsigned char _ufat_unmount();
unsigned char _ufat_init_fs(void);
unsigned char ufat_available(void);
unsigned char ufat_format(unsigned char numlogfile);
unsigned char ufat_format_partial(unsigned char numlogfile);
void _ufat_lognumtoname(unsigned char n,unsigned char *name);
FILE *ufat_log_open(unsigned char n);
FILE *ufat_log_open_read(unsigned char n);
unsigned char ufat_log_close(void);
ssize_t ufat_cookie_read(void *__cookie, char *__buf, size_t __n);
ssize_t ufat_cookie_write(void *__cookie, const char *__buf, size_t __nbytes);
int ufat_cookie_close(void *__cookie);
void ufat_volumeinfo();
unsigned char ufat_erasefiles(int fileno);
int ufat_listfiles();
unsigned char _ufat_mountcheck();
void ufat_notifysdcardchange(unsigned char sddetect);
unsigned char _ufat_log_fputbuf(SERIALPARAM *sp,char *buffer,unsigned short size);
unsigned long long ufat_log_getmaxsize(void);
unsigned long long ufat_log_getsize(void);
void _ufat_estimatecardspace(int numlogfile);
unsigned char ufat_log_getnumlogs(void);

#endif
