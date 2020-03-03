/*
 * ufat.c
 *
 *  Created on: 11 oct. 2019
 *      Author: droggen
 */
#include <string.h>
#include "main.h"
#include "usrmain.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "sdmmc.h"
#include "bsp_driver_sd.h"
#include "fatfs.h"
#include "ufat.h"

#include "wait.h"

#include "global.h"
#include "sd/stm_sdio.h"

FATFS __fs;           // Filesystem object
//BYTE __fs_work[FF_MAX_SS]; // Work area (larger is better for processing time)
BYTE __fs_work[1000]; // Work area (larger is better for processing time)

FSINFO _fsinfo;												// Summary of key info here

char _ufat_vbuf[UFAT_VBUFMAX];						// setvbuf buffer

SERIALPARAM _log_file_param;

/*
	Does mounting and scanning the file system
*/
unsigned char ufat_init()
{
	_fsinfo.file_current_log_file = 0;			// No file open


	if(_ufat_mount())
		return 1;

	if(_ufat_init_fs())
	{
		_ufat_unmount();
		return 2;
	}
	return 0;
}




// 0: success
unsigned char _ufat_mount()
{
	FRESULT res;        // API result code

	_ufat_unmount();


	// Check if the card low-level initialisation must be performed
	if(!_fsinfo.card_isinitialised)
	{
		if(stm_sdm_init()==0)				// FatFs calls this via disk_initialize, but does not handle card insertion/removal
			_fsinfo.card_isinitialised=1;
		else
			_fsinfo.card_isinitialised=0;
	}


	// Register the file system object
	// Register work area
	if((res=f_mount(&__fs, "", 1))!=FR_OK)
	{
		fprintf(file_pri,"Mount error (%d)\n",res);
		return 1;
	}

	_fsinfo.card_isinitialised=1;

	return 0;
}

unsigned char _ufat_unmount()
{
	FRESULT res;        // API result code
	// Unregister work area
	if((res=f_mount(0, "", 0))!=FR_OK)
	{
		fprintf(file_pri,"Unmount error (%d)\n",res);
		return 1;
	}

	_fsinfo.fs_available=0;

	return 0;
}


/******************************************************************************
	function: ufat_format
*******************************************************************************
	Format a card with the uFAT filesystem. The filesystem is ready to be used immediately afterwards.

	Performs a low-level initialisation of the SD card, formats the card, and
	initialises the internal data structures.

	After formatting there is no need to call ufat_init to initialise the filesystem.

	This function assumes that the SPI peripheral has been initialised.
	It performs low-level SD-card initialisation followed by formatting.
	It attemps to strike a balance between erasing all the sectors of the card, which would be too slow on an avr (with a streaming block write this could takes ~23 hours at ~200KB/s with a 16GB card), and erasing a minimum set.

	The sectors corresponding to 3 clusters of the ROOT (3*64 sectors) are cleared, as _ufat_format_fat_root initialises 3 clusters for the ROOT (note: this 3-cluster assumption is based on reverse engineering of a computer formatted file system and not fully analysed).
	The sectors of the FAT1 and FAT2 (i.e. all sectors between the beginning of FAT1 and the start of the cluster area) are cleared.


	Parameters:
		numlogfile			-	Number of log files to create (between 1 and _UFAT_NUMLOGENTRY).

	Returns:
		0					-	Success
		1					-	Error
******************************************************************************/
unsigned char ufat_format(unsigned char numlogfile)
{
	unsigned char rv;
	FRESULT res;        // API result code

	unsigned t1,t2;

	// Check if a log file is open
	if(_fsinfo.file_current_log_file)
	{
		fprintf(file_pri,"Log file open - cannot format\n");
		return 1;
	}


	// Sanity check of parameter value
	if(numlogfile<1)
		numlogfile=1;
	if(numlogfile>_UFAT_NUMLOGENTRY)
		numlogfile=_UFAT_NUMLOGENTRY;


	// Unmount
	fprintf(file_pri,"\tUnmounting filesystem\n");
	_ufat_unmount();
	_ufat_mount();

	fprintf(file_pri,"\tFormatting...\n");
	// Create FAT volume */
	t1=timer_ms_get();
	res = f_mkfs("", FM_ANY, 0, __fs_work, sizeof __fs_work);
	t2=timer_ms_get();
	if(res)
	{
		fprintf(file_pri,"Format error (%d) in %u ms\n",res,t2-t1);
		return 1;
	}
	fprintf(file_pri,"Format success in %u ms\n",t2-t1);

	ufat_format_partial(numlogfile);

	return 0;
}

unsigned char ufat_format_partial(unsigned char numlogfile)
{
	FIL fil;            // File object
	FRESULT res;        // API result code
	UINT bw;            // Bytes written

	// Mount the filesystem
	if(_ufat_mountcheck())
		return 1;

	fprintf(file_pri,"Filesystem mounted\n");



	// Find the available disk space - this must be called after mount
	DWORD fre_clust,tot_sect,fre_sect;
	FATFS *fs;
	res = f_getfree("",&fre_clust, &fs);

	if(res!=FR_OK)
	{
		fprintf(file_pri,"Cannot get card capacity\n");
		return 1;
	}

	tot_sect = (fs->n_fatent - 2) * fs->csize;
	fre_sect = fre_clust * fs->csize;			// Available space in sector

	// Print the free space (assuming 512 bytes/sector)
	fprintf(file_pri,"Card capacity: %luKB (clusters: %u). Available: %luKB (clusters: %u). Sectors per clusters: %u\n", tot_sect / 2, fs->n_fatent-2,fre_sect / 2,fre_clust,fs->csize);


	// Make sure we have enough log files not to have logs larger than 4GB. (fre_sect/2) is space in KB.
	unsigned char adj=0;
	while( (fre_sect/2)/numlogfile >  4194303)
	{
		numlogfile++;
		//fprintf(file_pri,"adjusting number of log files\n");
		adj=1;
	}
	if(adj)
		fprintf(file_pri,"Adjusted number of log files to guarantee <4GB files: %d\n",numlogfile);

	// Compute the max log file size assuming equal sized logs and a 10% space reserved for other files
	//unsigned capacity = 512*(fre_sect-fre_sect/10);		// capacity in bytes
	//fprintf(file_pri,"Fre_cluster: %u\n",fre_clust);
	unsigned long long capacity = fre_clust-fre_clust/10;					// Reduce the capacity in cluster count by 10%
	//fprintf(file_pri,"use sect : %lu\n",capacity);
	capacity = capacity/numlogfile;											// Maximum ideal size per log file in clusters
	//fprintf(file_pri,"Capacity per log in clusters: %lu\n",capacity);
	_fsinfo.logsizebytes = capacity*fs->csize*512ll;						// Convert capacity to bytes

	//fprintf(file_pri,"logsizebytes: %lu\n",_fsinfo.logsizebytes);

	fprintf(file_pri,"Max expected log size for pre-allocation: %luMB\n",_fsinfo.logsizebytes>>20);


	// Create empty log files

	for(unsigned l=0;l<numlogfile;l++)
	{
		char fn[64];
		_ufat_lognumtoname(l,fn);


		// time
		//_logentries[i].time = _ufat_secfrommidnight_to_fattime(timer_s_get_frommidnight());

		// Create a file as new
		res = f_open(&fil, fn, FA_CREATE_NEW | FA_WRITE);
		if(res)
		{
			fprintf(file_pri,"Error creating log file %u (%s)\n",l,fn);
			return 1;
		}

		// Close the file
		if((res=f_close(&fil))!=FR_OK)
		{
			fprintf(file_pri,"Error closing log file %u (%s) error\n",l,fn);
			return 1;
		}

	}

	// Here must reread root to initialise the FS; while we should be okay and could just set fs as available, we piggy back on _ufat_init_fs
	_ufat_init_fs();

	// unmount the file system
	_ufat_unmount();

	return 0;
}
void _ufat_estimatecardspace(int numlogfile)
{
	// Find the available disk space - this must be called after mount
	FRESULT res;        // API result code
	DWORD fre_clust,tot_sect,fre_sect;
	FATFS *fs;
	res = f_getfree("",&fre_clust, &fs);

	if(res!=FR_OK)
	{
		fprintf(file_pri,"Cannot get card capacity\n");
		return;
	}

	tot_sect = (fs->n_fatent - 2) * fs->csize;
	fre_sect = fre_clust * fs->csize;			// Available space in sector

	// Print the free space (assuming 512 bytes/sector)
	fprintf(file_pri,"\tCard capacity: %luKB (clusters: %u). Available: %luKB (clusters: %u). Sectors per clusters: %u\n", tot_sect / 2, fs->n_fatent-2,fre_sect / 2,fre_clust,fs->csize);


	// Compute the max log file size assuming equal sized logs and a 10% space reserved for other files
	//unsigned capacity = 512*(fre_sect-fre_sect/10);		// capacity in bytes
	//fprintf(file_pri,"Fre_cluster: %u\n",fre_clust);
	unsigned long long capacity = fre_clust-fre_clust/10;					// Reduce the capacity in cluster count by 10%
	//fprintf(file_pri,"use sect : %lu\n",capacity);
	capacity = capacity/numlogfile;											// Maximum ideal size per log file in clusters
	//fprintf(file_pri,"Capacity per log in clusters: %lu\n",capacity);
	_fsinfo.logsizebytes = capacity*fs->csize*512ll;						// Convert capacity to bytes

	//fprintf(file_pri,"logsizebytes: %lu\n",_fsinfo.logsizebytes);

	fprintf(file_pri,"\tMax expected log size for pre-allocation: %luMB\n",_fsinfo.logsizebytes>>20);

}

/******************************************************************************
	function: _ufat_init_fs
*******************************************************************************
	Initialise the uFAT filesystem data structures from a uFAT-formatted card.

	Must be called after _ufat_init_sd

	Assumes a card formatted with uFAT. Reads the MBR, boot sector, and root entry and
	initialise the FSINFO structure with data related to the card.

	Updates:
	_fsinfo.fs_available
	_fsinfo.logstartcluster
	_fsinfo.logsizecluster
	_fsinfo.logsizebytes
	_fsinfo.lognum

	Parameters:
		capacity_sector		-	Capacity of the card in number of sectors

	Returns:
		0					-	Success
		1					-	Error
******************************************************************************/
unsigned char _ufat_init_fs(void)
{
	unsigned char rv;

	// The filesystem is not available yet
	//_fsinfo.fs_available=0;

	// Find how many log files are available

	unsigned l=0;
	while(1)
	{
		unsigned char fn[64];
		_ufat_lognumtoname(l,fn);

		// See if file available
		FRESULT res;
		FILINFO finfo;
		if( (res=f_stat(fn,&finfo)) != FR_OK)
		{
			break;
		}
		//fprintf(file_pri,"Logfile %u - %s ok\n",l,fn);
		l++;
	}

	_fsinfo.lognum=l;

	fprintf(file_pri,"Total log files: %u\n",_fsinfo.lognum);

	_ufat_estimatecardspace(_fsinfo.lognum);




	// The filesystem is available
	_fsinfo.fs_available=1;

	return 0;

}
void _ufat_lognumtoname(unsigned char n,unsigned char *name)
{
	// File name
	#if UFAT_INCLUDENODENAME==1
		sprintf(name,"LOG-%s",system_getdevicename());
	#else
		strcpy(name,"LOG-0000");
	#endif
	// Extension
	sprintf(&name[strlen(name)],".%03u",n);
}
/******************************************************************************
	function: ufat_log_open
*******************************************************************************
	Opens the indicated log file for write operations using fprintf, fputc, fputbuf, etc.

	Only one log file can be open at any time.

	Parameters:
		n			-	Number of the log file to open; the maximum number available depends on how the card was formatted.

	Returns:
		0			-	Error
		nonzero		-	FILE* for write operations
******************************************************************************/
FILE *ufat_log_open(unsigned char n)
{
	//printf("ufat log open\n");
	// Check if log file already open
	if(_fsinfo.file_current_log_file)
	{
		fprintf(file_pri,"Log file already open\n");
		return 0;
	}

	// Sanity check: fs must be initialised and lognumber available
	// Mount the filesystem
	/*if(_ufat_mount())
	{
		fprintf(file_pri,"Error mounting filesystem\n");
		return 0;
	}*/
	if(ufat_initcheck())
		return 0;
	if(n>=_fsinfo.lognum)
	{
		#ifdef UFATDBG
			printf("lognum larger than available\n");
		#endif
		return 0;
	}




	// File name
	char fn[64];
	_ufat_lognumtoname(n,fn);



	fprintf(file_pri,"Opening log %s\n",fn);
	FRESULT res = f_open(&_fsinfo.file_current_log, fn, FA_CREATE_ALWAYS | FA_WRITE);
	if(res)
	{
		fprintf(file_pri,"Error opening the file\n");
		return 0;
	}

	// Preallocate
	fprintf(file_pri,"Preallocating %uMB of contiguous space\n",_fsinfo.logsizebytes>>20);
	res = f_expand (&_fsinfo.file_current_log,_fsinfo.logsizebytes,0);
	if(res!=FR_OK)
	{
		fprintf(file_pri,"Cannot allocate continuous %uMB - continuing without continuous space\n",_fsinfo.logsizebytes>>20);
	}

	// Initialise a FILE structure for writing using
	cookie_io_functions_t iof;
	iof.read = &ufat_cookie_read;
	iof.write = &ufat_cookie_write;
	iof.close = 0;
	iof.seek = 0;
	FILE *file = fopencookie(0,"w+",iof);

	// Initialise our parameter structure with the buffer write function and store it in the FILE structure
	_log_file_param.putbuf = _ufat_log_fputbuf;
	_log_file_param.device = (void*) file;			// Hack to pass our FILE to fputbuf
	file->_cookie = &_log_file_param;


	if(file==0)
	{
		fprintf(file_pri,"Error opening the file handle\n");
		return 0;
	}

	// Keep the file for later closing
	_fsinfo.file_current_log_file = file;

	// Clear the size of file
	_fsinfo.curlogsize=0;


#warning "tofix: must try what is a good buffering strategy"
	// No buffering leads to cookie write one byte at a time.
	//setvbuf (file, 0, _IONBF, 0 );	// No buffering
	//setvbuf (file, 0, _IOLBF, 1024);	// Line buffer buffering
	//setvbuf (file, 0, _IOLBF, 16);	// Line buffer buffering


	// FatFS formats with 64 sector/cluster (32KB).
	//setvbuf (file, 0, _IOLBF, 256);	// Line buffer buffering (good - but all the low-level writes are single sector)
	//setvbuf (file, 0, _IOLBF, 2048);	// Line buffer buffering (faster - low-level writes are 4 sectors)
	//setvbuf (file, 0, _IOLBF, 4096);	// Line buffer buffering (faster - low-level writes are 8 sectors, but speed decreases from 171kb/s with 8GB card to 71 with 32GB card)
	//setvbuf (file, 0, _IOLBF, 8192);	// Line buffer buffering (faster - low-level writes are 8 sectors, but speed decreases from 171kb/s with 8GB card to 71 with 32GB card)
	//setvbuf (file, 0, _IOLBF, 16384);
	//setvbuf (file, 0, _IOFBF, 8192);
	//setvbuf (file, 0, _IOFBF, 16384);
	//setvbuf (file, 0, _IOFBF, 2048);
	//setvbuf (file, 0, _IOFBF, 2048);
	setvbuf(file,_ufat_vbuf,_IOFBF,UFAT_VBUFMAX);


	return file;
}

FILE *ufat_log_open_read(unsigned char n)
{
	//printf("ufat log open\n");
	// Check if log file already open
	if(_fsinfo.file_current_log_file)
	{
		fprintf(file_pri,"Log file already open\n");
		return 0;
	}

	// Sanity check: fs must be initialised and lognumber available
	// Mount the filesystem
	/*if(_ufat_mount())
	{
		fprintf(file_pri,"Error mounting filesystem\n");
		return 0;
	}*/
	if(ufat_initcheck())
		return 0;
	if(n>=_fsinfo.lognum)
	{
		#ifdef UFATDBG
			printf("lognum larger than available\n");
		#endif
		return 0;
	}

	// File name
	char fn[64];
	_ufat_lognumtoname(n,fn);

	fprintf(file_pri,"Opening log %s\n",fn);
	FRESULT res = f_open(&_fsinfo.file_current_log, fn, FA_OPEN_ALWAYS|FA_READ);
	if(res)
	{
		fprintf(file_pri,"Error\n");
		return 0;
	}

	// Get file size
	_fsinfo.curlogsize = f_size(&_fsinfo.file_current_log);


	// Initialise a FILE structure for writing using
	cookie_io_functions_t iof;
	iof.read = &ufat_cookie_read;
	iof.write = &ufat_cookie_write;
	iof.close = 0;
	iof.seek = 0;
	FILE *file = fopencookie(0,"r",iof);

	if(file==0)
	{
		fprintf(file_pri,"Error opening the file handle\n");
		return 0;
	}

	// Keep the file for later closing
	_fsinfo.file_current_log_file = file;

#warning "tofix: must try what is a good buffering strategy"
	//setvbuf (f, 0, _IONBF, 0 );	// No buffering
	//setvbuf (f, 0, _IOLBF, 1024);	// Line buffer buffering
	//setvbuf (file, 0, _IOLBF, 16);	// Line buffer buffering
	setvbuf (file, 0, _IOLBF, 256);	// Line buffer buffering


	return file;
}
/******************************************************************************
	function: ufat_log_close
*******************************************************************************
	Close the previously opened log file

	Returns:
		0			-	ok
		1			-	Error

******************************************************************************/
unsigned char ufat_log_close(void)
{

	if(_fsinfo.file_current_log_file==0)
	{
		fprintf(file_pri,"Log file not open\n");
		return 1;
	}

	// Close the stdio file first
	fclose(_fsinfo.file_current_log_file);
	_fsinfo.file_current_log_file = 0;

	// Close the low-level file
	FRESULT res = f_close(&_fsinfo.file_current_log);
	if(res!=FR_OK)
	{
		fprintf(file_pri,"Error closing log file (%d)\n",res);
		return 1;
	}

	// Unmount the filesystem
	_ufat_unmount();


	return 0;
}


/******************************************************************************
	serial_uart_cookiewrite_int
*******************************************************************************
	Used with fcookieopen to write a buffer to a stream.

	This function is interrupt driven, i.e. uses a circular buffer.

	Return value:
		number of bytes written (0 if none)
******************************************************************************/
ssize_t ufat_cookie_write(void *__cookie, const char *__buf, size_t __nbytes)
{
	(void) __cookie;
	unsigned bw;

	//fprintf(file_pri,"ufat_cookie_write %d\n",__nbytes);
	//itmprintf("ufat_cookie_write %d\n",__nbytes);

	FRESULT res = f_write(&_fsinfo.file_current_log,__buf,__nbytes,&bw);
	if(res!=FR_OK)
	{
		fprintf(file_pri,"Log write error\n");
		//return 0;
	}
	_fsinfo.curlogsize += bw;


	return bw;
}
/******************************************************************************
	serial_uart_cookieread_int
*******************************************************************************
	Used with fcookieopen to read a buffer from a stream.

	This function is interrupt driven, i.e. uses a circular buffer.

	Return value:
		number of bytes read (0 if none)
******************************************************************************/
ssize_t ufat_cookie_read(void *__cookie, char *__buf, size_t __n)
{
	(void) __cookie;


	//fprintf(file_pri,"ufat_cookie_read %d\n",__n);
	unsigned br;

	FRESULT res = f_read(&_fsinfo.file_current_log,__buf,__n,&br);
	//fprintf(file_pri,"res: %d br: %d",res,br);
	if(res!=FR_OK)
		return 0;

	return br;


	// If no data, should return error (-1) instead of eof (0).
	// Returning eof has side effects on subsequent calls to fgetc: it returns eof forever, until some write is done.
	return -1;
}
// Unused
int ufat_cookie_close(void *__cookie)
{
	(void) __cookie;
	//fprintf(file_pri,"ufat_cookie_close\n");
	return 0;	// 0: success; EOF: error
}
/******************************************************************************
	function: ufat_log_getsize
*******************************************************************************
	Returns the size of the currently open file.

	This function requires the filesystem to be initialised and a log file to be
	open to return a valid result.

	Returns:
		0			-	Error
		nonzero		-	Maximum size of the files
******************************************************************************/
unsigned long long ufat_log_getsize(void)
{
	// Sanity check: fs must be initialised and lognumber available
	/*if(_fsinfo.fs_available==0)
	{
		#ifdef UFATDBG
			printf("fs not available\n");
		#endif
		return 0;
	}*/

	return _fsinfo.curlogsize;

	//return f_size(&_fsinfo.file_current_log);
}


void ufat_volumeinfo()
{
	// Find the available disk space - this must be called after mount
	FRESULT res;        // API result code
	DWORD fre_clust,tot_sect,fre_sect;
	FATFS *fs;
	res = f_getfree("",&fre_clust, &fs);

	if(res!=FR_OK)
	{
		fprintf(file_pri,"Cannot get card capacity\n");
		return;
	}

	tot_sect = (fs->n_fatent - 2) * fs->csize;
	fre_sect = fre_clust * fs->csize;			// Available space in sector

	// Print the free space (assuming 512 bytes/sector)
	fprintf(file_pri,"Card capacity: %luKB (clusters: %lu). Available: %luKB (clusters: %lu). Sectors per clusters: %u\n", tot_sect / 2, fs->n_fatent-2,fre_sect / 2,fre_clust,fs->csize);

}
/*
 * fileno: zero-based file to erase (same order as ufat_listfiles) or -1 to erase all
 */
unsigned char ufat_erasefiles(int fileno)
{
	FRESULT res;
	DIR dir;
	static FILINFO fno;
	char buffer[256];


	if(_ufat_mountcheck())
		return 1;

	res = f_opendir(&dir,"/");                       // Open the directory
	if (res != FR_OK)
	{
		fprintf(file_pri,"Cannot read root directory\n");
		_ufat_unmount();
		return 1;
	}

	int filectr=0;
	while(1)
	{
		res = f_readdir(&dir, &fno);                   // Read a directory item
		if (res != FR_OK || fno.fname[0] == 0)		// Break on error or end of dir
			break;
		if (fno.fattrib & AM_DIR)						// It is a directory
		{
			if(fileno==filectr || fileno==-1)
			{
				fprintf(file_pri,"\tErase directory %s... ",fno.fname);
				sprintf(buffer, "/%s", fno.fname);
				res = f_unlink(buffer);
				if(res==FR_OK)
					fprintf(file_pri,"Ok\n");
				else
					fprintf(file_pri,"Error\n");
			}
			else
				fprintf(file_pri,"\tSkipping directory %s\n",fno.fname);
		}
		else
		{
			if(fileno==filectr || fileno==-1)
			{
				fprintf(file_pri,"\tErase file %s... ",fno.fname);
				sprintf(buffer, "/%s", fno.fname);
				res = f_unlink(buffer);
				if(res==FR_OK)
					fprintf(file_pri,"Ok\n");
				else
					fprintf(file_pri,"Error\n");

			}
			else
				fprintf(file_pri,"\tSkipping file %s\n",fno.fname);
		}
		filectr++;
	}
	f_closedir(&dir);

	_ufat_unmount();
	return 0;
}

int _ufat_listfiles(char *path)
{
	FRESULT res;
	DIR dir;
	static FILINFO fno;

	res = f_opendir(&dir, path);                       // Open the directory
	if (res == FR_OK)
	{
		while(1)
		{
			res = f_readdir(&dir, &fno);                   // Read a directory item
			if (res != FR_OK || fno.fname[0] == 0)
				break;  									// Break on error or end of dir
			if (fno.fattrib & AM_DIR)						//It is a directory
			{
				fprintf(file_pri,"\t          \t<%s>\n",fno.fname);
				/*UINT i = _ufat_listfiles(path);
				sprintf(&path[i], "/%s", fno.fname);
				res = _ufat_listfiles(path);                    // Enter the directory
				if (res != FR_OK) break;
				path[i] = 0;*/
			}
			else 											// It is a file
			{
				fprintf(file_pri,"\t%10lu\t%s/%s\n", fno.fsize,path,fno.fname);			// It is a file.
			}
		}
		f_closedir(&dir);
	}

	return res;

}

int ufat_listfiles()
{
	FRESULT res;
	if(_ufat_mountcheck())
		return 1;

	char buff[256];

	strcpy(buff, "/");
	res = _ufat_listfiles(buff);
	(void) res;

	_ufat_unmount();

	return 0;
}

unsigned char _ufat_mountcheck()
{
	if(_ufat_mount())
	{
		fprintf(file_pri,"Cannot mount filesystem\n");
		return 1;
	}
	return 0;
}
unsigned char ufat_initcheck()
{
	if(ufat_init())
	{
		fprintf(file_pri,"Cannot init filesystem\n");
		return 1;
	}
	return 0;
}
/*
	Call this from an interrupt to notify SD card insertion has changed
 */
void ufat_notifysdcardchange(unsigned char sddetect)
{
	// Regardless of new SD status, flag as "unmounted"
	_fsinfo.fs_available=0;
	_fsinfo.card_isinitialised=0;
}

// Try to mount and say if FS available
unsigned char ufat_available(void)
{
	if(_ufat_mountcheck())
		return 0;
	_ufat_unmount();
	return 1;
}
/******************************************************************************
	function: _ufat_log_fputbuf
*******************************************************************************
	Internally used to write data to logs when fputbuf is called.
	Do not call directly.

	Parameters:
		buffer		-		Buffer containing the data
		size 		-		Size of buffer
	Returns:
		0			-		Success
		EOF			-		Error
******************************************************************************/
unsigned char _ufat_log_fputbuf(SERIALPARAM *sp,char *buffer,unsigned short size)
{
	// Check if space to write to file
	/*if(_log_current_size+size>_fsinfo.logsizebytes)
	{
		return EOF;
	}

	unsigned char rv = sd_streamcache_write(buffer,size,0);
	_log_current_size+=size;
	if(rv!=0)
	{
		printf("Writing block to sector %lu failed\n",_log_current_sector);
		return EOF;
	}*/



	FILE *file = (FILE*) sp->device;

	if(fwrite(buffer,1,size,file) == size)
		return 0;
	return EOF;


}
unsigned long long ufat_log_getmaxsize(void)
{
	return _fsinfo.logsizebytes;
}


/******************************************************************************
	function: ufat_log_getnumlogs
*******************************************************************************
	Returns the number of logs available.

	If the filesystem is not to be initialised and a log file to be
	open to return a valid result.

	Returns:
		Number of logs available, or 0 if the filesystem is not available.
******************************************************************************/
unsigned char ufat_log_getnumlogs(void)
{
	if(_fsinfo.fs_available==0)
	{
		#ifdef UFATDBG
			printf("fs not available\n");
		#endif


		//fprintf(file_pri,"ufat_log_getnumlogs\n");
		if(ufat_init())
			return 0;

		_ufat_unmount();
	}
	return _fsinfo.lognum;
}
