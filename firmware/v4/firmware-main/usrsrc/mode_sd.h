#ifndef __MODE_SD_H
#define __MODE_SD_H

#include "command.h"

extern const char help_sdinit[];
extern const char help_sdbench[];
extern const char help_sd_detect[];

unsigned char CommandParserSD(char *buffer,unsigned char size);
unsigned char CommandParserSDInit(char *buffer,unsigned char size);
unsigned char CommandParserSDErase(char *buffer,unsigned char size);
unsigned char CommandParserSDErase2(char *buffer,unsigned char size);
unsigned char CommandParserSDErase3(char *buffer,unsigned char size);
unsigned char CommandParserSDWrite(char *buffer,unsigned char size);
unsigned char CommandParserSDRead(char *buffer,unsigned char size);
unsigned char CommandParserSDRead2(char *buffer,unsigned char size);
unsigned char CommandParserSDStream(char *buffer,unsigned char size);
unsigned char CommandParserSDVolume(char *buffer,unsigned char size);
unsigned char CommandParserSDFormatFull(char *buffer,unsigned char size);
unsigned char CommandParserSDFormat(char *buffer,unsigned char size);
unsigned char CommandParserSDLogTest(char *buffer,unsigned char size);
unsigned char CommandParserSDLogTest2(char *buffer,unsigned char size);
unsigned char CommandParserSDBench(char *buffer,unsigned char size);
unsigned char CommandParserSDBench2(char *buffer,unsigned char size);
unsigned char CommandParserSDBench_t1(char *buffer,unsigned char size);
unsigned char CommandParserSDBench_t2(char *buffer,unsigned char size);
unsigned char CommandParserSDDetectTest(char *buffer,unsigned char size);
unsigned char CommandParserSDYModemSendLog(char *buffer,unsigned char size);
unsigned char CommandParserSDTest1(char *buffer,unsigned char size);
unsigned char CommandParserSDReg(char *buffer,unsigned char size);
unsigned char CommandParserSDRegPUPDActive(char *buffer,unsigned char size);
unsigned char CommandParserSDRegPUPDDeactive(char *buffer,unsigned char size);
unsigned char CommandParserSDBench(char *buffer,unsigned char size);
unsigned char CommandParserSDState(char *buffer,unsigned char size);
unsigned char CommandParserSDLogWriteBenchmark(char *buffer,unsigned char size);
//unsigned char CommandParserSDLogRead(char *buffer,unsigned char size);
unsigned char CommandParserSDLogReadBenchmark(char *buffer,unsigned char size);
unsigned char CommandParserSDLogTest3(char *buffer,unsigned char size);
unsigned char CommandParserSDList(char *buffer,unsigned char size);
unsigned char CommandParserSDInitInterface(char *buffer,unsigned char size);
unsigned char CommandParserSDWritePattern(char *buffer,unsigned char size);

unsigned char CommandParserSDBenchWriteSect(char *buffer,unsigned char size);
unsigned char CommandParserSDBenchWriteSect2(char *buffer,unsigned char size);

unsigned char CommandParserSDBenchReadSect(char *buffer,unsigned char size);
unsigned char CommandParserSDBenchReadSectIT(char *buffer,unsigned char size);
unsigned char CommandParserSDBenchReadSectDMA(char *buffer,unsigned char size);

unsigned char CommandParserSDCardInfo(char *buffer,unsigned char size);
unsigned char CommandParserSDVolumeInfo(char *buffer,unsigned char size);
unsigned char CommandParserSDDeleteAll(char *buffer,unsigned char size);
unsigned char CommandParserSDCreate(char *buffer,unsigned char size);
unsigned char CommandParserSDCat(char *buffer,unsigned char size);
unsigned char CommandParserSDYModemSendFile(char *buffer,unsigned char size);

unsigned char CommandParserSDDMA1(char *buffer,unsigned char size);
unsigned char CommandParserSDDMA2(char *buffer,unsigned char size);
unsigned char CommandParserSDDMA3(char *buffer,unsigned char size);

void mode_sd(void);


#endif
