#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"

#include "global.h"
#include "serial.h"
#include "rn41.h"
#include "pkt.h"
#include "wait.h"
//#include "init.h"
#include "uiconfig.h"
#include "helper.h"
#include "system.h"
#include "system-extra.h"
#include "mode.h"
#include "mode_sd.h"
#include "commandset.h"
#include "mode_global.h"
//#include "spi.h"
//#include "sd.h"
#include "ufat.h"
//#include "test_sd.h"
#include "helper.h"

#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "sdmmc.h"
#include "bsp_driver_sd.h"
#include "fatfs.h"
#include "sd/stm_sdio.h"
#include "ymodem.h"

/*
 * STM32F4
1-bit, falling, /255:
format:
	98 809ms
	98 805ms

1-bit, falling, /16: issues
1-bit, rising, /16: issues

1-bit, falling, /32: mostly ok
	17 180ms

1-bit, falling, /128: mostly ok (sometimes re-reading same sectors gives weird value; could be missing pull-up on data and clock lines).
	53 706ms

4-bit, rising, /255
	34 129

4-bit, rising, /16
	error

4-bit, rising, /32
	error

4-bit, rising, /64
	12 458ms

*/
/*

AVR divider: /2 = 5529600Hz

==== STM32L4 - READ ====

Read: 1,0,1000,16

/128 1w		- 	Throughput: 45KB/s
	G1
		0-1000		38KB/s
	G8
		0-1000		43KB/s
	G16
		0-1000		43Kb/S
/128 4w		- 	Throughput: 183KB/s
	G1
		0-1000		107KB/s
	G8
		0-1000		159KB/s
	G16
		0-1000		167KB/s

/2 1w		-	Throughput: 2929 KB/s
  	G1
  		0-1000		333KB/s
  	G8
  		0-1000		1025KB/s
  	G16
  		0-1000		1208KB/s
/2 4w		-	Error transfer data
/4 4w		-	Error transfer data
/8 4w		-		2929 KB/s
	G1
		0-1000		356KB/s
	G8
		0-1000		1356KB/s
	G16
		0-1000		1719KB/s

==== STM32L4 - WRITE ====

6,30000000,30001000,16

/128 1w		- 	Throughput: 45KB/s
	G1
		0-1000					31KB/s	31KB/s
		30000000-30001000		31KB/s
	G8
		0-1000					41KB/s	42KB/s
		30000000-30001000		42KB/s
	G16
		0-1000					43KB/s
		30000000-30001000		43KB/s	42KB/s
/64	1w		-
  	G16
		30000000-30001000		82KB/s	83KB/s

/32	1w		-
  	G16
		30000000-30001000		156KB/s	156KB/s	156KB/s	Errors

/16	1w		-
  	G16
		30000000-30001000		Errors	279KB/s	Errors	Errors	Errors			Falling edge:	280 Err Err

/8	1w		-
  	G16
		30000000-30001000		Errors

/128 4w		- 	Throughput: 183KB/s
	G1
		0-1000					66KB/s
		30000000-30001000		66KB/s
	G8
		0-1000					139KB/s
		30000000-30001000		143KB/s		142KB/s
	G16
		0-1000					156KB/s
		30000000-30001000		156KB/s

/64	4w		-
  	G16
		30000000-30001000		285KB/s 287KB/s			VV
/32	4w		-
  	G16
		30000000-30001000		Errors	Errors	Errors



/8 4w		-		2929 KB/s
	G1
		0-1000
		30000000-30001000
	G8
		0-1000
		30000000-30001000
	G16
		0-1000
		30000000-30001000

Format time				1-bit					4-bit
Divider		128			118538					54162
			8			31795 					27244
			2			27327 					26386


*/
const char help_sdinit[] = "Low-level SD card initialisation";
const char help_erase[] = "E,<sectorstart>,<sectorend>: erase all sectors or from [start;end]. If start and end are zero the entire flash is erased.";
const char help_write[] = "W,<sector>,<value>: Fills a sector with the given value (all data in decimal)";
/*const char help_stream[] PROGMEM="S,<sector>,<value>,<size>,<bsize>: Writes size bytes data in streaming mode with caching; sends data by blocks of size bsize";
*/
const char help_read[] = "R,<sector>: Reads a sector (sector number in decimal)";
const char help_volume[] = "Initialise volume";
const char help_formatfull[] = "F,<numlogfiles>: Format the card with numlogfiles (maximum numlogfiles=14)";
const char help_format[] = "f,<numlogfiles>: Erase all files and create numlogfiles (maximum numlogfiles=14)";
/*const char help_logtest[] PROGMEM="l,<lognum>,<sizekb>: QA test. Logs test data to <lognum> up to <sizekb> KB. Use to validate speed/consistency of SD card writes.";
//const char help_logtest2[] PROGMEM="L,<lognum>,<sizebytes>,<char>,<bsiz>: Writes to lognum sizebytes character char in bsiz blocks";
const char help_sdbench[] PROGMEM="B,<benchtype>";
const char help_sdbench2[] PROGMEM="b,<startsect>,<sizekb> stream cache write from startsect up to sizekb";
const char help_sdbench3[] PROGMEM="1,<startsect>,<sizekb>,<preerasekb> stream cache write from startsect up to sizekb, optional preerase kb";*/
const char help_sd_detect[] = "SD detection test";
const char help_sd_1[] = "tests";
const char help_sd_bench[]="B,<size>,<pkts> Benchmark FAT write (filesystem level) of <size> bytes in packets of <pkts> bytes (pkts<=256)";
const char help_sd_bench_ll[]="b,<secfrom>,<secto> Benchmark low-level sector write (sector level) from <secfrom> to <secto> inclusive";
const char help_sd_state[]="Card state";
const char help_sd_logread[]="r,<log> Reads log file";
const char help_sd_list[]="List files";

const COMMANDPARSER CommandParsersSD[] =
{ 
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{0,0,"---- Main functions ----"},
	{'F', CommandParserSDFormatFull,help_formatfull},
	{'f', CommandParserSDFormat,help_format},
	{'L', CommandParserSDList,help_sd_list},
	{'D', CommandParserSDDeleteAll,"D[,<fileno>] Delete all files or if fileno specified deletes the nth file of the root"},
	{'C', CommandParserSDCreate,"C,<fname>[,<size>,<ascii>[,<bs>] Creates fname and optionally resizes it to size writing ascii bytes in blocks of bs bytes"},
	{'c', CommandParserSDCat,"c,<fname>[,<maxsize>] Cats the content of fname up to maxsize"},
	{'Y', CommandParserSDYModemSendFile,"Y,<fname> Sends the file <fname> with YMODEM to the host"},
	{'i', CommandParserSDCardInfo,"Card information"},
	{'V', CommandParserSDVolume,help_volume},
	{'v', CommandParserSDVolumeInfo,"Volume information"},
	{0,0,"---- Log benchmark functions ----"},
	{'w', CommandParserSDLogWriteBenchmark,			"w,<log>,<size>,<pkts>[,<vbuf>] Benchmark file write with stdio of <size> bytes to logfile <log> in packets of <pkts> bytes (<pkts><=256)\n\t\tSet full buffering with buffer size <vbuf>"},
	{'r', CommandParserSDLogReadBenchmark,	"r,<log>,<pkts>[,<vbuf>] Benchmark file read with stdio from logfile <log> in packets of <pkts> bytes (<pkts><=256)\n\t\tSet full buffering with buffer size <vbuf>"},




	/*//{'e', CommandParserSDErase2,help_erase},
	//{'X', CommandParserSDErase3,help_erase},*/

	/*{'S', CommandParserSDStream,help_stream},*/




	/*{'L', CommandParserSDLogTest,help_logtest},
	//{'l', CommandParserSDLogTest2,help_logtest2},
	{'B', CommandParserSDBench,help_sdbench},
	{'b', CommandParserSDBench2,help_sdbench2},
	{'1', CommandParserSDBench_t1,help_sdbench3},
	{'2', CommandParserSDBench_t2,help_sdbench2},*/
	{0,0,"---- Sector benchmark functions ----"},
	{'1',CommandParserSDBenchReadSect,"1,<sectfrom>,<sectto>,<sectatatime> Benchmark read sector from sectfrom to sectto by groups of sectatatime (HAL_SD_ReadBlocks)."},
	//{'2',CommandParserSDBenchReadSectIT,"2,<sectfrom>,<sectto>,<sectatatime> Benchmark read sector from sectfrom to sectto by groups of sectatatime with IT (HAL_SD_ReadBlocks_IT)."},
	{'3',CommandParserSDBenchReadSectDMA,"3,<sectfrom>,<sectto>,<sectatatime> Benchmark read sector from sectfrom to sectto by groups of sectatatime with DMA (HAL_SD_ReadBlocks_DMA)."},

	{'6',CommandParserSDBenchWriteSect,"6,<sectfrom>,<sectto>,<sectatatime> Benchmark write sector from sectfrom to sectto by groups of sectatatime (HAL_SD_WriteBlocks)."},
	{'7',CommandParserSDBenchWriteSect2,"7,<sectfrom>,<sectto>,<sectatatime> Benchmark write sector from sectfrom to sectto by groups of sectatatime (HAL_SD_WriteBlocks_DMA)."},

	{'P',CommandParserSDWritePattern,"P,<sectfrom>,<sectto> Write sectors from sectfrom to sectto with identifiable pattern"},

	{0,0,"---- Developer functions ----"},
	{'I', CommandParserSDInitInterface,"I[,<divider>,<4ben>] Init SD interface and card with optinal divider 4-bit wide bus enable"},
	{'W', CommandParserSDWrite,help_write},
	{'R', CommandParserSDRead,help_read},
	{'T', CommandParserSDRead2,"read with dma"},
	{'E', CommandParserSDErase,help_erase},
	{'S', CommandParserSDState,help_sd_state},
	{'d', CommandParserSDDetectTest,help_sd_detect},


	//{'f', CommandParserSDTest1,help_sd_1},
	//{'G', CommandParserSDReg,help_sd_1},
	//{'1', CommandParserSDRegPUPDActive,help_sd_1},
	//{'2', CommandParserSDRegPUPDDeactive,help_sd_1},
	//{'B', CommandParserSDBench,help_sd_bench},

	{'M', CommandParserSDDMA1,"DMA test 1"},
	{'m', CommandParserSDDMA2,"DMA test 2"},
	{'n', CommandParserSDDMA3,"DMA test 3"},


	//{'3', CommandParserSDLogTest3,help_sd_1},
};
const unsigned char CommandParsersSDNum=sizeof(CommandParsersSD)/sizeof(COMMANDPARSER);

#if 0
unsigned char CommandParserSDErase(char *buffer,unsigned char size)
{
	// Parse arguments
	unsigned long addr1,addr2;
	unsigned char rv = ParseCommaGetLong(buffer,2,&addr1,&addr2);
	if(rv)
		return 2;
	
	if(addr1==0 && addr2==0)
	{
		addr2 = _fsinfo.card_capacity_sector-1;
	}
	printf_P(PSTR("Erase %lu-%lu\n"),addr1,addr2);
	
	if(sd_erase(addr1,addr2))
	{
		printf_P(PSTR("Erase error.\n"));
		return 1;
	}
	
	
	return 0;
}
#endif
unsigned char CommandParserSDErase(char *buffer,unsigned char size)
{
	// Parse arguments
	unsigned addr1,addr2;
	unsigned char rv = ParseCommaGetInt(buffer,2,&addr1,&addr2);
	if(rv)
		return 2;


	HAL_SD_CardInfoTypeDef ci;
	if(HAL_SD_GetCardInfo(&hsd1,&ci))
		fprintf(file_pri,"Error: card not initialised?\n");

	if(addr1==0 && addr2==0)
	{
		//addr2 = _fsinfo.card_capacity_sector-1;
		// Get the number of sector -1
		addr2 = ci.BlockNbr-1;
	}

	if(addr1>ci.BlockNbr-1 || addr2>ci.BlockNbr-1)
	{
		fprintf(file_pri,"Error: maximum sector is %lu\n",ci.BlockNbr-1);
		return 2;
	}

	fprintf(file_pri,"Erase %u-%u\n",addr1,addr2);

	if(sd_erase(addr1,addr2))
	{
		fprintf(file_pri,"Erase error.\n");
		return 1;
	}


	return 0;
}
/*unsigned char CommandParserSDErase2(char *buffer,unsigned char size)
{
	// Parse arguments
	unsigned long addr;
	unsigned char rv = ParseCommaGetLong(buffer,1,&addr);
	if(rv)
		return 2;
	
	printf("Erase end: %lu\n",addr);
	
	_sd_cmd33(addr);
	return 0;
}
unsigned char CommandParserSDErase3(char *buffer,unsigned char size)
{
	printf("try erase\n");
	//_sd_cmd38();
	_sd_cmd_38_wait();
	return 0;
}*/


#if 0
unsigned char CommandParserSDBench(char *buffer,unsigned char size)
{
	unsigned char rv;
	char *p1;
	rv = ParseComma((char*)buffer,1,&p1);
	if(rv)
		return 2;
		
	// Get sector
	unsigned int benchtype;
	if(sscanf(p1,"%u",&benchtype)!=1)
	{
		printf("failed benchtype\n");
		return 2;
	}	
	switch(benchtype)
	{
		case 0:
		default:
			printf("card capacity sector: %lu\n",_fsinfo.card_capacity_sector);
			test_sd_benchmarkwriteblock(_fsinfo.card_capacity_sector);
			break;
		case 1:
			test_sd_benchmarkwriteblockmulti(_fsinfo.card_capacity_sector);
			break;
		case 3:
			test_sd_benchmarkwriteblockmulti(_fsinfo.card_capacity_sector);
			break;
		case 2:
			test_sdmulti();
			break;
	}
	return 0;
}
unsigned char CommandParserSDBench2(char *buffer,unsigned char size)
{
	// Parse arguments
	unsigned long startaddr,sizekb,preerasekb;
	unsigned char rv = ParseCommaGetLong(buffer,3,&startaddr,&sizekb,&preerasekb);
	if(rv)
		return 2;
	
	printf("Stream cache write from %lu up to %luKB\n",startaddr,sizekb);
	
	sd_bench_streamcache_write2(startaddr,sizekb*1024l,preerasekb*2l);
	return 0;
}
unsigned char CommandParserSDBench_t1(char *buffer,unsigned char size)
{
	// Parse arguments
	unsigned long startaddr,sizekb,preerasekb;
	unsigned char rv = ParseCommaGetLong(buffer,3,&startaddr,&sizekb,&preerasekb);
	if(rv)
		return 2;
	
	printf("Stream write from %lu up to %luKB\n",startaddr,sizekb);
	
	// preerase in sectors=preerase*1024/512=preerase*2l
	sd_bench_stream_write2(startaddr,sizekb*1024l,preerasekb*2l);
	return 0;
}
unsigned char CommandParserSDBench_t2(char *buffer,unsigned char size)
{
	// Parse arguments
	unsigned long startaddr,sizekb;
	unsigned char rv = ParseCommaGetLong(buffer,2,&startaddr,&sizekb);
	if(rv)
		return 2;
	
	printf("Write from %lu up to %luKB\n",startaddr,sizekb);
	
	//sd_bench_write(startaddr,sizekb*1024l);
	sd_bench_write2(startaddr,sizekb*1024l);
	return 0;
}
#endif

unsigned char CommandParserSDFormatFull(char *buffer,unsigned char size)
{
	unsigned int numlog;
	if(ParseCommaGetInt((char*)buffer,1,&numlog))
		return 2;

	fprintf(file_pri,"Formatting with %u log file(s)\n",numlog);
	ufat_format(numlog);
	return 0;
}
unsigned char CommandParserSDFormat(char *buffer,unsigned char size)
{
	unsigned int numlog;
	if(ParseCommaGetInt((char*)buffer,1,&numlog))
		return 2;
		
	fprintf(file_pri,"Erase and prepare %u log file(s)\n",numlog);
	ufat_format_partial(numlog);
	return 0;
}

/*unsigned char CommandParserSDLogTest2(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned int lognum,ch,sz,bsiz;
	rv = ParseCommaGetInt((char*)buffer,4,&lognum,&sz,&ch,&bsiz);
	if(rv)
		return 2;
		
	ufat_log_test2(lognum,sz,ch,bsiz);
	return 0;
}*/
#if 0
unsigned char CommandParserSDLogTest(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned int lognum,sz;
	rv = ParseCommaGetInt((char*)buffer,2,&lognum,&sz);
	if(rv)
		return 2;
	
	printf("lognum: %u\n",lognum);
	printf("sz: %u KB\n",sz);
	ufat_log_test(lognum,(unsigned long)sz*1024l,65536);
	return 0;
}
#endif
#if 0
unsigned char CommandParserSDWrite(char *buffer,unsigned char size)
{
	unsigned char rv;
	char *p1,*p2;
	rv = ParseComma((char*)buffer,2,&p1,&p2);
	if(rv)
		return 2;
		
	// Get sector
	unsigned long sector;
	if(sscanf(p1,"%lu",&sector)!=1)
	{
		printf("failed sector\n");
		return 2;
	}	
	// Get data
	unsigned int data;
	if(sscanf(p2,"%u",&data)!=1)
	{
		printf("failed data\n");
		return 2;
	}
	
	printf("Writing %02hX at sector %lu\n",data,sector);
	char block[512];
	for(unsigned short i=0;i<512;i++)
		block[i]=data;
	rv = sd_block_write(sector,block);
	printf("Write result: %d\n",rv);
	
	return 0;
	
}
#endif
unsigned char CommandParserSDWrite(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sector,data;
	rv = ParseCommaGetInt(buffer,2,&sector,&data);
	if(rv)
		return 2;


	printf("Writing %02hX at sector %u\n",data,sector);
	char block[512];
	for(unsigned short i=0;i<512;i++)
		block[i]=data;


	//rv = sd_block_write(sector,block);
	 rv = SD_write(0, block, sector, 1);			// Use the board support package of fatfs, which uses dma


	fprintf(file_pri,"Write result: %d\n",rv);




	return 0;

}
unsigned char CommandParserSDRead(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sector;
	rv = ParseCommaGetInt(buffer,1,&sector);
	if(rv)
		return 2;

	// Get sector
	HAL_SD_CardInfoTypeDef ci;
	if(HAL_SD_GetCardInfo(&hsd1,&ci))
		fprintf(file_pri,"Error: card not initialised?\n");

	if(sector>ci.BlockNbr-1)
	{
		fprintf(file_pri,"Error: maximum sector is %lu\n",ci.BlockNbr-1);
		return 2;
	}

	fprintf(file_pri,"Read sector %u\n",sector);
	char block[512];
	memset(block,0x55,512);
	rv = sd_block_read(sector,block);
	fprintf(file_pri,"Read result: %d\n",rv);
	print_bin(file_pri,block,512);

	return 0;

}
unsigned char CommandParserSDRead2(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sector;
	rv = ParseCommaGetInt(buffer,1,&sector);
	if(rv)
		return 2;


	/*BSP_SD_Init();

	// Get sector
	HAL_SD_CardInfoTypeDef ci;
	if(HAL_SD_GetCardInfo(&hsd1,&ci))
		fprintf(file_pri,"Error: card not initialised?\n");

	if(sector>ci.BlockNbr-1)
	{
		fprintf(file_pri,"Error: maximum sector is %lu\n",ci.BlockNbr-1);
		return 2;
	}*/

	fprintf(file_pri,"Read sector %u\n",sector);
	char _block[512+32];
	char *block=_block;
	fprintf(file_pri,"Address: %08X\n",block);
	block+=32;
	block=(char*)(((unsigned int)block)&0xfffffffC);
	fprintf(file_pri,"Address2: %08X\n",block);
	memset(block,0x55,512);


		//HAL_StatusTypeDef s = HAL_SD_ReadBlocks(&hsd1,block,sector,1,MMC_TIMEOUT_READWRITE);
		//HAL_StatusTypeDef s = HAL_SD_ReadBlocks_DMA(&hsd1, block, sector, 1);

		DRESULT r = SD_read(0, block,sector, 1);

		//fprintf(file_pri,"readblock: %d\n",s);
		fprintf(file_pri,"sd_read: %d\n",r);
		/*if(s)
		{
			fprintf(file_pri,"HAL_SD_ReadBlocks error\n");
		}
		else
		{*/

			/*HAL_SD_CardStateTypeDef s2;
			s2 = HAL_SD_GetCardState(&hsd1);

			fprintf(file_pri,"card state: %d\n",s2);*/
			//fprintf(file_pri,"s: %d s2: %d. buffer[0]: %02X\n",s,s2,blck[0]);

		//	HAL_Delay(100);
		//	s2 = HAL_SD_GetCardState(&hsd1);

		//	fprintf(file_pri,"\ts: %d s2: %d\n",s,s2);


//		}



	print_bin(file_pri,block,512);

	return 0;

}

#if 0
unsigned char CommandParserSDStream(char *buffer,unsigned char size)
{
	unsigned char rv;
	char *p1,*p2,*p3,*p4;
	rv = ParseComma((char*)buffer,4,&p1,&p2,&p3,&p4);
	if(rv)
		return 2;
		
	// Get sector
	unsigned long sector;
	if(sscanf(p1,"%lu",&sector)!=1)
	{
		printf("sect err\n");
		return 2;
	}	
	// Get data
	unsigned int data;
	if(sscanf(p2,"%u",&data)!=1)
	{
		return 2;
	}
	// Get size
	unsigned int len;
	if(sscanf(p3,"%u",&len)!=1)
	{
		return 2;
	}
	// Get size
	unsigned int bsize;
	if(sscanf(p4,"%u",&bsize)!=1)
	{
		return 2;
	}
	if(bsize>512)
		return 2;
	
	printf("Streaming %u bytes %02hX at sector %lu in blocks of %u bytes\n",len,data,sector,bsize);
	char block[512];
	unsigned long curraddr;
	for(unsigned short i=0;i<512;i++)
		block[i]=data;
	sd_stream_open(sector,0);
	while(len)
	{
		unsigned int effw=bsize;
		if(len<effw)
			effw=len;
		
		rv = sd_streamcache_write(block,effw,&curraddr);
		printf("Write %d. Return: %d. Current address: %lu\n",effw,rv,curraddr);
		len-=effw;
	}
	rv=sd_streamcache_close(&curraddr);
	printf("Close. Return: %d. Current address: %lu\n",rv,curraddr);
	
	return 0;
	
}
#endif
unsigned char CommandParserSDVolume(char *buffer,unsigned char size)
{	
	ufat_init();
	
	return 0;
	
}
#if 0
unsigned char CommandParserSDInit(char *buffer,unsigned char size)
{
	unsigned char rv;
	CID cid;
	CSD csd;
	SDSTAT sdstat;
	unsigned long capacity_sector;	
	
	printf_P(PSTR("Init SD\r"));
		
	rv = sd_init(&cid,&csd,&sdstat,&capacity_sector);
	if(rv!=0)
	{
		printf_P(PSTR("Init SD failure (%d)\r"),rv);
		return 1;
	}
	_fsinfo.card_capacity_sector=capacity_sector;
	printf_P(PSTR("Init SD success (%d)\r"),rv);
	sd_print_cid(file_pri,&cid);
	sd_print_csd(file_pri,&csd);	
	sd_print_sdstat(file_pri,&sdstat);	
	printf_P(PSTR("Card capacity: %ld sectors\n"),capacity_sector);
	
	// Check that we deal with an SDHC card with block length of 512 bytes
	if(csd.CSD!=1 || csd.READ_BL_LEN!=9 || csd.WRITE_BL_LEN!=9)
	{
		printf("SD card unsuitable\n");
		return 1;
	}
	
	
	return 0;
}
#endif
/*unsigned char CommandParserSDInit(char *buffer,unsigned char size)
{
	stm_sdm_init();
	stm_sdm_printcardinfo();



	return 0;
}*/
unsigned char CommandParserSD(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_SD);
	return 0;
}

unsigned char CommandParserSDYModemSendLog(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	unsigned log[5];
	char filenames[5][64];			// Space to store names
	char *filenameptr[5]={filenames[0],filenames[1],filenames[2],filenames[3],filenames[4]};			// The ymodem function needs an array of pointers to the file names


	unsigned np = ParseCommaGetNumParam(buffer);

	if(np==0)
		return 2;
	if(np>5)
	{
		fprintf(file_pri,"Maximum 5 log files can be specified\n");
		return 1;
	}
	ParseCommaGetUnsigned(buffer,np,&log[0],&log[1],&log[2],&log[3],&log[4]);

	// Check the log file number on the SD card
	unsigned char nl = ufat_log_getnumlogs();

	//fprintf(file_pri,"Number of logs: %u\n",nl);
	fprintf(file_pri,"Logs selected for transfer: ");
	for(int i=0;i<np;i++)
		fprintf(file_pri,"%u ",log[i]);
	fprintf(file_pri,"\n");

	// Check that all the logs are within the available range
	for(int i=0;i<np;i++)
	{
		if(log[i]>=nl)
		{
			fprintf(file_pri,"Invalid log files ID specified: the filesystem contains %u logs\n",nl);
			return 1;
		}
	}

	// All the logs are ok: get the filename
	for(int i=0;i<np;i++)
	{
		_ufat_lognumtoname(log[i],(unsigned char*)filenames[i]);
		fprintf(file_pri,"\t%u: %s\n",log[i],filenames[i]);
	}

	// Initiate the transfer
	unsigned char rv = ymodem_send_multi_file(np,filenameptr,file_pri);

	if(rv)
	{
		fprintf(file_pri,"\nTransfer error\n");
		return 1;
	}


	return 0;
}


unsigned char CommandParserSDDetectTest(char *buffer,unsigned char size)
{
	fprintf(file_pri,"fat test\n");

	//DSTATUS  s = disk_initialize();

	while(1)
	{
		unsigned char s = system_issdpresent();

		//fprintf(file_pri,"SD: %d (%d)\n",s,BSP_PlatformIsDetected());
		fprintf(file_pri,"SD: %d\n",s);

		if(fgetc(file_pri)!=-1)
			break;
		HAL_Delay(100);
	}





	return 0;
}
#if 0
//BYTE work[FF_MAX_SS]; // Work area (larger is better for processing time)
BYTE work[1000]; // Work area (larger is better for processing time)
unsigned char CommandParserSDTest1(char *buffer,unsigned char size)
{
	FATFS fs;           // Filesystem object
	FIL fil;            // File object
	FRESULT res;        // API result code
	UINT bw;            // Bytes written

	unsigned t1,t2;

	fprintf(file_pri,"Calling mkfs\n");
	// Create FAT volume */
	t1=timer_ms_get();
	res = f_mkfs("", FM_ANY, 0, work, sizeof work);
	t2=timer_ms_get();
	fprintf(file_pri,"mkfs time: %u ms\n",t2-t1);
	if(res)
	{
		fprintf(file_pri,"mkfs error\n");
		return 1;
	}
	fprintf(file_pri,"mkfs done\n");

	// Register work area
	/*f_mount(&fs, "", 0);

	// Create a file as new
	res = f_open(&fil, "hello.txt", FA_CREATE_NEW | FA_WRITE);
	if (res)
	{
		fprintf(file_pri,"fopen error\n");
		return 1;
	}

	// Write a message
	f_write(&fil, "Hello, World!\r\n", 15, &bw);
	if (bw != 15)
	{
	fprintf(file_pri,"fwrite error\n");
	return 1;
	}

	// Close the file
	f_close(&fil);

	// Unregister work area
	f_mount(0, "", 0);
*/

	/*FATFS mynewdiskFatFs; // File system object for User logical drive
	FIL MyFile; // File object
	char mynewdiskPath[4]; // User logical drive path

	if(FATFS_LinkDriver(&mynewdisk_Driver, mynewdiskPath) == 0)
	{
	if(f_mount(&mynewdiskFatFs, (TCHAR const*)mynewdiskPath, 0) == FR_OK)
	{
	if(f_open(&MyFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
	{
	if(f_write(&MyFile, wtext, sizeof(wtext), (void *)&wbytes) == FR_OK);
	{
	f_close(&MyFile);
	}
	}
	}
	}
	FATFS_UnLinkDriver(mynewdiskPath);*/

	/*DSTATUS s = SD_initialize(0);
	fprintf(file_pri,"status: %d - %s\n",s,s?"error":"ok");

	unsigned char buf[520];
	memset(buf,0x55,520);

	DRESULT r = SD_read(0,buf,0,1);
	fprintf(file_pri,"read: %d\n",r);
	print_bin(file_pri,buf,520);*/

	/*
	if(BSP_SD_Init() == MSD_OK)
	{
		fprintf(file_pri,"BSP_SD_Init ok\n");
		//Stat = SD_CheckStatus(0);
	}
	else
		fprintf(file_pri,"BSP_SD_Init error\n");

	if (BSP_SD_IsDetected() != SD_PRESENT)
	{
		fprintf(file_pri,"BSP_SD_IsDetected not present\n");
	}
	fprintf(file_pri,"BSP_SD_IsDetected present\n");
	// HAL SD initialization
	uint8_t sd_state = MSD_OK;
	sd_state = HAL_SD_Init(&hsd);
	fprintf(file_pri,"sd_state: %d\n",sd_state);
*/
	SD_HandleTypeDef hsd;

	/*HAL_SD_MspInit(&hsd);

	hsd.Instance = SDIO;
	hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
	hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
	hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
	hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
	hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
	hsd.Init.ClockDiv = 64;
	if (HAL_SD_Init(&hsd) != HAL_OK)
	{
		fprintf(file_pri,"HAL_SD_Init error\n");
		return 1;
	}
	//if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK)
	if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_1B) != HAL_OK)
	{
		fprintf(file_pri,"HAL_SD_ConfigWideBusOperation error\n");
		return 1;
	}

*/

	//HAL_SD_Init();

	/*
	FATFS mynewdiskFatFs;
	FIL MyFile;
	char mynewdiskPath[4];
	char wtext="hello";
	uint32_t wbytes;

	if(f_mount(&mynewdiskFatFs, (TCHAR const*)mynewdiskPath, 0) == FR_OK)
	{
		if(f_open(&MyFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
		{
			//if(f_write(&MyFile, wtext, sizeof(wtext), (void *)&wbytes) == FR_OK);
			if(f_write(&MyFile, wtext, sizeof(wtext), (void *)&wbytes) == FR_OK);
			{
				fprintf(file_pri,"wbytes: %u\n",wbytes);
				f_close(&MyFile);
			}
			fprintf(file_pri,"f_write error\n");
		}
		else
			fprintf(file_pri,"f_open error\n");
	}
	else
		fprintf(file_pri,"Fmount error\n");
*/



	return 0;
}
#endif
#if 0
unsigned char CommandParserSDReg(char *buffer,unsigned char size)
{

	fprintf(file_pri,"PUPDR C: %08X\n",GPIOC->PUPDR);
	fprintf(file_pri,"PUPDR D: %08X\n",GPIOD->PUPDR);



	return 0;
}
#endif
unsigned char CommandParserSDRegPUPDActive(char *buffer,unsigned char size)
{
	//PUPDR C: 02550000
	//PUPDR D: 00000010

	GPIOC->PUPDR = 0x02550000;		// new
	GPIOD->PUPDR = 0x10;			// new
	return 0;
}
unsigned char CommandParserSDRegPUPDDeactive(char *buffer,unsigned char size)
{
	GPIOC->PUPDR = 0x00540000;
	GPIOD->PUPDR = 0x00000000;
	return 0;
}
#if 0
unsigned char CommandParserSDBench(char *buffer,unsigned char size)
{
	(void) size;

	FATFS fs;           // Filesystem object
	FIL fil;            // File object
	FRESULT res;        // API result code
	// For n
	unsigned t1,t2;
	unsigned it=0;
	unsigned tw=0;
	unsigned wr=0;
	unsigned maxs,pkts;
	unsigned tot=0;
	unsigned suc=0;
	char buf[257];



	if(ParseCommaGetInt(buffer,2,&maxs,&pkts))
		return 2;
	if(pkts>256 || pkts<=0)
	{
		fprintf(file_pri,"Packet size error\n");
		return 2;
	}

	for(unsigned i=0;i<256;i++)
		buf[i] = 'A'+i%26;
	buf[pkts]=0;


	// Open file
	FRESULT fr = f_mount(&fs, "", 0);
	if(fr != FR_OK)
	{
		fprintf(file_pri,"Cannot mount filesystem\n");
		return 1;
	}

	// Create a file as new
	res = f_open(&fil, "BENCH.TXT", FA_CREATE_ALWAYS | FA_WRITE);
	if(res)
	{
		fprintf(file_pri,"Cannot open file\n");
		return 1;
	}

	fprintf(file_pri,"Writing to BENCH.TXT up to %u bytes in packets of %u\n",maxs,pkts);

	t1 = timer_ms_get();
	while(tot<maxs)
	{
		UINT nw;
		FRESULT fr = f_write (&fil,buf,pkts,&nw);

		if(fr==FR_OK)
		{
			tot+=pkts;
			tw+=nw;
			suc++;
		}
		else
			break;

		it++;
	}
	t2 = timer_ms_get();


	unsigned long long kbps = (unsigned long long)tw*1000/(t2-t1)/1024;
	fprintf(file_pri,"Wrote %u bytes in %lu ms: %lu KB/s\n",tw,t2-t1,kbps);


	// Close the file
	f_close(&fil);

	// Unregister work area
	f_mount(0, "", 0);

	return 0;
}
#endif
unsigned char CommandParserSDState(char *buffer,unsigned char size)
{
	HAL_SD_CardStateTypeDef s2 = HAL_SD_GetCardState(&hsd1);

	fprintf(file_pri,"Card state: %lu\n",s2);

	return 0;
}
unsigned char CommandParserSDLogWriteBenchmark(char *buffer,unsigned char size)
{
	(void) size;
	unsigned logn,maxs,pkts,vbuf=0;
	char buf[257];


	if(ParseCommaGetNumParam(buffer)==3)
	{
		if(ParseCommaGetInt(buffer,3,&logn,&maxs,&pkts))
			return 2;
	}
	else
	{
		if(ParseCommaGetInt(buffer,4,&logn,&maxs,&pkts,&vbuf))
			return 2;
	}

	if(pkts>256 || pkts<=0)
	{
		fprintf(file_pri,"Packet size error\n");
		return 2;
	}

	fprintf(file_pri,"Writing to log %d up to %u byes in packets of %u",logn,maxs,pkts);
	if(vbuf)
		fprintf(file_pri," with setvbuf %d",vbuf);
	else
		fprintf(file_pri," without setvbuf");

	fprintf(file_pri,"\n");


	for(unsigned i=0;i<256;i++)
		buf[i] = 'A'+i%26;
	buf[pkts]=0;

	FILE *file = ufat_log_open(logn);


	if(file==0)
	{
		fprintf(file_pri,"Error opening log %d\n",logn);
		return 1;
	}
	//fprintf(file_pri,"current log after opening: %p\n",_fsinfo.file_current_log);

	if(vbuf)
	{
		if(setvbuf(file, 0, _IOFBF, vbuf))
		{
			fprintf(file_pri,"Error: setvbuf failed\n");
		}
	}
	fprintf(file_pri,"Writing %uB in packets of %uB\n",maxs,strlen(buf));

	unsigned tot=0;
	unsigned suc=0;
	unsigned t1,t2;
	t1 = timer_ms_get();
	while(tot<maxs)
	{
		int r = fputs(buf,file);
		//fprintf(file_pri,"fputs : %d\n",r);


		if(r>=0)
		{
			//fprintf(file_pri,"fputs ok tot=%u\n",tot);
			tot+=pkts;
			suc++;
		}
		else
		{
			//fprintf(file_pri,"fputs error\n");
			break;
		}

	}
	t2 = timer_ms_get();

	unsigned long kbps = (unsigned long long)tot*125/128/(t2-t1);
	fprintf(file_pri,"Wrote %u bytes in %u ms: %lu KB/s\n",tot,t2-t1,kbps);

	ufat_log_close();


	return 0;
}
/*unsigned char CommandParserSDLogRead(char *buffer,unsigned char size)
{
	(void) size;
	unsigned logn;


	if(ParseCommaGetInt(buffer,1,&logn))
		return 2;

	fprintf(file_pri,"Reading logfile %d\n",logn);


	FILE *file = ufat_log_open_read(logn);


	if(file==0)
	{
		fprintf(file_pri,"Error opening log %d\n",logn);
		return 1;
	}

	fprintf(file_pri,"Log file size: %llu\n",ufat_log_getsize());

	unsigned nr=0;
	while(1)
	{
		int c = fgetc(file);
		if(c==-1)
			break;

		fprintf(file_pri,"%c",c);
		if(!((nr+1)%128))
			fprintf(file_pri,"\n");
	}
	fprintf(file_pri,"\n");

	ufat_log_close();



	return 0;
}*/

unsigned char CommandParserSDLogReadBenchmark(char *buffer,unsigned char size)
{
	(void) size;
	unsigned logn,maxs,pkts,vbuf=0;
	char buf[257];


	if(ParseCommaGetNumParam(buffer)==2)
	{
		if(ParseCommaGetInt(buffer,2,&logn,&pkts))
			return 2;
	}
	else
	{
		if(ParseCommaGetInt(buffer,3,&logn,&pkts,&vbuf))
			return 2;
	}

	if(pkts>256 || pkts<=0)
	{
		fprintf(file_pri,"Packet size error\n");
		return 2;
	}

	fprintf(file_pri,"Reading from log %d in packets of %u",logn,pkts);
	if(vbuf)
		fprintf(file_pri," with setvbuf %d",vbuf);
	else
		fprintf(file_pri," without setvbuf");

	fprintf(file_pri,"\n");

	FILE *file = ufat_log_open_read(logn);
	if(file==0)
	{
		fprintf(file_pri,"Error opening log %d\n",logn);
		return 1;
	}



	maxs = ufat_log_getsize();

	fprintf(file_pri,"Log file size: %u\n",maxs);


	if(vbuf)
	{
		if(setvbuf(file, 0, _IOFBF, vbuf))
		{
			fprintf(file_pri,"Error: setvbuf failed\n");
		}
	}

	unsigned tot=0;
	unsigned suc=0;
	unsigned t1,t2;
	t1 = timer_ms_get();
	while(maxs)
	{
		unsigned n = maxs;
		if(n>pkts)
			n=pkts;

		//fprintf(file_pri,"maxs: %u. tot: %u. Reading %u: \n",maxs,tot,n);

		unsigned nr = fread(buf,1,n,file);
		//fprintf(file_pri,"nr: %u (n=%u)\n",nr,n);

		if(nr == n)
		{
			//fprintf(file_pri,"new maxs: %u. new tot: %u\n",maxs,tot);
			tot+=pkts;
			maxs-=pkts;
			suc++;
		}
		else
		{

			break;
		}

	}
	t2 = timer_ms_get();

	unsigned long kbps = (unsigned long long)tot*125/128/(t2-t1);
	fprintf(file_pri,"Read %u bytes in %u ms: %lu KB/s\n",tot,t2-t1,kbps);

	ufat_log_close();






	return 0;
}

#if 0
unsigned char CommandParserSDLogTest3(char *buffer,unsigned char size)
{

	FIL fil;            // File object
	FRESULT res;        // API result code
	unsigned t1,t2;
	unsigned int numlogfile;
	if(ParseCommaGetInt((char*)buffer,1,&numlogfile))
		return 2;

	fprintf(file_pri,"Formatting with %u log files\n",numlogfile);

	// Sanity check of parameter value
	if(numlogfile<1)
		numlogfile=1;
	if(numlogfile>_UFAT_NUMLOGENTRY)
		numlogfile=_UFAT_NUMLOGENTRY;







	// Unmount
	fprintf(file_pri,"Unmounting filesystem\n");
	_ufat_unmount();

	fprintf(file_pri,"Formatting...\n");
	// Create FAT volume */
	t1=timer_ms_get();
	BYTE __fs_work[1000]; // Work area (larger is better for processing time)
	res = f_mkfs("", FM_ANY, 0, __fs_work, sizeof __fs_work);
	t2=timer_ms_get();
	fprintf(file_pri,"Format time: %u ms\n",t2-t1);
	if(res)
	{
		fprintf(file_pri,"Format error (%d)\n",res);
		return 1;
	}
	fprintf(file_pri,"Format done\n");


	// Mount the filesystem
	if(_ufat_mount())
	{
		fprintf(file_pri,"Cannot mount filesystem\n");
		return 1;
	}

	DWORD fre_clust;
	FATFS *fs;
	res = f_getfree("",&fre_clust, &fs);

	fprintf(file_pri,"getfree: %d %d\n",res,fre_clust);
	DWORD tot_sect = (fs->n_fatent - 2) * fs->csize;
	DWORD fre_sect = fre_clust * fs->csize;

	// Print the free space (assuming 512 bytes/sector)
	fprintf(file_pri,"%10lu KiB total drive space.\n%10lu KiB available.\n", tot_sect / 2, fre_sect / 2);

	// Create empty log files

	for(unsigned l=0;l<numlogfile;l++)
	{
		char fn[64];
		_ufat_lognumtoname(l,(unsigned char*)fn);


		// time
		//_logentries[i].time = _ufat_secfrommidnight_to_fattime(timer_s_get_frommidnight());

		// Create a file as new
		res = f_open(&fil, fn, FA_CREATE_NEW | FA_WRITE);
		if(res)
		{
			fprintf(file_pri,"Error creating log file %u (%s)\n",l,fn);
			return 1;
		}

		res = f_expand(&fil,10000000,1);
		if(res)
		{
			fprintf(file_pri,"Error expanding log file %u (%s)\n",l,fn);
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

	return 0;
}
#endif
unsigned char CommandParserSDBenchWriteSect(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sfrom,sto,sectatatime;
	const unsigned maxsectatatime=16;

	if(ParseCommaGetNumParam(buffer)==3)
	{
		rv = ParseCommaGetInt(buffer,3,&sfrom,&sto,&sectatatime);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
	}
	else
	{
		rv = ParseCommaGetInt(buffer,2,&sfrom,&sto);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
		sectatatime=1;
	}
	if(sectatatime>maxsectatatime)
	{
		fprintf(file_pri,"Maximum sector at a time is 16 (now: %d)\n",sectatatime);
		return 2;
	}

	if(sfrom>sto)
		return 2;


	fprintf(file_pri,"Benchmarking write from sector %u to %u by groups of %d\n",sfrom,sto,sectatatime);
	char block[maxsectatatime*512];
	for(unsigned short i=0;i<512;i+=2)
	{
		memset(block,0,maxsectatatime*512);
	}

	unsigned errcnt=0;
	unsigned t1,t2;
	t1 = timer_ms_get();
	for(unsigned s = sfrom;s<=sto;s+=sectatatime)
	{
		block[0]='S';
		block[1]=s>>8;
		block[2]=s&0xff;
		block[509]='S';
		block[510]=s>>8;
		block[511]=s&0xff;
		block[3] = block[508] = s%10;

		rv = sd_block_write_n(s,block,sectatatime);
		//rv = sd_block_write(s,block);
		if(rv)
			errcnt++;
	}
	t2 = timer_ms_get();
	unsigned ns = sto-sfrom+1;
	fprintf(file_pri,"Write complete in %u ms. Errors: %u. Write speed: %u sector/second; %u KB/s\n",t2-t1,errcnt,ns*1000/(t2-t1),ns*1000/(t2-t1)*512/1024);

	return 0;
}
unsigned char CommandParserSDBenchWriteSect2(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sfrom,sto,sectatatime;
	const unsigned maxsectatatime=16;

	if(ParseCommaGetNumParam(buffer)==3)
	{
		rv = ParseCommaGetInt(buffer,3,&sfrom,&sto,&sectatatime);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
	}
	else
	{
		rv = ParseCommaGetInt(buffer,2,&sfrom,&sto);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
		sectatatime=1;
	}
	if(sectatatime>maxsectatatime)
	{
		fprintf(file_pri,"Maximum sector at a time is 16 (now: %d)\n",sectatatime);
		return 2;
	}

	if(sfrom>sto)
		return 2;


	fprintf(file_pri,"Benchmarking write from sector %u to %u by groups of %d\n",sfrom,sto,sectatatime);
	char block[maxsectatatime*512];
	for(unsigned short i=0;i<512;i+=2)
	{
		memset(block,0,maxsectatatime*512);
	}

	unsigned errcnt=0;
	unsigned t1,t2;
	t1 = timer_ms_get();
	for(unsigned s = sfrom;s<=sto;s+=sectatatime)
	{
		block[0]='S';
		block[1]=s>>8;
		block[2]=s&0xff;
		block[509]='S';
		block[510]=s>>8;
		block[511]=s&0xff;
		block[3] = block[508] = s%10;

		//rv = sd_block_write_n(s,block,sectatatime);
		//HAL_StatusTypeDef s = HAL_SD_WriteBlocks_DMA(&hsd1,block,s,sectatatime);
		rv = SD_write(0, block, s, sectatatime);

			//fprintf(file_pri,"write dma: %d\n");
		//rv = sd_block_write(s,block);
		if(rv)
			errcnt++;
	}
	t2 = timer_ms_get();
	unsigned ns = sto-sfrom+1;
	fprintf(file_pri,"Write complete in %u ms. Errors: %u. Write speed: %u sector/second; %u KB/s\n",t2-t1,errcnt,ns*1000/(t2-t1),ns*1000/(t2-t1)*512/1024);

	return 0;
}
unsigned char CommandParserSDBenchReadSect(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sfrom,sto,sectatatime;
	const unsigned maxsectatatime=16;

	if(ParseCommaGetNumParam(buffer)==3)
	{
		rv = ParseCommaGetInt(buffer,3,&sfrom,&sto,&sectatatime);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
	}
	else
	{
		rv = ParseCommaGetInt(buffer,2,&sfrom,&sto);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
		sectatatime=1;
	}
	if(sectatatime>maxsectatatime)
	{
		fprintf(file_pri,"Maximum sector at a time is 16 (now: %d)\n",sectatatime);
		return 2;
	}

	if(sfrom>sto)
		return 2;


	fprintf(file_pri,"Benchmarking read from sector %u to %u by groups of %d\n",sfrom,sto,sectatatime);
	char block[maxsectatatime*512];

	unsigned errcnt=0;
	unsigned t1,t2;
	t1 = timer_ms_get();
	for(unsigned s = sfrom;s<=sto;s+=sectatatime)
	{
		//rv = sd_block_read(s,block);
		rv = sd_block_read_n(s,block,sectatatime);
		if(rv)
			errcnt++;
	}
	t2 = timer_ms_get();
	unsigned ns = sto-sfrom+1;
	fprintf(file_pri,"Read complete in %u ms. Sector errors: %u. Read speed: %u sector/second; %u KB/s\n",t2-t1,errcnt,ns*1000/(t2-t1),ns*1000/(t2-t1)*512/1024);

	return 0;
}
unsigned char CommandParserSDBenchReadSectIT(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sfrom,sto,sectatatime;
	const unsigned maxsectatatime=16;

	if(ParseCommaGetNumParam(buffer)==3)
	{
		rv = ParseCommaGetInt(buffer,3,&sfrom,&sto,&sectatatime);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
	}
	else
	{
		rv = ParseCommaGetInt(buffer,2,&sfrom,&sto);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
		sectatatime=1;
	}
	if(sectatatime>maxsectatatime)
	{
		fprintf(file_pri,"Maximum sector at a time is 16 (now: %d)\n",sectatatime);
		return 2;
	}

	if(sfrom>sto)
		return 2;


	fprintf(file_pri,"Benchmarking read from sector %u to %u by groups of %d\n",sfrom,sto,sectatatime);
	char block[maxsectatatime*512];

	unsigned errcnt=0;
	unsigned t1,t2;
	t1 = timer_ms_get();
	for(unsigned s = sfrom;s<=sto;s+=sectatatime)
	{
		//rv = sd_block_read(s,block);
		rv = sd_block_read_n_it(s,block,sectatatime);
		if(rv)
			errcnt++;
	}
	t2 = timer_ms_get();
	unsigned ns = sto-sfrom+1;
	fprintf(file_pri,"Read complete in %u ms. Sector errors: %u. Read speed: %u sector/second; %u KB/s\n",t2-t1,errcnt,ns*1000/(t2-t1),ns*1000/(t2-t1)*512/1024);

	return 0;
}
unsigned char CommandParserSDBenchReadSectDMA(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sfrom,sto,sectatatime;
	const unsigned maxsectatatime=16;

	if(ParseCommaGetNumParam(buffer)==3)
	{
		rv = ParseCommaGetInt(buffer,3,&sfrom,&sto,&sectatatime);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
	}
	else
	{
		rv = ParseCommaGetInt(buffer,2,&sfrom,&sto);
		if(rv)
		{
			fprintf(file_pri,"Invalid arguments\n");
			return 2;
		}
		sectatatime=1;
	}
	if(sectatatime>maxsectatatime)
	{
		fprintf(file_pri,"Maximum sector at a time is 16 (now: %d)\n",sectatatime);
		return 2;
	}

	if(sfrom>sto)
		return 2;


	fprintf(file_pri,"Benchmarking read from sector %u to %u by groups of %d\n",sfrom,sto,sectatatime);
	char block[maxsectatatime*512];

	unsigned errcnt=0;
	unsigned t1,t2;
	t1 = timer_ms_get();
	for(unsigned s = sfrom;s<=sto;s+=sectatatime)
	{
		//rv = sd_block_read(s,block);
		//rv = sd_block_read_n_dma(s,block,sectatatime);
		rv = SD_read(0,block,s,sectatatime);
		if(rv)
			errcnt++;
	}
	t2 = timer_ms_get();
	unsigned ns = sto-sfrom+1;
	fprintf(file_pri,"Read complete in %u ms. Sector errors: %u. Read speed: %u sector/second; %u KB/s\n",t2-t1,errcnt,ns*1000/(t2-t1),ns*1000/(t2-t1)*512/1024);

	return 0;
}

unsigned char CommandParserSDWritePattern(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned sfrom,sto;

	rv = ParseCommaGetInt(buffer,2,&sfrom,&sto);
	if(rv)
	{
		fprintf(file_pri,"Invalid arguments\n");
		return 2;
	}

	if(sfrom>sto)
		return 2;


	fprintf(file_pri,"Writing ID pattern from sector %u to %u\n",sfrom,sto);
	char block[512];


	unsigned errcnt=0;
	unsigned t1,t2;
	t1 = timer_ms_get();
	for(unsigned s = sfrom;s<=sto;s++)
	{
		memset(block,s%10,512);
		block[0]='S';
		block[1]=s>>8;
		block[2]=s&0xff;
		block[509]='S';
		block[510]=s>>8;
		block[511]=s&0xff;

		rv = sd_block_write(s,block);
		if(rv)
			errcnt++;
	}
	t2 = timer_ms_get();
	unsigned ns = sto-sfrom+1;
	fprintf(file_pri,"Write complete in %u ms. Sector errors: %u. Write speed: %u sector/second; %u KB/s\n",t2-t1,errcnt,ns*1000/(t2-t1),ns*1000/(t2-t1)*512/1024);

	return 0;
}


unsigned char CommandParserSDList(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	ufat_listfiles();


	return 0;
}
unsigned char CommandParserSDInitInterface(char *buffer,unsigned char size)
{
	int divider,enwb4;

	if(ParseCommaGetNumParam(buffer)!=0)
	{
		unsigned char rv = ParseCommaGetInt(buffer,2,&divider,&enwb4);
		if(rv)
			return 2;
		stm_sdm_setinitparam(divider,enwb4);
	}

	stm_sdm_init();

/*#warning dma
	DSTATUS s = SD_initialize(0);
	fprintf(file_pri,"dstatus: %d\n",s);*/


	return 0;
}
unsigned char CommandParserSDCardInfo(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	stm_sdm_printcardinfo();

	return 0;
}

unsigned char CommandParserSDVolumeInfo(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	ufat_volumeinfo();

	return 0;
}

unsigned char CommandParserSDDeleteAll(char *buffer,unsigned char size)
{
	int fileno;

	if(ParseCommaGetNumParam(buffer)==1)
	{
		unsigned char rv = ParseCommaGetInt(buffer,1,&fileno);
		if(rv)
			return 2;
	}
	else
		if(ParseCommaGetNumParam(buffer)==0)
			fileno=-1;
		else
			return 2;


	ufat_erasefiles(fileno);

	return 0;
}
unsigned char CommandParserSDCreate(char *buffer,unsigned char size)
{
	int fsize,ascii;
	char fname[256];
	char *p1,*p2;
	unsigned maxbufsize=16384;
	unsigned bufsize;
	char buf[maxbufsize];

	//fprintf(file_pri,"buffer: '%s'\n",buffer);

	if(ParseCommaGetNumParam(buffer)==1)
	{
		// One parameter only (filename)
		if(ParseComma(buffer,1,&p1))
			return 2;
		strcpy(fname,p1);			// Copy the file
	}
	else
	{
		// Three parameters: filename, size, ascii, bs
		if(ParseComma(buffer,2,&p1,&p2))
			return 2;
		strcpy(fname,p1);			// Copy the file
		p2--;
		*p2=',';					// Remainder of string must start by ',' for ParseCommaGetInt

		//fprintf(file_pri,"p2: '%s'\n",p2);

		ParseCommaGetInt(p2,3,&fsize,&ascii,&bufsize);


	}

	fprintf(file_pri,"Creating '%s' with size %d",fname,fsize);
	if(fsize)
		fprintf(file_pri," filling with ascii %d",ascii);
	if(bufsize==0 || bufsize>maxbufsize) bufsize=maxbufsize;
	fprintf(file_pri," in blocks of %d bytes\n",bufsize);


	if(_ufat_mountcheck())
		return 1;



	FIL fil;            // File object
	UINT bw;            // Bytes written
	FRESULT res = f_open(&fil, fname, FA_CREATE_ALWAYS | FA_WRITE);
	if (res)
	{
		fprintf(file_pri,"Error creating file '%s'\n",fname);
		_ufat_unmount();
		return 1;
	}

	unsigned totwritten=0;
	unsigned long t1,t2;
	if(fsize)
	{
		memset(buf,ascii,bufsize);

		t1 = timer_ms_get();

		while(fsize)
		{
			int numwrite=fsize; if(numwrite>bufsize) numwrite = bufsize;


			f_write(&fil,buf,numwrite,&bw);
			if (bw != numwrite)
			{
				fprintf(file_pri,"Write error\n");
				f_close(&fil);
				_ufat_unmount();
				return 1;
			}
			totwritten+=bw;

			fsize-=bw;
		}

		t2 = timer_ms_get();
		unsigned long long kbps = (unsigned long long)totwritten*1000/(t2-t1)/1024;
		fprintf(file_pri,"Wrote %u bytes in %lu ms: %llu KB/s\n",totwritten,t2-t1,kbps);
	}

	// Close the file
	f_close(&fil);
	_ufat_unmount();




	return 0;



}

unsigned char CommandParserSDCat(char *buffer,unsigned char size)
{
	int fsize=0;
	char fname[256];
	char *p1,*p2;
	unsigned bufsize=128;
	char buf[bufsize];

	//fprintf(file_pri,"buffer: '%s'\n",buffer);

	if(ParseCommaGetNumParam(buffer)==1)
	{
		// One parameter only (filename)
		if(ParseComma(buffer,1,&p1))
			return 2;
		strcpy(fname,p1);			// Copy the file
	}
	else
	{
		// Two parameters: filename, size
		if(ParseComma(buffer,2,&p1,&p2))
			return 2;
		strcpy(fname,p1);			// Copy the file
		p2--;
		*p2=',';					// Remainder of string must start by ',' for ParseCommaGetInt

		//fprintf(file_pri,"p2: '%s'\n",p2);

		ParseCommaGetInt(p2,1,&fsize);


	}

	fprintf(file_pri,"Reading '%s'",fname);
	if(fsize)
		fprintf(file_pri," up to size %d\n",fsize);
	fprintf(file_pri,"\n");


	//if(_ufat_mountcheck())
	if(ufat_initcheck())
		return 1;



	FIL fil;            // File object
	UINT br;            // Bytes read
	FRESULT res = f_open(&fil, fname, FA_READ);
	if (res)
	{
		fprintf(file_pri,"Error opening file '%s'\n",fname);
		_ufat_unmount();
		return 1;
	}

	if(fsize==0)
	{
		fsize = f_size(&fil);
		fprintf(file_pri,"File size: %d\n",fsize);
	}

	unsigned totread=0;
	while(1)
	{

		int numread=bufsize;
		if(numread>fsize) numread = fsize;


		//fprintf(file_pri,"pre. totread: %d numread: %d br: %d fsize: %d\n",totread,numread,br,fsize);

		res = f_read(&fil, buf, numread, &br);
		if(res)
		{
			fprintf(file_pri,"Read error\n");
			f_close(&fil);
			_ufat_unmount();
			return 1;
		}

		if(br)
		{
			for(int i=0;i<br;i++)
			{
				if(buf[i]<32)
					fprintf(file_pri,"[%02X]",buf[i]);
				else
					fprintf(file_pri,"%c",buf[i]);
			}
			fprintf(file_pri,"\n");
		}

		totread += numread;
		if(br < numread)
			break;		// eof
		fsize-=numread;

		//fprintf(file_pri,"it. totread: %d numread: %d br: %d fsize: %d\n",totread,numread,br,fsize);

		if(fsize==0)
			break;
	}

	fprintf(file_pri,"Read %d bytes\n",totread);

	// Close the file
	f_close(&fil);


	_ufat_unmount();


	return 0;



}
unsigned char CommandParserSDYModemSendFile(char *buffer,unsigned char size)
{
	int fsize=0;
	char fname[256];
	char *fnameptr[1]={fname};			// Multiple files could be sent in succession; currently we handle only one.
	char *p1;


	// One parameter only (filename)
	if(ParseComma(buffer,1,&p1))
		return 2;
	strcpy(fname,p1);			// Copy the file



	fprintf(file_pri,"Sending '%s' with YMODEM\n",fname);

	// Initiate the transfer
	unsigned char rv = ymodem_send_multi_file(1,fnameptr,file_pri);

	if(rv)
	{
		fprintf(file_pri,"\nTransfer error\n");
		return 1;
	}


	return 0;
}


unsigned char CommandParserSDDMA1(char *buffer,unsigned char size)
{
	/* DMA for SDMMC1 is:
	DMA2 Channel 4
	DMA2 Channel 5

	After a write the following are changed:
		Addr 40020444: 00000A91
		Addr 40020448: 00000000
		Addr 4002044C: 40012880
		Addr 40020450: 2004FD80

	After a read the following are changed:
		Addr 40020444: 00000A90
		Addr 40020448: 00000000
		Addr 4002044C: 00000000
		Addr 40020450: 00000000
		Addr 40020458: 00000A81
		Addr 4002045C: 00000000
		Addr 40020460: 40012880
		Addr 40020464: 2004FD7C
	*/
	unsigned cserl = *(((unsigned *)DMA2_BASE)+(0xa8/4));
	fprintf(file_pri,"CSERL %08x\n",cserl);
	fprintf(file_pri,"Address of DMA2 %08x\n",DMA2);
	fprintf(file_pri,"Address of DMA2_BASE %08x\n",DMA2_BASE);
	fprintf(file_pri,"Address of IFCR %08x\n",&DMA2->IFCR);
	fprintf(file_pri,"Address of ISR %08x\n",&DMA2->ISR);

	for(int o=0;o<=0xA8/4;o++)
	{
		unsigned *adr = ((unsigned *)DMA2_BASE)+o;
		fprintf(file_pri,"Addr %08X: %08X: %08X\n",adr,o*4,*adr);
	}


	return 0;
}
unsigned char CommandParserSDDMA2(char *buffer,unsigned char size)
{
	//__HAL_LINKDMA(&hsd1,hdmarx,hdma_sdmmc1_rx);
	//__HAL_LINKDMA(&hsd1,hdmatx,hdma_sdmmc1_tx);
	unsigned rv1,rv2;

	//rv2 = HAL_DMA_DeInit(&hdma_sdmmc1_tx);
	//__HAL_DMA_DISABLE(&hdma_sdmmc1_tx);
	//rv1 = HAL_DMA_Init(&hdma_sdmmc1_rx);

	*((unsigned*)0x400204A8) = 0x00017000;		// Channel 4=7=SDMMC, channel 5=1=nothing (write to mmc)

	fprintf(file_pri,"%d %d\n",rv1,rv2);
	/*LL_DMA_DisableChannel(DMA2,LL_DMA_CHANNEL_4);
	LL_DMA_EnableChannel(DMA2,LL_DMA_CHANNEL_5);*/

	//LL_DMA_SetDataTransferDirection(DMA2,LL_DMA_CHANNEL_4,LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

	return 0;
}
unsigned char CommandParserSDDMA3(char *buffer,unsigned char size)
{
	//__HAL_LINKDMA(&hsd1,hdmarx,hdma_sdmmc1_rx);
	//__HAL_LINKDMA(&hsd1,hdmatx,hdma_sdmmc1_tx);

	unsigned rv1,rv2;

	//rv2 = HAL_DMA_DeInit(&hdma_sdmmc1_rx);
	//__HAL_DMA_DISABLE(&hdma_sdmmc1_rx);
	//rv1 = HAL_DMA_Init(&hdma_sdmmc1_tx);

	*((unsigned*)0x400204A8) = 0x00072000;		// Channel 5=7=SDMMC, Channel 4=2=nothing (read from mmc)

	fprintf(file_pri,"%d %d\n",rv1,rv2);

	/*LL_DMA_DisableChannel(DMA2,LL_DMA_CHANNEL_5);
	LL_DMA_EnableChannel(DMA2,LL_DMA_CHANNEL_4);*/

	//LL_DMA_SetDataTransferDirection(DMA2,LL_DMA_CHANNEL_4,LL_DMA_DIRECTION_MEMORY_TO_PERIPH);


	return 0;
}


void mode_sd(void)
{
	mode_run("SD",CommandParsersSD,CommandParsersSDNum);

}




