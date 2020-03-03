#ifndef __RN41_H
#define __RN41_H

#include <stdio.h>
#include <string.h>


void rn41_CmdEnter(FILE *fileinfo,FILE *filebt);
void rn41_CmdLeave(FILE *fileinfo,FILE *filebt);
void rn41_CmdResp(FILE *fileinfo,FILE *filebt,const char *cmd);
void rn41_CmdRespn(FILE *fileinfo,FILE *filebt,const char *cmd,unsigned char n);
void rn41_Reset(FILE *fileinfo);
void rn41_PrintBasicParam(FILE *file,char *mac,char *name,char *baud,char mode,char auth,char *pin);
void rn41_PrintExtParam(FILE *file,unsigned short srvclass,unsigned short devclass, unsigned char cfgtimer,unsigned short inqw,unsigned short pagw);
unsigned char rn41_SetConfigTimer(FILE *fileinfo,FILE *filebt,unsigned char v);
unsigned char rn41_SetPassData(FILE *fileinfo,FILE *filebt,unsigned char v);
unsigned char rn41_SetAuthentication(FILE *fileinfo,FILE *filebt,unsigned char a);
unsigned char rn41_SetMode(FILE *fileinfo,FILE *filebt,unsigned char a);
unsigned char rn41_SetPIN(FILE *fileinfo,FILE *filebt,const char *pin);
unsigned char rn41_SetTempBaudrate(FILE *fileinfo,FILE *filebt,const char *b);
unsigned char rn41_SetName(FILE *fileinfo,FILE *filebt,char *name);
unsigned char rn41_SetSerializedName(FILE *fileinfo,FILE *filebt,const char *name);
unsigned char rn41_SetServiceClass(FILE *fileinfo,FILE *filebt,unsigned short c);
unsigned char rn41_SetDeviceClass(FILE *fileinfo,FILE *filebt,unsigned short c);
unsigned char rn41_GetBasic(FILE *fileinfo,FILE *filebt);
unsigned char rn41_GetBasicParam(FILE *filebt,char *mac,char *name,char *baud,char *mode,char *auth,char *pin);
unsigned char rn41_GetExtended(FILE *fileinfo,FILE *filebt);
unsigned char rn41_GetExtendedParam(FILE *filebt,unsigned short *srvclass,unsigned short *devclass,unsigned char *cfgtimer,unsigned short *inqw,unsigned short *pagw);
void rn41_Setup(FILE *fileinfo,FILE *filebt,unsigned char *devname);


#endif
