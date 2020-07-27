#include <math.h>
#include "mpu.h"
#include "mpu_config.h"
#include "mpu_geometry.h"
#include "MadgwickAHRS.h"
#include "MadgwickAHRS_float.h"
#include "wait.h"
#include "main.h"


#define MPU_GEOMETRY_BENCH	1

#if MPU_GEOMETRY_BENCH==1
unsigned long _mpu_quat_time=0;
#endif






float rad_to_deg(float rad)
{
	return rad/3.14159265*180.0;
}

/*
	https://math.stackexchange.com/questions/1772138/getting-tait-bryan-angles-from-quaternion-for-a-non-standard-left-handed-coordi/1774493
	
	Convert to Tait–Bryan/aerospace/zx'y\" (yaw, pitch, roll).
	
	In the sensor node:
	- Z points perpendicular to the node's plane outwards of the bluetooth module.
	- Y points in plane with the node, pointinug outwards the bluetooth module.
	- X points outwards the power switch.
	
*/
void mpu_quaternion_to_aerospace(float *yaw,float *pitch, float *roll)
{
	*yaw=rad_to_deg(atan2((-2*_mpu_q1*_mpu_q2+2*_mpu_q0*_mpu_q3),(1-2*_mpu_q1*_mpu_q1-2*_mpu_q3*_mpu_q3)));
	

	*pitch = rad_to_deg(asin(2*_mpu_q2*_mpu_q3+2*_mpu_q0*_mpu_q1));
	
	*roll = rad_to_deg(atan2((-2*_mpu_q1*_mpu_q3+2*_mpu_q0*_mpu_q2),(1-2*_mpu_q1*_mpu_q1-2*_mpu_q2*_mpu_q2)));
}


void mpu_compute_geometry(MPUMOTIONDATA *mpumotiondata,MPUMOTIONGEOMETRY *mpumotiongeometry)
{
	// Compute the quaternions if in a quaternion mode
	
	
	if(sample_mode==MPU_MODE_ACCGYRMAGQ || sample_mode==MPU_MODE_Q || sample_mode==MPU_MODE_E || sample_mode==MPU_MODE_QDBG)
	{
		#if ENABLEQUATERNION==1
			#if FIXEDPOINTQUATERNION==1
			
			//float tax,tay,taz;
			//tax = mpu_data_ax[mpu_data_rdptr]*1.0/16384.0;
			//tay = mpu_data_ay[mpu_data_rdptr]*1.0/16384.0;
			//taz = mpu_data_az[mpu_data_rdptr]*1.0/16384.0;
			
			// change range to avoid possible overflow
			//tax /= 16;
			//tay /= 16;
			//taz /= 16;

			FIXEDPOINTTYPE ax,ay,az,gx,gy,gz;
			
			//ax=tax;
			//ay=tay;
			//az=taz;
			
			ax = mpumotiondata.ax*atog;
			ay = mpumotiondata.ay*atog;
			az = mpumotiondata.az*atog;
			gx = mpumotiondata.gx*mpu_gtorps;
			gy = mpumotiondata.gy*mpu_gtorps;
			gz = mpumotiondata.gz*mpu_gtorps;				
			
			// Killing g does not fix bug
			// Killing a seems to fix bug
			// Killing m does not seem to fix bug.
			//gx=0;
			//gy=0;
			//gz=0;
			
			// bug still present with g=0 and m=0 and a amplitude reduced to avoid overflow. seems related to normalisation
			// bug not related to normalisation of a
			// bug seems in recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
			
			
			
			// Bug might be normalisation related
			
			
			// Sensors x (y)-axis of the accelerometer is aligned with the y (x)-axis of the magnetometer;
			// the magnetometer z-axis (+ down) is opposite to z-axis (+ up) of accelerometer and gyro!
			//unsigned long t1,t2;
			//t1=timer_us_get();
			//MadgwickAHRSupdate_fixed(gx,gy,gz,ax,ay,az,	-mpumotiondata.my,
			//										-mpumotiondata.mx,
			//										mpumotiondata.mz);
				
			MadgwickAHRSupdate_fixed(gx,gy,gz,ax,ay,az,mpumotiondata.mx,		// Coordinate system now rotated during acquisition
													mpumotiondata.my,
													mpumotiondata.mz);
			//t2=timer_us_get();
			//printf("%lu\n",t2-t1);
			//MadgwickAHRSupdate_fixed(gx,gy,gz,ax,ay,az,	0,
			//										0,
			//										0);
			#else
			float ax,ay,az,gx,gy,gz;
			
			// Acc does not need normalisation as it is normalised by Madgwick 
			//ax = mpumotiondata.ax*atog;
			//ay = mpumotiondata.ay*atog;
			//az = mpumotiondata.az*atog;
			ax = mpumotiondata->ax;
			ay = mpumotiondata->ay;
			az = mpumotiondata->az;
			gx = mpumotiondata->gx*mpu_gtorps;
			gy = mpumotiondata->gy*mpu_gtorps;
			gz = mpumotiondata->gz*mpu_gtorps;
			// Sensors x (y)-axis of the accelerometer is aligned with the y (x)-axis of the magnetometer;
			// the magnetometer z-axis (+ down) is opposite to z-axis (+ up) of accelerometer and gyro!
			//unsigned long t1,t2;
			//t1=timer_us_get();
			
			float mx,my,mz;
			
			//mx = -mpumotiondata.my;
			//my = -mpumotiondata.mx;
			//mz = mpumotiondata.mz;
			mx = mpumotiondata->mx;		// Coordinate system now rotated during acquisition
			my = mpumotiondata->my;
			mz = mpumotiondata->mz;
			
			//printf("Qpre: %f %f %f %f\n",_mpu_q0,_mpu_q1,_mpu_q2,_mpu_q3);
			//printf("Param: %f %f %f  %f %f %f  %f %f %f\n",gx,gy,gz,ax,ay,az,mx,my,mz);
			//printf("Param2: %f %f %f  %f %f %f  %f %f %f\n",gx,gy,gz,ax,ay,az,-mpumotiondata.my,-mpumotiondata.mx,mpumotiondata.mz);
			// Gives: _mpu_q0=w, _mpu_q1=x,_mpu_q2=y,_mpu_q3=z
			#if MPU_GEOMETRY_BENCH==1
			unsigned long t1=timer_us_get();
			#endif
			MadgwickAHRSupdate_float(gx,gy,gz,ax,ay,az,mx,my,mz);
			#if MPU_GEOMETRY_BENCH==1
			unsigned long t2=timer_us_get();
			_mpu_quat_time = (_mpu_quat_time*31+(t2-t1))/32;
			#endif
			
			mpumotiongeometry->q0 = _mpu_q0;
			mpumotiongeometry->q1 = _mpu_q1;
			mpumotiongeometry->q2 = _mpu_q2;
			mpumotiongeometry->q3 = _mpu_q3;
			
			
			
			//MadgwickAHRSupdate_float(0,0,0,1,0,0,1,0,0);
			//testf(gx,gy,gz,ax,ay,az,	-mpumotiondata.my,
			//										-mpumotiondata.mx,
			//										mpumotiondata.mz);
			
			//testf(gx,gy,gz,ax,ay,az,mx,my,mz);
			
			//testf(1,2,3,4,5,6,7,8,9);
			//printf("Qpost: %f %f %f %f\n",_mpu_q0,_mpu_q1,_mpu_q2,_mpu_q3);
			
			
			//t2=timer_us_get();
			//printf("%lu\n",t2-t1);
			#endif
		#else
			//_mpu_q0=_mpu_q1=_mpu_q2=_mpu_q3=0;
			mpumotiongeometry->q0 = mpumotiongeometry->q1 = mpumotiongeometry->q2 = mpumotiongeometry->q3 = 0;
		#endif
	}
	
	
	if(sample_mode==MPU_MODE_E)
	{
		#if ENABLEQUATERNION==1
		mpu_quaternion_to_aerospace(&mpumotiongeometry->yaw,&mpumotiongeometry->pitch,&mpumotiongeometry->roll);
		// https://math.stackexchange.com/questions/1772138/getting-tait-bryan-angles-from-quaternion-for-a-non-standard-left-handed-coordi/1774493
		
		#else
		mpumotiongeometry->yaw=mpumotiongeometry->pitch=mpumotiongeometry->roll=0;
		#endif
		
		//fprintf(file_pri,"%f %f %f %f:  yaw: %f pitch %f roll: %f\n",_mpu_q0,_mpu_q1,_mpu_q2,_mpu_q3,yaw,pitch,t2);
		//fprintf(file_pri,"yaw: %f pitch %f roll: %f\n",mpumotiongeometry.yaw,mpumotiongeometry.pitch,mpumotiongeometry.roll);
	}
	if(sample_mode==MPU_MODE_QDBG)
	{
		#if ENABLEQUATERNION==1
		
		float a2=acos(_mpu_q0);
		mpumotiongeometry->alpha=rad_to_deg(2*a2);
		float a2s = sin(a2);
		mpumotiongeometry->x = _mpu_q1/a2s;
		mpumotiongeometry->y = _mpu_q2/a2s;
		mpumotiongeometry->z = _mpu_q3/a2s;
		
		#else
		mpumotiongeometry->alpha=mpumotiongeometry->x=mpumotiongeometry->y=mpumotiongeometry->z=0;
		#endif
		
		//fprintf(file_pri,"%f %f %f %f:  yaw: %f pitch %f roll: %f\n",_mpu_q0,_mpu_q1,_mpu_q2,_mpu_q3,yaw,pitch,t2);
		//fprintf(file_pri,"alpha: %f x %f y %f z %f\n",mpumotiongeometry.alpha,mpumotiongeometry.x,mpumotiongeometry.y,mpumotiongeometry.z);
	}
	
}

unsigned long mpu_compute_geometry_time(void)
{
	#if MPU_GEOMETRY_BENCH==1
	return _mpu_quat_time;
	#endif
	return 0;
}



