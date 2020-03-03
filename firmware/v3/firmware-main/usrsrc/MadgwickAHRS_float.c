//=====================================================================================================
// MadgwickAHRS.c
//=====================================================================================================
//
// Implementation of Madgwick's IMU and AHRS algorithms.
// See: http://www.x-io.co.uk/node/8#open_source_ahrs_and_imu_algorithms
//
// Date			Author          Notes
// 29/09/2011	SOH Madgwick    Initial release
// 02/10/2011	SOH Madgwick	Optimised for reduced CPU load
// 19/02/2012	SOH Madgwick	Magnetometer measurement is normalised
// 2016			D. Roggen		Code size optimisation
//=====================================================================================================

/*
	Madgwick's algorithm consider the following cases only:
	
	Acc!=0; Mag==0		Use Gyro, Acc
	Acc==0; Mag==0		Use Gyro
	Acc!=0; Mag!=0;		Use Gyro, Acc, Mag
	Acc==0; Mag!=0		Use Gyro
	
*/

//---------------------------------------------------------------------------------------------------
// Header files

#if ENABLEQUATERNION==1
#if FIXEDPOINTQUATERNION==0

#include "MadgwickAHRS.h"
#include <math.h>
#include <stdio.h>
#include "usrmain.h"
#include "global.h"


//---------------------------------------------------------------------------------------------------
// Definitions

//#define sampleFreq	512.0f		// sample frequency in Hz
//#define sampleFreq	100.0f		// sample frequency in Hz

// 0.1 too slow
//#define betaDef		0.1f		// 2 * proportional gain
//#define betaDef		1.1f		// 2 * proportional gain		// dan - too much
//#define betaDef		0.4f		// 2 * proportional gain		// dan - default
//#define betaDef		1.0f		// 2 * proportional gain		// dan -tryouts
//float betaDef;



//---------------------------------------------------------------------------------------------------
// Variable definitions
//float invSampleFreq = 1.0/sampleFreq;
float invSampleFreq;
//volatile float beta = betaDef;								// 2 * proportional gain (Kp)
float beta;								// 2 * proportional gain (Kp)
float _mpu_q0 = 1.0f, _mpu_q1 = 0.0f, _mpu_q2 = 0.0f, _mpu_q3 = 0.0f;	// quaternion of sensor frame relative to auxiliary frame
unsigned char corrds;
unsigned char corrdsctr;

float mxo,myo,mzo;
float axo,ayo,azo;

//---------------------------------------------------------------------------------------------------
// Function declarations

float invSqrtf(float x);

//====================================================================================================
// Functions

void MadgwickAHRSinit(float sampleFreq,float _beta,unsigned char _corrds)
{


	invSampleFreq = 1.0/sampleFreq;
	beta = _beta;
	corrds = _corrds;
	corrdsctr=0;
	
	_mpu_q0 = 1.0f; 
	_mpu_q1 = 0.0f;
	_mpu_q2 = 0.0f;
	_mpu_q3 = 0.0f;
	
	mxo=myo=mzo=0;
	axo=ayo=azo=0;

	//fprintf(file_pri,"MadgwickAHRSinit frq: %f beta: %f coords: %d. invfrq: %f\n",sampleFreq,_beta,_corrds,invSampleFreq);
}

//---------------------------------------------------------------------------------------------------
// AHRS algorithm update

void testf(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz) {
	printf("MadgParam: %f %f %f  %f %f %f  %f %f %f\n",gx,gy,gz,ax,ay,az,mx,my,mz);
}

void MadgwickAHRSupdate_float(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz) {
	float recipNorm;
	float s0, s1, s2, s3;
	float qDot1, qDot2, qDot3, qDot4;
	float hx, hy;
	float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz, _4bx, _4bz, _2q0, _2q1, _2q2, _2q3, _2q0q2, _2q2q3, q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;
	float _4q0, _4q1, _4q2 ,_8q1, _8q2;
	
	
	
	//fprintf(file_pri,"Madgqpre: %f %f %f %f\n",_mpu_q0,_mpu_q1,_mpu_q2,_mpu_q3);
	//fprintf(file_pri,"MadgParam: %f %f %f  %f %f %f  %f %f %f. ifrq %f\n",gx,gy,gz,ax,ay,az,mx,my,mz,invSampleFreq);
	
	// Rate of change of quaternion from gyroscope
	qDot1 = 0.5f * (-_mpu_q1 * gx - _mpu_q2 * gy - _mpu_q3 * gz);
	qDot2 = 0.5f * (_mpu_q0 * gx + _mpu_q2 * gz - _mpu_q3 * gy);
	qDot3 = 0.5f * (_mpu_q0 * gy - _mpu_q1 * gz + _mpu_q3 * gx);
	qDot4 = 0.5f * (_mpu_q0 * gz + _mpu_q1 * gy - _mpu_q2 * gx);

	//fprintf(file_pri,"qdot: %f %f %f %f\n",qDot1,qDot2,qDot3,qDot4);


	//fprintf(file_pri,"I");

	// Downsampling of correction
	corrdsctr++;
	if(corrdsctr>corrds)
	{
		corrdsctr=0;
		//fprintf(file_pri,"C");
		// Compute feedback only if accelerometer mxeasurement valid (avoids NaN in accelerometer normalisation)
		if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

			// Filter
			/*
			if(0)
			{
				ax=axo=(3.0*axo+ax)/4.0;
				ay=ayo=(3.0*ayo+ay)/4.0;
				az=azo=(3.0*azo+az)/4.0;
			}
			*/

			// Normalise accelerometer measurement
			//printf("A");
			recipNorm = invSqrtf(ax * ax + ay * ay + az * az);
			ax *= recipNorm;
			ay *= recipNorm;
			az *= recipNorm;   

			// Auxiliary variables to avoid repeated arithmetic
			_2q0 = 2.0f * _mpu_q0;
			_2q1 = 2.0f * _mpu_q1;
			_2q2 = 2.0f * _mpu_q2;
			_2q3 = 2.0f * _mpu_q3;
			q0q0 = _mpu_q0 * _mpu_q0;
			q1q1 = _mpu_q1 * _mpu_q1;
			q2q2 = _mpu_q2 * _mpu_q2;
			q3q3 = _mpu_q3 * _mpu_q3;
			
			if(!((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)))
			{
				// mag is non null
				
				// Filter. 
				/*
				//Magnetic field is noisy, but filtering has side effects.
				if(0)
				{
					mx=mxo=(31.0*mxo+mx)/32.0;
					my=myo=(31.0*myo+my)/32.0;
					mz=mzo=(31.0*mzo+mz)/32.0;
				}
				*/
				
				// Normalise magnetometer measurement
				//printf("B");
				recipNorm = invSqrtf(mx * mx + my * my + mz * mz);
				//printf("%f \n",recipNorm);
				mx *= recipNorm;
				my *= recipNorm;
				mz *= recipNorm;

				// Auxiliary variables to avoid repeated arithmetic
				_2q0mx = 2.0f * _mpu_q0 * mx;
				_2q0my = 2.0f * _mpu_q0 * my;
				_2q0mz = 2.0f * _mpu_q0 * mz;
				_2q1mx = 2.0f * _mpu_q1 * mx;
				_2q0q2 = 2.0f * _mpu_q0 * _mpu_q2;
				_2q2q3 = 2.0f * _mpu_q2 * _mpu_q3;
				q0q1 = _mpu_q0 * _mpu_q1;
				q0q2 = _mpu_q0 * _mpu_q2;
				q0q3 = _mpu_q0 * _mpu_q3;
				q1q2 = _mpu_q1 * _mpu_q2;
				q1q3 = _mpu_q1 * _mpu_q3;
				q2q3 = _mpu_q2 * _mpu_q3;
				

				// Reference direction of Earth's magnetic field
				hx = mx * q0q0 - _2q0my * _mpu_q3 + _2q0mz * _mpu_q2 + mx * q1q1 + _2q1 * my * _mpu_q2 + _2q1 * mz * _mpu_q3 - mx * q2q2 - mx * q3q3;
				hy = _2q0mx * _mpu_q3 + my * q0q0 - _2q0mz * _mpu_q1 + _2q1mx * _mpu_q2 - my * q1q1 + my * q2q2 + _2q2 * mz * _mpu_q3 - my * q3q3;
				_2bx = sqrt(hx * hx + hy * hy);
				_2bz = -_2q0mx * _mpu_q2 + _2q0my * _mpu_q1 + mz * q0q0 + _2q1mx * _mpu_q3 - mz * q1q1 + _2q2 * my * _mpu_q3 - mz * q2q2 + mz * q3q3;
				_4bx = 2.0f * _2bx;
				_4bz = 2.0f * _2bz;

				// Gradient decent algorithm corrective step
				s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax) + _2q1 * (2.0f * q0q1 + _2q2q3 - ay) - _2bz * _mpu_q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * _mpu_q3 + _2bz * _mpu_q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * _mpu_q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
				s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax) + _2q0 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * _mpu_q1 * (1 - 2.0f * q1q1 - 2.0f * q2q2 - az) + _2bz * _mpu_q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * _mpu_q2 + _2bz * _mpu_q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * _mpu_q3 - _4bz * _mpu_q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
				s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax) + _2q3 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * _mpu_q2 * (1 - 2.0f * q1q1 - 2.0f * q2q2 - az) + (-_4bx * _mpu_q2 - _2bz * _mpu_q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * _mpu_q1 + _2bz * _mpu_q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * _mpu_q0 - _4bz * _mpu_q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
				s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax) + _2q2 * (2.0f * q0q1 + _2q2q3 - ay) + (-_4bx * _mpu_q3 + _2bz * _mpu_q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * _mpu_q0 + _2bz * _mpu_q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * _mpu_q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
			} 
			else
			{
				// mag is null
				// Auxiliary variables to avoid repeated arithmetic
				_4q0 = 4.0f * _mpu_q0;
				_4q1 = 4.0f * _mpu_q1;
				_4q2 = 4.0f * _mpu_q2;
				_8q1 = 8.0f * _mpu_q1;
				_8q2 = 8.0f * _mpu_q2;

				// Gradient decent algorithm corrective step
				s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
				s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * _mpu_q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
				s2 = 4.0f * q0q0 * _mpu_q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
				s3 = 4.0f * q1q1 * _mpu_q3 - _2q1 * ax + 4.0f * q2q2 * _mpu_q3 - _2q2 * ay;
			}
			//printf("C");
			recipNorm = invSqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3); // normalise step magnitude
			//printf("%f \n",recipNorm);
			//printf("%f %f %f %f  %f\n",s0,s1,s2,s3,recipNorm);
			
			// Numerical instability with very small values of ||s|| -> limit correction.
			if(recipNorm>4)
				recipNorm=4;
			s0 *= recipNorm;
			s1 *= recipNorm;
			s2 *= recipNorm;
			s3 *= recipNorm;

			// Apply feedback step
			/*qDot1 -= beta * s0;
			qDot2 -= beta * s1;
			qDot3 -= beta * s2;
			qDot4 -= beta * s3;*/
			
			qDot1 -= beta * s0*(corrds+1);
			qDot2 -= beta * s1*(corrds+1);
			qDot3 -= beta * s2*(corrds+1);
			qDot4 -= beta * s3*(corrds+1);
			
			
			
		}
	}	// Downsampling of correction

	// Integrate rate of change of quaternion to yield quaternion
	_mpu_q0 += qDot1 * invSampleFreq;
	_mpu_q1 += qDot2 * invSampleFreq;
	_mpu_q2 += qDot3 * invSampleFreq;
	_mpu_q3 += qDot4 * invSampleFreq;

	// Normalise quaternion
	//printf("D");
	recipNorm = invSqrtf(_mpu_q0 * _mpu_q0 + _mpu_q1 * _mpu_q1 + _mpu_q2 * _mpu_q2 + _mpu_q3 * _mpu_q3);
	_mpu_q0 *= recipNorm;
	_mpu_q1 *= recipNorm;
	_mpu_q2 *= recipNorm;
	_mpu_q3 *= recipNorm;
	
	//fprintf(file_pri,"Madgqpost: %f %f %f %f\n",_mpu_q0,_mpu_q1,_mpu_q2,_mpu_q3);
}



//---------------------------------------------------------------------------------------------------
// Fast inverse square-root
// See: http://en.wikipedia.org/wiki/Fast_inverse_square_root

float invSqrtf(float x) {

	//fprintf(file_pri,"invsqrt %f ",x);


	float halfx = 0.5f * x;
	float y = x;
	long i = *(long*)&y;
	i = 0x5f3759df - (i>>1);
	y = *(float*)&i;
	y = y * (1.5f - (halfx * y * y));

	
	
	//float y = 1.0/sqrt(x);     // No need to save computation here
	
	//fprintf(file_pri,"%f\n",y);
	return y;
}

//====================================================================================================
// END OF CODE
//====================================================================================================
#endif
#endif
