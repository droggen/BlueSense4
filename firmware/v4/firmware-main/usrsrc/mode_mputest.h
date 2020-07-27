#ifndef __MODE_MPUTEST_H
#define __MODE_MPUTEST_H

unsigned char CommandParserMPUTest_ReadReg(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_ReadReg(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Start(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Stop(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Calibrate(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Reset(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Fifo(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_FifoEn(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_CalibrationData(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Magn(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_MagnMode(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Interface(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_External(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Shadow(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Off(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Poll(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Poll2(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Auto(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Bench(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Quaternion(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Quaternion2(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_MagneticCalib(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_GetMagneticCalib(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_MagneticSelfTest(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_BenchMath(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_AccScale(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_GyroScale(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_SetGyroBias(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Kill(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Beta(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_Init(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestLPAcc(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestWriteReg(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestWriteMagReg(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestAccDivider(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestGetKeyParameters(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_SetDefaults(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_PowerOnOff(char *buffer,unsigned char size);
unsigned char CommandParserMPUInitSPIMegalol(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest_SelfTest(char *buffer,unsigned char size);

void mode_mputest(void);

extern const char help_mt_r[];
extern const char help_mt_beta[];

#endif
