#ifndef __MADGWICKAHRS_H
#define __MADGWICKAHRS_H


#if ENABLEQUATERNION==1
#if FIXEDPOINTQUATERNION==1
#include "MadgwickAHRS_fixed.h"
#define MadgwickAHRSupdate MadgwickAHRSupdate_fixed 
#else
#include "MadgwickAHRS_float.h"
#define MadgwickAHRSupdate MadgwickAHRSupdate_float
#endif
#endif

#endif