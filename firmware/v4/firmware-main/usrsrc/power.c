#include <stdio.h>
#include "power.h"
#include "global.h"
#include "ltc2942.h"
#include "wait.h"

//
_POWERUSE_DATA _poweruse_data_atoff;			// Data about power stored prior to platform turning off
_POWERUSE_DATA _poweruse_data_aton;				// Data about power read when platform turns on

_POWERUSE_DATA _powermeas_data;					// Data for power measurement functions

/******************************************************************************
	function: power_print_off_on_info
*******************************************************************************
	Prints power consumption between two snapshots: _poweruse_data_atoff and _poweruse_data_aton.

	Used to calculate power consumption in "off" mode.

	Parameters:

	Returns:

******************************************************************************/
void power_print_off_on_info()
{
	// Render info
	fprintf(file_pri,"Off power data:\n");
	if(_poweruse_data_atoff.valid)
	{
		fprintf(file_pri,"\tPower off at: %lu ms. Q: %lu V: %u\n",_poweruse_data_atoff.time,_poweruse_data_atoff.charge,_poweruse_data_atoff.voltage);
		fprintf(file_pri,"\tPower on at: %lu ms. Q: %lu V: %u\n",_poweruse_data_aton.time,_poweruse_data_aton.charge,_poweruse_data_aton.voltage);

		// Compute the power
		signed long pwr2;
		unsigned long deltams;
		signed short pwr1 = power_compute(&_poweruse_data_atoff,&_poweruse_data_aton,&deltams,&pwr2);

		fprintf(file_pri,"\tDelta T: %lu ms\n",deltams);

		// Compute power
		fprintf(file_pri,"\tpwr1: %d mW pwr2: %ld uW\n",pwr1,pwr2);
	}
	else
		fprintf(file_pri,"\tNo valid off-power data\n");

}

/******************************************************************************
	function: power_get_state
*******************************************************************************
	Get voltage, charge and time and store in _POWERUSE_DATA structure.

	parameter left_right mandatory

	Parameters:
		pud:		Pointer to the _POWERUSE_DATA structure
	Returns:
		0			Ok
		Nonzero		Error

	Interrupts:
		Do not use from interrupts.

******************************************************************************/
unsigned char power_get_state(_POWERUSE_DATA *pud)
{
	pud->charge = ltc2942_getcharge();
	pud->voltage = ltc2942_getvoltage();
	pud->time = timer_ms_get();
	// No error checking currently
	return 0;
}



/******************************************************************************
	function: power_measure_start
*******************************************************************************
	Start a power measurement by reading voltage, coulomb and time.

	The power consumed is obtained when calling power_measure_stop.

	Parameters:
		-

	Returns:
		-

	Interrupts:
		Do not use from interrupts.

******************************************************************************/
void power_measure_start()
{
	power_get_state(&_powermeas_data);
}
/******************************************************************************
	function: power_measure_stop
*******************************************************************************
	Stop a power measurement and returns power used on average, and time elapsed



	Parameters:
		timems:			Pointer to a variable which receives the elapsed time in ms, if non-null.

	Returns:
		Power consumed in mW

	Interrupts:
		Do not use from interrupts.

******************************************************************************/
signed long power_measure_stop(unsigned long *timems)
{
	_POWERUSE_DATA pstop;
	power_get_state(&pstop);

	signed long pwruw;
	signed long pwrmw = power_compute(&_powermeas_data,&pstop,timems,&pwruw);

	return pwrmw;
}
/******************************************************************************
	function: power_compute
*******************************************************************************
	Compute the power between two _POWERUSE_DATA structures


	Parameters:
		pudstart:		Pointer to structure holding the start power data
		pudend:			Pointer to structure holding the end power data
		timems:			Pointer to a variable which receives the elapsed time in ms, if non-null.
		pwruw:			Pointer to a variable which receives the power in uW, if non-null

	Returns:
		Power consumed in mW

	Interrupts:
		Do not use from interrupts.

******************************************************************************/
signed long power_compute(_POWERUSE_DATA *pudstart,_POWERUSE_DATA *pudend,unsigned long *timems,signed long *pwruw)
{
	// Compute delta-T in seconds; does not work if the measurement is across one month
	unsigned long deltams = pudend->time-pudstart->time;

	//fprintf(file_pri,"\tDelta T: %lu ms\n",deltams);

	// Compute power
	// pwr1: power in mW
	signed short pwr1 = ltc2942_getavgpower(pudstart->charge,pudend->charge,pudend->voltage,deltams);
	// pwr2: power in uW
	signed long pwr2 = ltc2942_getavgpower2(pudstart->charge,pudend->charge,pudstart->voltage,pudend->voltage,deltams/1000l);

	if(timems)
		*timems=deltams;
	if(pwruw)
		*pwruw=pwr2;

	return pwr1;

}
