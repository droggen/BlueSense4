#ifndef __UICONFIG_H
#define __UICONFIG_H

// Maximum 


//extern PGM_P MenuSensorSrSetup[];

//void ui_configure(FILE *file);
void ui_configure_mpumode(FILE *file);
void ui_configure_streaming(FILE *file);
unsigned char LoadConfiguration(void);
void SaveConfiguration(void);
void PrintConfiguration(FILE *file);

void ConfigSaveStreamTimestamp(unsigned char timestamp);
unsigned char ConfigLoadStreamTimestamp(void);
void ConfigSaveStreamBattery(unsigned char battery);
unsigned char ConfigLoadStreamBattery(void);
void ConfigSaveStreamBinary(unsigned char binary);
unsigned char ConfigLoadStreamBinary(void);
void ConfigSaveStreamPktCtr(unsigned char pktctr);
unsigned char ConfigLoadStreamPktCtr(void);
void ConfigSaveStreamLabel(unsigned char label);
unsigned char ConfigLoadStreamLabel(void);
//void ConfigSaveADCMask(unsigned char mask);
//unsigned char ConfigLoadADCMask(void);
//void ConfigSaveADCPeriod(unsigned long period);
//unsigned long ConfigLoadADCPeriod(void);
void ConfigSaveEnableLCD(unsigned char lcden);
unsigned char ConfigLoadEnableLCD(void);
void ConfigSaveEnableInfo(unsigned char ien);
unsigned char ConfigLoadEnableInfo(void);
void ConfigSaveMotionMode(unsigned char mode);
unsigned char ConfigLoadMotionMode(void);
void ConfigSaveTSPeriod(unsigned long period);
unsigned long ConfigLoadTSPeriod(void);
void ConfigSaveMotionAccScale(unsigned char scale);
void ConfigSaveMotionGyroScale(unsigned char scale);
void ConfigLoadScript(char *buf);
void ConfigSaveScript(char *buf,unsigned char n);
unsigned char ConfigLoadVT100();
void ConfigSaveVT100(unsigned char en);

#endif
