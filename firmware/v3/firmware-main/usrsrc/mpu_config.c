#include <stdio.h>
#include <string.h>
#include "mpu_config.h"
#include "mpu-reg.h"
#include "mpu.h"
#include "MadgwickAHRS_float.h"


/*
	File: motionconfig

Principles:
	Common ODR for gyro and acceleration modes which is at least twice the DLP frequency
	DLP enabled

	Low noise:
		ODR			ODRDiv		Acc3DB(cfg)		Gyro3DB			Include?
		1125Hz		0			473Hz(7)		361Hz(7)		X
		562.5Hz 	1			246Hz(1)		196.6(0)		V
		375Hz 		2			111Hz(2)		151.8(1)		V
		281.25		3			X				119.5(2)		X
		225Hz 		4			111Hz(2)		119.5(2)		V
		187.5Hz		5			x				x				x
		112.5Hz 	9			x				x				x
		102Hz		10			50.4Hz(3)		51.2(3)			V
		51.1Hz		21			23.9(4)			23.9(4)			V
		10Hz		112			5.7Hz(6)		5.7(6)			V
		1Hz			1124		5.7Hz(6)		5.7(6)			X
		


	Low power (duty cycled):
		125 Hz
		10Hz
		1Hz




*/

// Exhaustive set of frequencies up to 1KHz
const char mc_0[]   =  "Motion off";

const char mc_1000[]   =  "562.5Hz Gyro (BW=197Hz)";
const char mc_1001[]   =  "  375Hz Gyro (BW=152Hz)";
const char mc_1002[]   =  "  225Hz Gyro (BW=120Hz)";
const char mc_1003[]   =  "  102Hz Gyro (BW= 51Hz)";
const char mc_1004[]   =  "   51Hz Gyro (BW= 25Hz)";
const char mc_1005[]   =  "   10Hz Gyro (BW=  5Hz)";

const char mc_2001[]   = "562.5Hz Acc  (BW=246Hz)";
const char mc_2002[]   = "  375Hz Acc  (BW=111Hz)";
const char mc_2003[]   = "  225Hz Acc  (BW=111Hz)";
const char mc_2004[]   = "  102Hz Acc  (BW= 50Hz)";
const char mc_2005[]   = "   51Hz Acc  (BW= 25Hz)";
const char mc_2006[]   = "   10Hz Acc  (BW=  5Hz)";

const char mc_3001[]   = "562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz)";
const char mc_3002[]   = "  375Hz Gyro (BW=152Hz) + Acc  (BW=111Hz)";
const char mc_3003[]   = "  225Hz Gyro (BW=120Hz) + Acc  (BW=111Hz)";
const char mc_3004[]   = "  102Hz Gyro (BW= 51Hz) + Acc  (BW= 50Hz)";
const char mc_3005[]   = "   51Hz Gyro (BW= 25Hz) + Acc  (BW= 25Hz)";
const char mc_3006[]   = "   10Hz Gyro (BW=  5Hz) + Acc  (BW=  5Hz)";


//const char mc_4000[]   = "  125Hz Acc low power";
//const char mc_4001[]   = "   10Hz Acc low power";
//const char mc_4002[]   = "    1Hz Acc low power";


const char mc_5001[]   = "562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz) + Mag 10Hz";
const char mc_5002[]   = "  375Hz Gyro (BW=152Hz) + Acc  (BW=111Hz) + Mag 10Hz";
const char mc_5003[]   = "  225Hz Gyro (BW=120Hz) + Acc  (BW=111Hz) + Mag 10Hz";
const char mc_5004[]   = "  102Hz Gyro (BW= 51Hz) + Acc  (BW= 50Hz) + Mag 10Hz";
const char mc_5005[]   = "   51Hz Gyro (BW= 25Hz) + Acc  (BW= 25Hz) + Mag 10Hz";
const char mc_5006[]   = "   10Hz Gyro (BW=  5Hz) + Acc  (BW=  5Hz) + Mag 10Hz";

const char mc_6001[]   = "562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz) + Mag 100Hz";
const char mc_6002[]   = "  375Hz Gyro (BW=152Hz) + Acc  (BW=111Hz) + Mag 100Hz";
const char mc_6003[]   = "  225Hz Gyro (BW=120Hz) + Acc  (BW=111Hz) + Mag 100Hz";
const char mc_6004[]   = "  102Hz Gyro (BW= 51Hz) + Acc  (BW= 50Hz) + Mag 100Hz";
const char mc_6005[]   = "   51Hz Gyro (BW= 25Hz) + Acc  (BW= 25Hz) + Mag 100Hz";
const char mc_6006[]   = "   10Hz Gyro (BW=  5Hz) + Acc  (BW=  5Hz) + Mag 100Hz";

const char mc_7001[]   = "562.5Hz Acc  (BW=197Hz) Gyro (BW=246Hz) + Mag 100Hz + Quaternions";
const char mc_7002[]   = "  375Hz Gyro (BW=152Hz) + Acc  (BW=111Hz) + Mag 100Hz + Quaternions";
const char mc_7003[]   = "  225Hz Gyro (BW=120Hz) + Acc  (BW=111Hz) + Mag 100Hz + Quaternions";
const char mc_7004[]   = "  102Hz Gyro (BW= 51Hz) + Acc  (BW= 50Hz) + Mag 100Hz + Quaternions";

const char mc_8001[]   = "562.5Hz Quaternions Quaternions Qsg=(qw,qx,qy,qz); rotates vector in earth coords G into sensor coords S";
const char mc_8002[]   = "  375Hz Quaternions";
const char mc_8003[]   = "  225Hz Quaternions";
const char mc_8004[]   = "  102Hz Quaternions";

const char mc_9001[]   = "  102Hz Tait–Bryan/aerospace/zx'y\", intrinsic (yaw, pitch, roll)";
const char mc_9002[]   = "  102Hz Quaternions debug (angle, x,y,z)";



const char *mc_options[MOTIONCONFIG_NUM]   =
{
	// Off
    mc_0,
	// Gyro only
    mc_1000,
    mc_1001,
    mc_1002,
    mc_1003,
    mc_1004,
    mc_1005,
	// Acc only
    mc_2001,
    mc_2002,
    mc_2003,
    mc_2004,
    mc_2005,
	mc_2006,
	// Acc+Gyro
    mc_3001,
    mc_3002,
    mc_3003,
    mc_3004,
    mc_3005,
	mc_3006,
	// LP Acc
//    mc_4000,		// LP 125Hz
//    mc_4001,		// LP 10Hz
//    mc_4002,		// LP 1Hz
	// Acc+Gyro+Mag 10Hz
	mc_5001,
	mc_5002,
	mc_5003,
	mc_5004,
	mc_5005,
	mc_5006,
	// Acc+Gyro+Mag 100Hz
	mc_6001,
	mc_6002,
	mc_6003,
	mc_6004,
	mc_6005,
	mc_6006,
	// Acc+Gyro+Mag 100Hz + Quat
	mc_7001,
	mc_7002,
	mc_7003,
	mc_7004,

	// Quat
	mc_8001,
	mc_8002,
	mc_8003,
	mc_8004,

	// Quat debug
	mc_9001,
	mc_9002,
};




/******************************************************************************
	config_sensorsr_settings
*******************************************************************************	
	Configures the motion sensor based (active sensors and sample rate) on 
	config_sensorsr.
	
	config_sensorsr_settings contains for each option: mode gdlpe gdlpoffhbw gdlpbw adlpe adlpbw divider lpodr softdiv
	mode is 0=gyro, 1=acc, 2=gyroacc, 3=lpacc
	
	magmode: 0=off, 1=8Hz, 2=100Hz. Node that magmode is only used in moe MPU_MODE_ACCGYRMAG
	
	magdiv:	magnetometer ODR divider, reads data at ODR/(1+magdiv). The ODR is the sample frequency divided by (1+divider).

	ODR = 1125/(1+ODRDiv)


******************************************************************************/
//const char hello[]   = {1,2,3};

const short config_sensorsr_settings[MOTIONCONFIG_NUM][12] = {
					// mode              gdlpe gdlpoffhbw        gdlpbw     adlpe           adlpbw   ODRDiv        divider softdiv	magmode	magdiv		splratex10
					// Off
					{ MPU_MODE_OFF,        0,         0,               0,     0,               0,      0,             0,     0,     0,		0,			0},

					//=== Regular gyroscope === (6 modes)
					// 562.5Hz Gyro (BW=196.6Hz)
					{ MPU_MODE_GYR,        1,         0, ICM_GYRO_LPF_196,    0,               0,      1,             0,     0,    0,		0,			5625},			//
					// 375Hz Gyro (BW=152Hz)
					{ MPU_MODE_GYR,        1,         0, ICM_GYRO_LPF_152,    0,               0,      2,             0,     0,     0,		0,			3750},			//
					// 225Hz Gyro (BW=119.5Hz)
					{ MPU_MODE_GYR,        1,         0, ICM_GYRO_LPF_120,    0,               0,      4,             0,     0,     0,		0,			2250},
					// 102Hz Gyro (BW= 51Hz)
					{ MPU_MODE_GYR,        1,         0, ICM_GYRO_LPF_51,     0,               0,     10,             0,     0,     0,		0,			1023},
					// 51Hz Gyro (BW= 25Hz)
					{ MPU_MODE_GYR,        1,         0, ICM_GYRO_LPF_25,     0,               0,     21,             0,     0,     0,		0,			511},
					// 10Hz Gyro (BW=  5Hz)
					{ MPU_MODE_GYR,        1,         0, ICM_GYRO_LPF_5,      0,               0,    111,             0,     0,     0,		0,			100},

					//=== Regular acceleration === (6 modes)

					// 1125Hz Acc  (BW=473Hz)
					//{ MPU_MODE_ACC,      0,         0,               0,     1, ICM_ACC_LPF_473,      0,             0,     0,     0,		0,			1125},			// ODR=1125Hz	OK
					// 562.5Hz Acc  (BW=246Hz)
					{ MPU_MODE_ACC,        0,         0,               0,     1,  ICM_ACC_LPF_246,     1,             0,     0,     0,		0,			5625},
					// 375 Acc  (BW=111Hz)
					{ MPU_MODE_ACC,        0,         0,               0,     1,  ICM_ACC_LPF_111,     2,             0,     0,     0,		0,			3750},
					// 225Hz Acc  (BW=111Hz)
					{ MPU_MODE_ACC,        0,         0,               0,     1,  ICM_ACC_LPF_111,     4,             0,     0,     0,		0,			2250},
					// 102Hz Acc  (BW= 50Hz)
					{ MPU_MODE_ACC,        0,         0,               0,     1,   ICM_ACC_LPF_50,    10,             0,     0,     0,		0,			1023},		// OK
					// 50Hz Acc  (BW= 25Hz)
					{ MPU_MODE_ACC,        0,         0,               0,     1,   ICM_ACC_LPF_25,    21,             0,     0,     0,		0,			511},		// OK
					// 10Hz Acc  (BW=  5Hz)
					{ MPU_MODE_ACC,        0,         0,               0,     1,    ICM_ACC_LPF_5,   111,             0,     0,     0,		0,			100},		// OK
					// 1Hz Acc  (BW=  5Hz)
					//{ MPU_MODE_ACC,        0,         0,               0,     1,    ICM_ACC_LPF_5,  1124,             0,     0,     0,		0,			1},			// OK

					//=== Regular acceleration+gyro === (6 modes)
					// 562.5Hz Gyro (BW=196.6Hz) + Acc (246Hz)
					{ MPU_MODE_ACCGYR,     1,         0, ICM_GYRO_LPF_196,    1, ICM_ACC_LPF_246,      1,             0,     0,    0,		0,			5625},			//
					// 375Hz Gyro (BW=152Hz) + Acc (BW=111Hz)
					{ MPU_MODE_ACCGYR,     1,         0, ICM_GYRO_LPF_152,    1, ICM_ACC_LPF_111,      2,             0,     0,     0,		0,			3750},			//
					// 225Hz Gyro (BW=119.5Hz) + Acc (BW=111Hz)
					{ MPU_MODE_ACCGYR,     1,         0, ICM_GYRO_LPF_120,    1, ICM_ACC_LPF_111,      4,             0,     0,     0,		0,			2250},
					// 102Hz Gyro (BW= 51Hz) + Acc (BW= 50Hz)
					{ MPU_MODE_ACCGYR,     1,         0, ICM_GYRO_LPF_51,     1,  ICM_ACC_LPF_50,     10,             0,     0,     0,		0,			1023},
					// 51Hz Gyro (BW= 25Hz) + Acc (BW= 25Hz)
					{ MPU_MODE_ACCGYR,     1,         0, ICM_GYRO_LPF_25,     1,  ICM_ACC_LPF_25,     21,             0,     0,     0,		0,			511},
					// 10Hz Gyro (BW=  5Hz) + Acc (BW=  5Hz)
					{ MPU_MODE_ACCGYR,     1,         0, ICM_GYRO_LPF_5,      1,   ICM_ACC_LPF_5,    111,             0,     0,     0,		0,			100},


					//=== Low-power acceleration === (3 modes)
					/*
					// 125Hz Acc low power
					{ MPU_MODE_LPACC,      0,         0,               0,     0,               0,      0, ICM_LPODR_125,     0,     0,		0,			125},
					// 125Hz Acc low power
					{ MPU_MODE_LPACC,      0,         0,               0,     0,               0,      0, ICM_LPODR_10,     0,     0,		0,			10},
					// 1Hz Acc low power
					{ MPU_MODE_LPACC,      0,         0,               0,     0,               0,      0,   ICM_LPODR_1,     0,     0,		0,			1},
					*/

					//=== Magn 8Hz === (6 modes)
					// 562.5Hz Gyro (BW=196.6Hz) + Acc (246Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_196,    1, ICM_ACC_LPF_246,      1,             0,     0,     1,	   54,			5625},	// ODR=562Hz, mag=10HZ, magodr=562.5/55=10.2Hz
					// 375Hz Gyro (BW=152Hz) + Acc (BW=111Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_152,    1, ICM_ACC_LPF_111,      2,             0,     0,     1,	   36,			3750},	// ODR=375Hz, mag=10HZ, magodr=375/55=10.13Hz
					// 225Hz Gyro (BW=119.5Hz) + Acc (BW=111Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_120,    1, ICM_ACC_LPF_111,      4,             0,     0,     1,	   20,			2250},	// ODR=225Hz, mag=10HZ, magodr=225/21=10.7Hz
					// 102Hz Gyro (BW= 51Hz) + Acc (BW= 50Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_51,     1,  ICM_ACC_LPF_50,     10,             0,     0,     1,		8,			1023},	// ODR=102Hz, mag=10HZ, magodr=102/9=11.3Hz
					// 51Hz Gyro (BW= 25Hz) + Acc (BW= 25Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_25,     1,  ICM_ACC_LPF_25,     21,             0,     0,     1,		4,			511},	// ODR=51Hz, mag=10HZ, magodr=51/5=10.2Hz
					// 10Hz Gyro (BW=  5Hz) + Acc (BW=  5Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_5,      1,   ICM_ACC_LPF_5,    111,             0,     0,     1,		0,			10},	// ODR=10Hz, mag=10HZ, magodr=51/1=10Hz
					
					//=== Magn 100Hz === (6 modes)
					// 562.5Hz Gyro (BW=196.6Hz) + Acc (246Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_196,    1, ICM_ACC_LPF_246,      1,             0,     0,     2,	    4,			5625},	// ODR=562Hz, mag=100HZ, magodr=562.5/5=112.4Hz
					// 375Hz Gyro (BW=152Hz) + Acc (BW=111Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_152,    1, ICM_ACC_LPF_111,      2,             0,     0,     2,	    2,			3750},	// ODR=375Hz, mag=100HZ, magodr=375/3=125Hz
					// 225Hz Gyro (BW=119.5Hz) + Acc (BW=111Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_120,    1, ICM_ACC_LPF_111,      4,             0,     0,     2,	    1,			2250},	// ODR=225Hz, mag=100HZ, magodr=225/2=112.5Hz
					// 102Hz Gyro (BW= 51Hz) + Acc (BW= 50Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_51,     1,  ICM_ACC_LPF_50,     10,             0,     0,     2,		0,			1023},	// ODR=102Hz, mag=10H0Z, magodr=102/1=102Hz
					// 51Hz Gyro (BW= 25Hz) + Acc (BW= 25Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_25,     1,  ICM_ACC_LPF_25,     21,             0,     0,     2,		0,			511},	// ODR=51Hz, mag=100HZ, magodr=51/1=51Hz
					// 10Hz Gyro (BW=  5Hz) + Acc (BW=  5Hz)
					{ MPU_MODE_ACCGYRMAG,  1,         0, ICM_GYRO_LPF_5,      1,   ICM_ACC_LPF_5,    111,             0,     0,     2,		0,			10},	// ODR=10Hz, mag=100HZ, magodr=10/1=10Hz

					//=== Acc+Gyro+Mag 100Hz + Quat === (4 modes)
					// 562.5Hz Gyro (BW=196.6Hz) + Acc (246Hz) + Mag 100Hz
					{ MPU_MODE_ACCGYRMAGQ, 1,         0, ICM_GYRO_LPF_196,    1, ICM_ACC_LPF_246,      1,             0,     0,     2,		4,			5625},	// ODR=562Hz, mag=100HZ, magodr=562.5/5=112.4Hz
					// 375Hz Gyro (BW=152Hz) + Acc (BW=111Hz) + Mag 100Hz
					{ MPU_MODE_ACCGYRMAGQ, 1,         0, ICM_GYRO_LPF_152,    1, ICM_ACC_LPF_111,      2,             0,     0,     2,		2,			3750},	// ODR=375Hz, mag=100HZ, magodr=375/3=125Hz
					// 225Hz Gyro (BW=119.5Hz) + Acc (BW=111Hz) + Mag 100Hz
					{ MPU_MODE_ACCGYRMAGQ, 1,         0, ICM_GYRO_LPF_120,    1, ICM_ACC_LPF_111,      4,             0,     0,     2,		1,			2250},	// ODR=225Hz, mag=100HZ, magodr=225/2=112.5Hz
					// 102Hz Gyro (BW= 51Hz) + Acc (BW= 50Hz) + Mag 100Hz
					{ MPU_MODE_ACCGYRMAGQ, 1,         0, ICM_GYRO_LPF_51,     1,  ICM_ACC_LPF_50,     10,             0,     0,     2,		0,			1023},	// ODR=102Hz, mag=10H0Z, magodr=102/1=102Hz

					//=== Quat === (4 modes)
					// 562.5Hz Gyro (BW=196.6Hz) + Acc (246Hz) + Mag 100Hz
					{ MPU_MODE_Q,          1,         0, ICM_GYRO_LPF_196,    1, ICM_ACC_LPF_246,      1,             0,     0,     2,		4,			5625},	// ODR=562Hz, mag=100HZ, magodr=562.5/5=112.4Hz
					// 375Hz Gyro (BW=152Hz) + Acc (BW=111Hz) + Mag 100Hz
					{ MPU_MODE_Q,          1,         0, ICM_GYRO_LPF_152,    1, ICM_ACC_LPF_111,      2,             0,     0,     2,		2,			3750},	// ODR=375Hz, mag=100HZ, magodr=375/3=125Hz
					// 225Hz Gyro (BW=119.5Hz) + Acc (BW=111Hz) + Mag 100Hz
					{ MPU_MODE_Q,          1,         0, ICM_GYRO_LPF_120,    1, ICM_ACC_LPF_111,      4,             0,     0,     2,		1,			2250},	// ODR=225Hz, mag=100HZ, magodr=225/2=112.5Hz
					// 102Hz Gyro (BW= 51Hz) + Acc (BW= 50Hz) + Mag 100Hz
					{ MPU_MODE_Q,          1,         0, ICM_GYRO_LPF_51,     1,  ICM_ACC_LPF_50,     10,             0,     0,     2,		0,			1023},	// ODR=102Hz, mag=10H0Z, magodr=102/1=102Hz

					//=== Quat debug === (2 modes)

					// 102Hz Gyro (BW= 51Hz) + Acc (BW= 50Hz) + Mag 100Hz
					{ MPU_MODE_E,          1,         0, ICM_GYRO_LPF_51,     1,  ICM_ACC_LPF_50,     10,             0,     0,     2,		0,			1023},	// ODR=102Hz, mag=10H0Z, magodr=102/1=102Hz
					// 102Hz Gyro (BW= 51Hz) + Acc (BW= 50Hz) + Mag 100Hz
					{ MPU_MODE_QDBG,       1,         0, ICM_GYRO_LPF_51,     1,  ICM_ACC_LPF_50,     10,             0,     0,     2,		0,			1023},	// ODR=102Hz, mag=10H0Z, magodr=102/1=102Hz
			};
/******************************************************************************
	function: mpu_config_motionmode
*******************************************************************************	
	Activates a motion sensing mode among a predefined list of available modes.
	
	Parameters:
		sensorsr	-	Motion sensor mode
		autoread	-	Enables automatically reading the data from the motion 
						sensor and place it in a local buffer
*******************************************************************************/
/*void mpu_config_motionmode(unsigned char sensorsr,unsigned char autoread)
{
	// TOFIX
}*/
void mpu_config_motionmode(unsigned char sensorsr,unsigned char autoread)
{
 	// Sanitise
	if(sensorsr>=MOTIONCONFIG_NUM) 
		sensorsr=0;

	// Turn off MPU always
	_mpu_disableautoread();			// OK
	mpu_mode_off();
		
	//	fprintf(file_pri,"%d\n",sensorsr);
		
	sample_mode = config_sensorsr_settings[sensorsr][0];

	// Soft divider unused in BlueSense4
	__mpu_sample_softdivider_ctr=0;
	__mpu_sample_softdivider_divider = 	config_sensorsr_settings[sensorsr][8];

	_mpu_samplerate=config_sensorsr_settings[sensorsr][11];
	switch(sample_mode)
	{
		case MPU_MODE_OFF:
			// Already off: do nothing, but continue executing to set the autoread below
			break;
		case MPU_MODE_GYR:
			//fprintf(file_pri,"Setting mode gyro. dlpe %d  dlp bw %d div %d\n",config_sensorsr_settings[sensorsr][1],config_sensorsr_settings[sensorsr][3],config_sensorsr_settings[sensorsr][6]);
			mpu_mode_gyro(config_sensorsr_settings[sensorsr][1],config_sensorsr_settings[sensorsr][3],config_sensorsr_settings[sensorsr][6]);
			break;
		case MPU_MODE_ACC:
			//fprintf(file_pri,"Setting mode acc. acc dlpe %d  acc dlp bw %d div %d softdiv %d\n",config_sensorsr_settings[sensorsr][4],config_sensorsr_settings[sensorsr][5],config_sensorsr_settings[sensorsr][6],config_sensorsr_settings[sensorsr][8]);
			mpu_mode_acc(config_sensorsr_settings[sensorsr][4],config_sensorsr_settings[sensorsr][5],config_sensorsr_settings[sensorsr][6]);
			break;
		case MPU_MODE_ACCGYR:
			/*fprintf(file_pri,"Setting mode acc+gyro. Gyro: dlpe %d bw %d. Acc: dlpe %d bw %d. Div: %d\n",config_sensorsr_settings[sensorsr][1],config_sensorsr_settings[sensorsr][3],
					config_sensorsr_settings[sensorsr][4],config_sensorsr_settings[sensorsr][5],
					config_sensorsr_settings[sensorsr][6]);*/
			mpu_mode_accgyro(config_sensorsr_settings[sensorsr][1],config_sensorsr_settings[sensorsr][3],
								config_sensorsr_settings[sensorsr][4],config_sensorsr_settings[sensorsr][5],
								config_sensorsr_settings[sensorsr][6]);
			break;
		case MPU_MODE_ACCGYRMAG:
		case MPU_MODE_ACCGYRMAGQ:
		case MPU_MODE_Q:
		case MPU_MODE_E:
		case MPU_MODE_QDBG:
			// Enable the gyro
			/*fprintf(file_pri,"Setting mode acc+gyro. Gyro: dlpe %d bw %d. Acc: dlpe %d bw %d. Div: %d\n",config_sensorsr_settings[sensorsr][1],config_sensorsr_settings[sensorsr][3],
								config_sensorsr_settings[sensorsr][4],config_sensorsr_settings[sensorsr][5],
								config_sensorsr_settings[sensorsr][6]);*/
			mpu_mode_accgyro(config_sensorsr_settings[sensorsr][1],config_sensorsr_settings[sensorsr][3],
								config_sensorsr_settings[sensorsr][4],config_sensorsr_settings[sensorsr][5],
								config_sensorsr_settings[sensorsr][6]);

			// Enable additionally the magnetic field
			//fprintf_P(file_pri,PSTR("Mag mode %d\n"),config_sensorsr_settings[sensorsr][9]);
			_mpu_mag_mode(config_sensorsr_settings[sensorsr][9],config_sensorsr_settings[sensorsr][10]);
			//fprintf_P(file_pri,PSTR("Done\n"));
			break;
		case MPU_MODE_LPACC:
		default:
			mpu_mode_lpacc_icm20948(config_sensorsr_settings[sensorsr][7]);
	}
	
	if(autoread)
		_mpu_enableautoread();
	//fprintf(file_pri,"return from mpu_config_motionmode. _mpu_samplerate %d\n",_mpu_samplerate);
	
	// Initialise Madgwick
	#if ENABLEQUATERNION==1
	//MadgwickAHRSinit(((float)_mpu_samplerate)/10.0,_mpu_beta,(_mpu_samplerate/1000)*8-1);			// All -> 12.5Hz
	MadgwickAHRSinit(((float)_mpu_samplerate)/10.0,_mpu_beta,(_mpu_samplerate/1000)*1-1);			// All -> 100Hz
	#endif
	
	_mpu_current_motionmode = sensorsr;

}

/******************************************************************************
	function: mpu_get_motionmode
*******************************************************************************	
	Gets the current motion mode, which has been previously set with 
	mpu_config_motionmode.
	
	Parameters:
		autoread		-		Pointer to a variable receiving the autoread parameter.
								If null, then autoread is not returned.
	Returns:
		Current motion mode
*******************************************************************************/			
unsigned char mpu_get_motionmode(unsigned char *autoread)
{
	if(autoread)
		*autoread=__mpu_autoread;
	return _mpu_current_motionmode;
}

/******************************************************************************
	function: mpu_getmodename
*******************************************************************************	
	Returns the name of a motion mode from its ID.
	
	Parameters:
		motionmode	-	Motion sensor mode
		buffer		-	Long enough buffer to hold the name of the motion mode.
						The recommended length is 96 bytes (longest of mc_xx strings).
						The buffer is returned null-terminated.
		Returns:
			Name of the motion mode in buffer, or an empty string if the motionmode
			is invalid.
*******************************************************************************/
void mpu_getmodename(unsigned char motionmode,char *buffer)
{	
	buffer[0]=0;
	if(motionmode>=MOTIONCONFIG_NUM)
		return;
	strcpy(buffer,mc_options[motionmode]);
}


/******************************************************************************
	function: mpu_printmotionmode
*******************************************************************************	
	Prints the available MPU sensing modes
	
	Parameters:
		file	-	Output stream
*******************************************************************************/
void mpu_printmotionmode(FILE *file)
{	
	fprintf(file,"Motion modes (x indicates experimental modes; sample rate not respected):\n");
	for(unsigned char i=0;i<MOTIONCONFIG_NUM;i++)
	{
		char buf[128];
		mpu_getmodename(i,buf);
		fprintf(file,"[%d]\t",i);
		fputs(buf,file);
		fputc('\n',file);
	}		
}




