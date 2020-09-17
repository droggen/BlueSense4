
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "uiconfig.h"
#include "eeprom-m24.h"




/******************************************************************************
Configuration menu
******************************************************************************/




void ConfigSaveStreamTimestamp(unsigned char timestamp)
{
	eeprom_write_byte(CONFIG_ADDR_ENABLE_TIMESTAMP, timestamp?1:0);
}
unsigned char ConfigLoadStreamTimestamp(void)
{
	//return eeprom_read_byte((uint8_t*)CONFIG_ADDR_ENABLE_TIMESTAMP) ? 1:0;
	return eeprom_read_byte(CONFIG_ADDR_ENABLE_TIMESTAMP,1,0) ? 1:0;
}
void ConfigSaveStreamBattery(unsigned char battery)
{
	eeprom_write_byte(CONFIG_ADDR_ENABLE_BATTERY, battery?1:0);
}
unsigned char ConfigLoadStreamBattery(void)
{
	//return eeprom_read_byte((uint8_t*)CONFIG_ADDR_ENABLE_BATTERY) ? 1:0;
	return eeprom_read_byte(CONFIG_ADDR_ENABLE_BATTERY,1,0) ? 1:0;
}
void ConfigSaveStreamBinary(unsigned char binary)
{
	eeprom_write_byte(CONFIG_ADDR_STREAM_BINARY, binary?1:0);
}
unsigned char ConfigLoadStreamBinary(void)
{
	//return eeprom_read_byte((uint8_t*)CONFIG_ADDR_STREAM_BINARY) ? 1:0;
	return eeprom_read_byte(CONFIG_ADDR_STREAM_BINARY,1,0) ? 1:0;
}
void ConfigSaveStreamPktCtr(unsigned char pktctr)
{
	eeprom_write_byte(CONFIG_ADDR_STREAM_PKTCTR, pktctr?1:0);
}
unsigned char ConfigLoadStreamPktCtr(void)
{
	//return eeprom_read_byte((uint8_t*)CONFIG_ADDR_STREAM_PKTCTR) ? 1:0;
	return eeprom_read_byte(CONFIG_ADDR_STREAM_PKTCTR,1,0) ? 1:0;
}
void ConfigSaveStreamLabel(unsigned char label)
{
	eeprom_write_byte(CONFIG_ADDR_STREAM_LABEL, label?1:0);
}
unsigned char ConfigLoadStreamLabel(void)
{
	//return eeprom_read_byte((uint8_t*)CONFIG_ADDR_STREAM_LABEL) ? 1:0;
	return eeprom_read_byte(CONFIG_ADDR_STREAM_LABEL,1,0) ? 1:0;
}

/*void ConfigSaveADCMask(unsigned char mask)
{
	eeprom_write_byte((uint8_t*)CONFIG_ADDR_ADC_MASK,mask);
}
unsigned char ConfigLoadADCMask(void)
{
	return eeprom_read_byte((uint8_t*)CONFIG_ADDR_ADC_MASK);
}
void ConfigSaveADCPeriod(unsigned long period)
{
	eeprom_write_byte((uint8_t*)CONFIG_ADDR_ADC_PERIOD0,(period>>0)&0xff);
	eeprom_write_byte((uint8_t*)CONFIG_ADDR_ADC_PERIOD1,(period>>8)&0xff);
	eeprom_write_byte((uint8_t*)CONFIG_ADDR_ADC_PERIOD2,(period>>16)&0xff);
	eeprom_write_byte((uint8_t*)CONFIG_ADDR_ADC_PERIOD3,(period>>24)&0xff);
}
unsigned long ConfigLoadADCPeriod(void)
{
	unsigned long v=0;
	v = eeprom_read_byte((uint8_t*)CONFIG_ADDR_ADC_PERIOD3);
	v<<=8;
	v |= eeprom_read_byte((uint8_t*)CONFIG_ADDR_ADC_PERIOD2);
	v<<=8;
	v |= eeprom_read_byte((uint8_t*)CONFIG_ADDR_ADC_PERIOD1);
	v<<=8;
	v |= eeprom_read_byte((uint8_t*)CONFIG_ADDR_ADC_PERIOD0);
	
	if(v>5000000)
	{
		v=5000000;
		ConfigSaveADCPeriod(v);
	}
	
	return v;
}*/
void ConfigSaveTSPeriod(unsigned long period)
{
	//eeprom_write_byte((uint8_t*)CONFIG_ADDR_TS_PERIOD0,(period>>0)&0xff);
	//eeprom_write_byte((uint8_t*)CONFIG_ADDR_TS_PERIOD1,(period>>8)&0xff);
	//eeprom_write_byte((uint8_t*)CONFIG_ADDR_TS_PERIOD2,(period>>16)&0xff);
	//eeprom_write_byte((uint8_t*)CONFIG_ADDR_TS_PERIOD3,(period>>24)&0xff);
	eeprom_write_dword(CONFIG_ADDR_TS_PERIOD0,period);
}
unsigned long ConfigLoadTSPeriod(void)
{
	/*unsigned long v=0;
	v = eeprom_read_byte((uint8_t*)CONFIG_ADDR_TS_PERIOD3);
	v<<=8;
	v |= eeprom_read_byte((uint8_t*)CONFIG_ADDR_TS_PERIOD2);
	v<<=8;
	v |= eeprom_read_byte((uint8_t*)CONFIG_ADDR_TS_PERIOD1);
	v<<=8;
	v |= eeprom_read_byte((uint8_t*)CONFIG_ADDR_TS_PERIOD0);*/

	unsigned v = eeprom_read_dword(CONFIG_ADDR_TS_PERIOD0,1,1000000);
	
	if(v>5000000)
	{
		v=5000000;
		ConfigSaveTSPeriod(v);
	}
	


	return v;
}
void ConfigSaveEnableLCD(unsigned char lcden)
{
	eeprom_write_byte(CONFIG_ADDR_ENABLE_LCD, lcden?1:0);
}
unsigned char ConfigLoadEnableLCD(void)
{
	//return eeprom_read_byte((uint8_t*)CONFIG_ADDR_ENABLE_LCD) ? 1:0;
	return eeprom_read_byte(CONFIG_ADDR_ENABLE_LCD,1,0) ? 1:0;
}
void ConfigSaveEnableInfo(unsigned char ien)
{
	eeprom_write_byte(CONFIG_ADDR_ENABLE_INFO, ien?1:0);
}
unsigned char ConfigLoadEnableInfo(void)
{
	//return eeprom_read_byte((uint8_t*)CONFIG_ADDR_ENABLE_INFO) ? 1:0;
	return eeprom_read_byte(CONFIG_ADDR_ENABLE_INFO,1,0) ? 1:0;
}


/******************************************************************************
	function: ConfigLoadScript
*******************************************************************************
	Load the startup configuration script from EEPROM address 
	CONFIG_ADDR_SCRIPTSTART.
	
	This function reads exactly CONFIG_ADDR_SCRIPTLEN bytes.
	
	This function guarantees that the last byte is null terminated (i.e. if
	the last byte of the script wasn't a null it is lost).
	
	Parameters:
		buf		-		Buffer to receive the configuration script. Buf large 
						enough to receive CONFIG_ADDR_SCRIPTLEN bytes
******************************************************************************/
void ConfigLoadScript(char *buf)
{
	for(unsigned char i=0;i<CONFIG_ADDR_SCRIPTLEN;i++)
	{
		//buf[i] = eeprom_read_byte((uint8_t*)(CONFIG_ADDR_SCRIPTSTART+i));
		buf[i] = eeprom_read_byte(CONFIG_ADDR_SCRIPTSTART+i,1,0);
	}
	buf[CONFIG_ADDR_SCRIPTLEN-1]=0;		// For securitz, set the last byte to null
}
/******************************************************************************
	function: ConfigSaveScript
*******************************************************************************
	Save a startup configuration script to EEPROM address 
	CONFIG_ADDR_SCRIPTSTART.

	Writes exactly CONFIG_ADDR_SCRIPTLEN bytes. 
	
	Does not process the string to append a null-terminating byte. The application
	should however put a null terminating byte to indicate the script length.

	Parameters:
		buf		-		Buffer containing the configuration script.
		n		-		Number of bytes to write. Maximum is CONFIG_ADDR_SCRIPTLEN.
						If n is larger, only the first CONFIG_ADDR_SCRIPTLEN bytes
						are written.
******************************************************************************/
void ConfigSaveScript(char *buf,unsigned char n)
{
	if(n>CONFIG_ADDR_SCRIPTLEN)
		n=CONFIG_ADDR_SCRIPTLEN;
	
	for(unsigned char i=0;i<n;i++)
	{
		//eeprom_write_byte((uint8_t*)(CONFIG_ADDR_SCRIPTSTART+i),buf[i]);
		eeprom_write_byte(CONFIG_ADDR_SCRIPTSTART+i,buf[i]);
	}
}

/******************************************************************************
	function: ConfigLoadVT100
******************************************************************************/
unsigned char ConfigLoadVT100()
{
	return eeprom_read_byte(CONFIG_ADDR_VT100,1,0)?1:0;
}

/******************************************************************************
	function: ConfigSaveVT100
******************************************************************************/
void ConfigSaveVT100(unsigned char en)
{
	eeprom_write_byte(CONFIG_ADDR_VT100,en);
}











