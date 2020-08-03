#ifndef __MODE_AUDIO_H
#define __MODE_AUDIO_H


#include "command.h"




void mode_audio(void);

unsigned char CommandParserAudTest1(char *buffer,unsigned char size);
unsigned char CommandParserAudPrintReg(char *buffer,unsigned char size);
unsigned char CommandParserAudTest2(char *buffer,unsigned char size);
unsigned char CommandParserAudTest3(char *buffer,unsigned char size);
unsigned char CommandParserAudTest4(char *buffer,unsigned char size);
unsigned char CommandParserAudTest5(char *buffer,unsigned char size);
unsigned char CommandParserAudTest6(char *buffer,unsigned char size);
unsigned char CommandParserAudTest7(char *buffer,unsigned char size);
unsigned char CommandParserAudInitStartDMA(char *buffer,unsigned char size);
unsigned char CommandParserAudInitOnlyDetailed(char *buffer,unsigned char size);
unsigned char CommandParserAudFilterInit(char *buffer,unsigned char size);
unsigned char CommandParserAudTest8(char *buffer,unsigned char size);
unsigned char CommandParserAudRec(char *buffer,unsigned char size);
unsigned char CommandParserAudRecPrint(char *buffer,unsigned char size);
unsigned char CommandParserAudTest9(char *buffer,unsigned char size);

unsigned char CommandParserAudPollStart(char *buffer,unsigned char size);
unsigned char CommandParserAudPollPrint(char *buffer,unsigned char size);
unsigned char CommandParserAudDMAStart(char *buffer,unsigned char size);
unsigned char CommandParserAudDMAPrint(char *buffer,unsigned char size);
unsigned char CommandParserAudDMAPrintStereo(char *buffer,unsigned char size);
unsigned char CommandParserAudDMAPrintStereoBin(char *buffer,unsigned char size);

unsigned char CommandParserAudStart(char *buffer,unsigned char size);
unsigned char CommandParserAudStop(char *buffer,unsigned char size);
//unsigned char CommandParserAudPoll2(char *buffer,unsigned char size);
//unsigned char CommandParserAudPoll3(char *buffer,unsigned char size);
unsigned char CommandParserAudPoll4(char *buffer,unsigned char size);
unsigned char CommandParserAudPoll5(char *buffer,unsigned char size);
unsigned char CommandParserAudReg(char *buffer,unsigned char size);
//unsigned char CommandParserAudPollExtStereo(char *buffer,unsigned char size);
unsigned char CommandParserAudOffsetAutoCalibrate(char *buffer,unsigned char size);
unsigned char CommandParserAudOffsetClear(char *buffer,unsigned char size);
unsigned char CommandParserAudOffsetInternalMeasure(char *buffer,unsigned char size);
unsigned char CommandParserAudOffsetSet(char *buffer,unsigned char size);
unsigned char CommandParserAudTest(char *buffer,unsigned char size);

unsigned char CommandParserAudLog(char *buffer,unsigned char size);
unsigned char CommandParserAudInitStartDMA(char *buffer,unsigned char size);
unsigned char CommandParserAudInitOnly(char *buffer,unsigned char size);
unsigned char CommandParserAudBench(char *buffer,unsigned char size);
//unsigned char CommandParserAudFFT(char *buffer,unsigned char size);
unsigned char CommandParserAudNothing(char *buffer,unsigned char size);
unsigned char CommandParserAudStatPrint(char *buffer,unsigned char size);
unsigned char CommandParserAudStatClear(char *buffer,unsigned char size);
unsigned char CommandParserAudStatus(char *buffer,unsigned char size);
unsigned char CommandParserAudRightshift(char *buffer,unsigned char size);

#endif

