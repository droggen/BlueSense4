#include <stdio.h>
#include "power.h"
#include "global.h"
#include "ltc2942.h"

//
_POWERUSE_DATA _poweruse_data_atoff;			// Data about power stored prior to platform turning off
_POWERUSE_DATA _poweruse_data_aton;				// Data about power read when platform turns on

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

		// Compute delta-T in seconds; does not work if the measurement is across one month
		unsigned long deltams = _poweruse_data_aton.time-_poweruse_data_atoff.time;

		fprintf(file_pri,"\tDelta T: %lu ms\n",deltams);

		// Compute power
		signed short pwr1 = ltc2942_getavgpower(_poweruse_data_atoff.charge,_poweruse_data_aton.charge,_poweruse_data_aton.voltage,deltams);
		signed long pwr2 = ltc2942_getavgpower2(_poweruse_data_atoff.charge,_poweruse_data_aton.charge,_poweruse_data_atoff.voltage,_poweruse_data_aton.voltage,deltams/1000l);
		fprintf(file_pri,"\tpwr1: %d mW pwr2: %ld uW\n",pwr1,pwr2);
	}
	else
		fprintf(file_pri,"\tNo valid off-power data\n");

}
