//=====================================================================================================
// MadgwickAHRS.h
//=====================================================================================================
//
// Implementation of Madgwick's IMU and AHRS algorithms.
// See: http://www.x-io.co.uk/node/8#open_source_ahrs_and_imu_algorithms
//
// Date			Author          Notes
// 29/09/2011	SOH Madgwick    Initial release
// 02/10/2011	SOH Madgwick	Optimised for reduced CPU load
//
//=====================================================================================================

#if ENABLEQUATERNION==1
#if FIXEDPOINTQUATERNION==0

#ifndef MadgwickAHRS_FLOAT_H
#define MadgwickAHRS_FLOAT_H



//----------------------------------------------------------------------------------------------------
// Variable declaration

extern float beta;				// algorithm gain
extern float _mpu_q0, _mpu_q1, _mpu_q2, _mpu_q3;	// quaternion of sensor frame relative to auxiliary frame

//---------------------------------------------------------------------------------------------------
// Function declarations
void testf(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz) ;
//void MadgwickAHRSinit(void);
void MadgwickAHRSinit(float sampleFreq,float _beta,unsigned char _corrds);
void MadgwickAHRSupdate_float(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz);


#endif
#endif
#endif
//=====================================================================================================
// End of file
//=====================================================================================================
