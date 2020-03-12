#ifndef __COMMANDSET_H
#define __COMMANDSET_H

#include "command.h"


extern const char help_demo[];
extern const char help_z[];
//extern const char help_zsyncfromrtc[];
extern const char help_y[];
extern const char help_w[];
extern const char help_b[];
extern const char help_r[];
extern const char help_i[];
extern const char help_c[];
extern const char help_l[];
extern const char help_t[];
extern const char help_timetest[];
extern const char help_ttest[];
extern const char help_d[];
extern const char help_quit[];
extern const char help_h[];
extern const char help_a[];
extern const char help_a_test[];
extern const char help_f[];
extern const char help_M[];
extern const char help_m[];
extern const char help_g[];
extern const char help_O[];
extern const char help_o[];
extern const char help_sd[];
extern const char help_coulomb[];
extern const char help_s[];
extern const char help_identify[];
extern const char help_annotation[];
extern const char help_bootscript[];
extern const char help_info[];
extern const char help_batterylong[];
extern const char help_battery[];
extern const char help_powertest[];
extern const char help_callback[];
extern const char help_clearbootctr[];
extern const char help_eeprom[];
//extern const char help_benchmark[];
extern const char help_rtc[];
extern const char help_usbreinit[];
extern const char help_interrupts[];
extern const char help_i2c[];
extern const char help_interface[];
extern const char help_benchmark[];
extern const char help_fat[];
extern const char help_sleep[];
extern const char help_cpureg[];
extern const char help_audio[];
extern const char help_benchmark_cpu[];

extern const char help_1[];

extern const COMMANDPARSER CommandParsersDefault[];
extern const unsigned char CommandParsersDefaultNum;
extern unsigned char __CommandQuit;

extern unsigned CurrentAnnotation;



unsigned char CommandParserTime(char *buffer,unsigned char size);
unsigned char CommandParserDate(char *buffer,unsigned char size);
unsigned char CommandParserTime_Test(char *buffer,unsigned char size);
unsigned char CommandParserQuit(char *buffer,unsigned char size);
unsigned char CommandParserSuccess(char *buffer,unsigned char size);
unsigned char CommandParserError(char *buffer,unsigned char size);
unsigned char CommandParserHelp(char *buffer,unsigned char size);
unsigned char CommandParserLCD(char *buffer,unsigned char size);
unsigned char CommandParserIO(char *buffer,unsigned char size);
unsigned char CommandParserSwap(char *buffer,unsigned char size);
unsigned char CommandParserOff(char *buffer,unsigned char size);
unsigned char CommandParserOffPower(char *buffer,unsigned char size);
unsigned char CommandParserOffStore(char *buffer,unsigned char size);
unsigned char CommandParserStreamFormat(char *buffer,unsigned char size);
unsigned char CommandParserPowerTest(char *buffer,unsigned char size);
unsigned char CommandParserSync(char *buffer,unsigned char size);
//unsigned char CommandParserSyncFromRTC(char  *buffer,unsigned char size);
unsigned char CommandParserTestSync(char *buffer,unsigned char size);
unsigned char CommandParserIdentify(char *buffer,unsigned char size);
unsigned char CommandParserAnnotation(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestN(char *buffer,unsigned char size);
unsigned char CommandParserBootScript(char *buffer,unsigned char size);
unsigned char CommandParserInfo(char *buffer,unsigned char size);
unsigned char CommandParserBatteryInfoLong(char *buffer,unsigned char size);
unsigned char CommandParserBatteryInfo(char *buffer,unsigned char size);
unsigned char CommandParserCallback(char *buffer,unsigned char size);
unsigned char CommandParserClearBootCounter(char *buffer,unsigned char size);
unsigned char CommandParserEEPROM(char *buffer,unsigned char size);
unsigned char CommandParserBench(char *buffer,unsigned char size);
unsigned char CommandParserRTC(char *buffer,unsigned char size);
unsigned char CommandParser1(char *buffer,unsigned char size);
unsigned char CommandParserInterrupts(char *buffer,unsigned char size);
unsigned char CommandParserI2CTest(char *buffer,unsigned char size);
unsigned char CommandParserInterface(char *buffer,unsigned char size);
unsigned char CommandParserBenchmark(char *buffer,unsigned char size);
unsigned char CommandParserSleep(char *buffer,unsigned char size);
unsigned char CommandParserCPUReg(char *buffer,unsigned char size);
unsigned char CommandParserAudio(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkCPU(char *buffer,unsigned char size);
unsigned char CommandParserModeDACTest(char *buffer,unsigned char size);
unsigned char CommandParserWait(char *buffer,unsigned char size);
unsigned char CommandParserPeriphPower(char *buffer,unsigned char size);

unsigned char CommandShouldQuit(void);
void CommandChangeMode(unsigned char newmode);


#endif
