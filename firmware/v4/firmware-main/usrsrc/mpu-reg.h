#ifndef __MPU_REG_H
#define __MPU_REG_H


// AK8963 address:0X0C
#define MAG_ADDRESS 0X0C
#define MPU_ADDRESS 105

#define ICM_BANK0					0x00
#define ICM_BANK1					0x10
#define ICM_BANK2					0x20
#define ICM_BANK3					0x30

//========= Bank 0 =========
#define ICM_R_WHOAMI				0x00
#define ICM_B_WHOAMI				ICM_BANK0

#define ICM_R_USR_CTRL	 			0x03
#define ICM_B_USR_CTRL	 			ICM_BANK0

#define ICM_R_LP_CONFIG				0x05
#define ICM_B_LP_CONFIG				ICM_BANK0

#define ICM_R_PWR_MGMT_1 			0x06
#define ICM_B_PWR_MGMT_1 			ICM_BANK0

#define ICM_R_PWR_MGMT_2 			0x07
#define ICM_B_PWR_MGMT_2 			ICM_BANK0

#define ICM_R_INTERRUPTPIN			0x0F
#define ICM_B_INTERRUPTPIN			ICM_BANK0

#define ICM_R_INTERRUPTENABLE 		0x10
#define ICM_B_INTERRUPTENABLE 		ICM_BANK0
#define ICM_R_INTERRUPTENABLE_1		0x11
#define ICM_B_INTERRUPTENABLE_1		ICM_BANK0
#define ICM_R_INTERRUPTENABLE_2		0x12
#define ICM_B_INTERRUPTENABLE_2		ICM_BANK0
#define ICM_R_INTERRUPTENABLE_3		0x13
#define ICM_B_INTERRUPTENABLE_3		ICM_BANK0

#define ICM_R_I2C_MST_STATUS		0x17

#define ICM_R_INT_STATUS			0x19
#define ICM_R_INT_STATUS_1			0x20
#define ICM_R_INT_STATUS_2			0x21
#define ICM_R_INT_STATUS_3			0x22

#define ICM_B_ACCGYROTEMP			ICM_BANK0
#define ICM_R_ACCEL_XOUT_H			0x2D
#define ICM_R_ACCEL_XOUT_L			0x2E
#define ICM_R_ACCEL_YOUT_H			0x2F
#define ICM_R_ACCEL_YOUT_L			0x30
#define ICM_R_ACCEL_ZOUT_H			0x31
#define ICM_R_ACCEL_ZOUT_L			0x32
#define ICM_R_GYRO_XOUT_H			0x33
#define ICM_R_GYRO_XOUT_L			0x34
#define ICM_R_GYRO_YOUT_H			0x35
#define ICM_R_GYRO_YOUT_L			0x36
#define ICM_R_GYRO_ZOUT_H			0x37
#define ICM_R_GYRO_ZOUT_L			0x38
#define ICM_R_TEMP_XOUT_H			0x39
#define ICM_R_TEMP_XOUT_L			0x3A

#define ICM_R_FIFO_EN_1				0x66
#define ICM_R_FIFO_EN_2				0x67
#define ICM_R_FIFO_RST				0x68
#define ICM_R_FIFO_MODE				0x69
#define ICM_R_FIFO_COUNTH			0x70
#define ICM_R_FIFO_COUNTL			0x71
#define ICM_R_FIFO_RW				0x72

#define ICM_R_REG_BANK_SEL			0x7f

//========= Bank 1 =========
#define ICM_R_TIMEBASE_CORRECTION_PLL	0x28

#define ICM_R_SELF_TEST_X_GYRO		0x02
#define ICM_R_SELF_TEST_Y_GYRO		0x03
#define ICM_R_SELF_TEST_Z_GYRO		0x04
#define ICM_R_SELF_TEST_X_ACCEL		0x0E
#define ICM_R_SELF_TEST_Y_ACCEL		0x0F
#define ICM_R_SELF_TEST_Z_ACCEL		0x10


//========= Bank 2 =========
#define ICM_R_GYRO_SMPLRT_DIV		0x00
#define ICM_B_GYRO_SMPLRT_DIV		ICM_BANK2
#define ICM_R_GYRO_CONFIG_1			0x01
#define ICM_B_GYRO_CONFIG_1			ICM_BANK2
#define ICM_R_GYRO_CONFIG_2			0x02
#define ICM_B_GYRO_CONFIG_2			ICM_BANK2

#define ICM_R_XG_OFFS_USRH			0x03
#define ICM_R_XG_OFFS_USRL			0x04
#define ICM_R_YG_OFFS_USRH			0x05
#define ICM_R_YG_OFFS_USRL			0x06
#define ICM_R_ZG_OFFS_USRH			0x07
#define ICM_R_ZG_OFFS_USRL			0x08

#define ICM_R_ACCEL_SMPLRT_DIV_1	0x10
#define ICM_B_ACCEL_SMPLRT_DIV_1	ICM_BANK2
#define ICM_R_ACCEL_SMPLRT_DIV_2	0x11
#define ICM_B_ACCEL_SMPLRT_DIV_2	ICM_BANK2

#define ICM_R_ACCEL_CONFIG			0x14
#define ICM_B_ACCEL_CONFIG			ICM_BANK2
#define ICM_R_ACCEL_CONFIG_2		0x15
#define ICM_B_ACCEL_CONFIG_2		ICM_BANK2

#define ICM_R_TEMP_CONFIG			0x53

//========= Bank 3 =========
#define ICM_R_I2C_MST_ODR_CONFIG 0x00
#define ICM_R_I2C_MST_CTRL		0x01
#define ICM_R_I2C_MST_DELAY_CTRL 0x02

#define ICM_R_I2C_SLV0_ADDR		0x03
#define ICM_R_I2C_SLV0_REG		0x04
#define ICM_R_I2C_SLV0_CTRL		0x05
#define ICM_R_I2C_SLV0_DO 		0x06

#define ICM_R_I2C_SLV4_ADDR		0x13
#define ICM_R_I2C_SLV4_REG		0x14
#define ICM_R_I2C_SLV4_CTRL		0x15
#define ICM_R_I2C_SLV4_DO		0x16
#define ICM_R_I2C_SLV4_DI 		0x17


#define MPU_R_I2C_MST_STATUS	54


/*#define MPU_R_I2C_MST_CTRL		36
#define MPU_R_I2C_MST_STATUS	54
#define MPU_R_I2C_MST_DELAY_CTRL 103*/

//--------------------------

#define MPU_R_SMPLRT_DIV 			0x19
#define MPU_R_CONFIG				0x1A
//#define MPU_R_GYROCONFIG			0x1B
//#define MPU_R_ACCELCONFIG			0x1C
//#define MPU_R_ACCELCONFIG2			0x1D
#define MPU_R_LPODR					0x1E
//#define MPU_R_FIFOEN 				0x23
//#define MPU_R_INTERRUPTPIN			0x37
//#define MPU_R_INTERRUPTENABLE 		0x38
//#define MPU_R_USR_CTRL	 			106
//#define MPU_R_PWR_MGMT_1 			0x6B
//#define MPU_R_PWR_MGMT_2			0x6C
//#define MPU_R_WHOAMI					0x75


/*#define MPU_R_I2C_SLV0_ADDR		37
#define MPU_R_I2C_SLV0_REG		38
#define MPU_R_I2C_SLV0_CTRL		39
#define MPU_R_I2C_SLV0_DO 99

#define MPU_R_I2C_SLV4_ADDR		49
#define MPU_R_I2C_SLV4_REG		50
#define MPU_R_I2C_SLV4_DO		51
#define MPU_R_I2C_SLV4_CTRL		52
#define MPU_R_I2C_SLV4_DI 		53*/
#define MPU_R_INT_STATUS		58

/*#define MPU_R_I2C_MST_CTRL		36
#define MPU_R_I2C_MST_STATUS	54
#define MPU_R_I2C_MST_DELAY_CTRL 103*/

/*#define MPU_LPODR_500 11
#define MPU_LPODR_250 10
#define MPU_LPODR_125 9
#define MPU_LPODR_62 8
#define MPU_LPODR_31 7
#define MPU_LPODR_16 6
#define MPU_LPODR_8 5
#define MPU_LPODR_4 4
#define MPU_LPODR_2 3
#define MPU_LPODR_1 2*/


// Gyro scale
#define MPU_GYR_SCALE_250 0
#define MPU_GYR_SCALE_500 1
#define MPU_GYR_SCALE_1000 2
#define MPU_GYR_SCALE_2000 3

// Acc scale
#define MPU_ACC_SCALE_2		0
#define MPU_ACC_SCALE_4		1
#define MPU_ACC_SCALE_8		2
#define MPU_ACC_SCALE_16	3


//========= Magnetometer =========
#define ICM_R_M_ST1			0x10
#define ICM_R_M_HXL			0x11
#define ICM_R_M_ST2			0x18
#define ICM_R_M_CNTL2		0x31


#endif // __MPU_REG_H
