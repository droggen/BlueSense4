#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rn41.h"
#include "serial.h"
#include "main.h"
#include "system.h"
#include "usrmain.h"
#include "serial_uart.h"

#define _delay_ms HAL_Delay

#define RN41DEBUG 1



/*
	file: rn41

	RN41 Bluetooth module functions
	
	The key functions are:
	
	* rn41_Setup:		Initialise the RN41

*/	
/*
	Notes about RN41:
		Changing a setting (i.e. ST for the configuration timeout) requires a reset either with "R,1" or with the reset pin.
		
		Setting the speed to 230.4 and higher leads to data overrun. Decreasing dbg callback frequency decrease the occurrence, but it still happens. 
		This is an issue if receiving data (such as bootloader), but not if streaming out.
		
		F,1 to enable fast mode is only a valid command during connection, otherwise ERR.
		
		ST with 0, 253, 254, 255 work as advertised. With another timeout the module still enters command mode.
		
		Prefer to use S-,<name> to set the name of the device as it is serialisation friendly
		
		Rerunning the configuration (not clear which parameters: name, pin, mode, devclass, srvclass) requires to remove/readd the device on windows otherwise the connection fails. Likely some encryption key that changed.
		
		Streaming data: 
			115.2K, w/o RTS, ST,255: speed 11563-11565 bytes/sec, with RTS often going off, then RN41 crashes (1-2 bursts of 128KB, not reboot); RTS led goes off, blinks, then stays mostly off with sporadic on; connection stays on. PC must disconnect, but this fails, rn41 must be reset
			115.2K, w/  RTS, ST,255: RN41 reboots (notifies REBOOT) and drops connection, but less frequently (3-6 bursts), 11143 bytes/sec. After reboot, PC can reconnect successfully after disconnection timeout on PC side.
			115.2K, w/o RTS, ST,0: 	no visible discontinuity in RTS light; speed 11563 bytes/sec. No crash after 6 bursts.
			115.2K, w/  RTS, ST,0: 	no visible discontinuity in RTS light; speed 11564 bytes/sec. No crash after 6 bursts.
			
			
			230.4K, w/  RTS, ST,255: RTS flickers continuously, speed 166606-16956, reset on burst 3 and 4
			230.4K, w/o RTS, ST,255: twice, crash halfway through burst; node receive garbage (likely "reboot" at 115k2), disconnection
			230.4K, w/o RTS, ST,0: no visible discontinuity in RTS light; speed 22870-22874 bytes/sec. No crash after 6 bursts.
			230.4K, w/  RTS, ST,0: no visible discontinuity in RTS light; speed 22870-22875 bytes/sec. No crash after 6 bursts.		
			
			460.6K, w/  RTS, ST,255: RTS flickers continuously, speed 20805-21389; once AVR rebooted; other time disconnect after 30 bursts
			460.6K, w/o RTS, ST,255: twice crash halfway through burst; node receive garbage (likely "reboot" at 115k2), disconnection
			460.4K, w/o RTS, ST,0: no visible discontinuity in RTS light; speed 24828-22874 bytes/sec. No crash after 6 bursts.
			460.6K, w/  RTS, ST,0: no visible discontinuity in RTS light; speed 24481-24504 bytes/sec. No crash after 6 bursts.
		(above benchmark with dbg_callback 3. with dbg_callback 0 speed at 230k4 is 22500 instead of 22800; with dbg_callback 1 speed at 230k4 is 22600 instead of 22800 - negligible impact on transmit, but important on receive)
			
		Speeds higher than 115k2 lead to data overrun on reception in the AVR; issue: AVR not fast enough to read data and move to a buffer.
		Recommended: 230k4; although reception of data must be handled by a slow sender on PC side
*/
/*
	The main contributing factors to power during idle (no connection) are:
	- Inquiry window (for discovery)
	- Page scan window (for connection)
	
	Decreasing the duration of the inquiry and page scan window lowers power significantly (other settings are set to defaults):	 
	- Inq&Page=0x100 (default): 	59mW
	- Inq&Page=0x20: 				44mW (-15mW)
	
	The sniff window (SW command) can decrease power during connection. 
	It only works if the dongle also supports sniff.
	During connection the radio will wake up at sniff intervals to see 
	if data must be exchanged.
	The RN41 documentation is unclear on this point, but experiment show that the radio stays 
	on (power equivalent to no sniff) for some time (experimentally determined to be about 20 seconds)
	before turning off again and sniffing.	
	The effect of sniffing are as follows:
	- There is an initial latency of up to the sniff window until the radio wakes up and stays on to exchange data
	- After the last data exchange the radio stays on for about 20 seconds; during this period there is no added latency
	- After the about 20second on period the radio goes back to sniffing, which leads again to an initial latency of up to the sniff window.
	A downside of sniffing is that it increases the likelihood of detecting "disconnections" (triggering a brief interface change), however 
	this is not well understood and could be a bug elsewhere in the firmware.
	
	During connection (other settings are set to defaults):
	- Sniff=0x0000 (default):		135/125mW while connected
	- Sniff=0x0640:					46mW while connected, without traffic (sniff every 1 second)
	
	
	Deep sleep, enabled by setting MSB of the sleep window (SW command) should make the RN41 go into deep sleep. 
	No change in behaviour or power usage has been observed setting the MSB of SW. The behaviour is identical 
	as to when setting the sniff window only.

	Low power connect mode (S| command) allows to set an ON/OFF ratio. 
	Two versions of the manual describe the function differently.
	Experimentally the syntax is: S|,ooOO with oo the OFF time in hex seconds and OO the ON time in hex seconds
	
	Power while waiting for a connection (other parameters to default values):
	S|,0000 (disabled)					60mW 	Baseline
	S|,0001 (off 0s, on 1s)				85mW 	While the setting does not make sense it is suprising to see higher power
	S|,0101 (off 1s, on 1s)				61mW 	Same as baseline
	S|,0301 (off 3s, on 1s)				52mW 	(-8mW)
	S|,0701 (off 7s, on 1s)				47/45mW (-13mW)
	S|,0F01 (off 15s, on 1s)			46/40mW (-15mW)
	
	Combinations of S| and SW (others to default):
	S|,0301 (off 3s, on 1s) + SW,8000	51mW	Deep sleep (MSB of SW) does not seem to activate
	S|,0301 (off 3s, on 1s) + SW,8640	51mW	Deep sleep (MSB of SW) does not seem to activate; however the sniff is enabled during connection
	
	Combinations of S| and SI, SJ (others to default):
	S|,0301 (off 3s, on 1s) + SI,0020 + SJ,0020		43mW
	SI,0020 + SJ,0020								45mW			
	
	

	Power:
		1. Idle, BTconn: 124/115mW
		2. Idle, BTnoconn: 61/57mW
		Idle, BTnoconn, quiet nondisc nonconn (Q): 42/39
		Idle, BTnoconn, quiet nondisc conn (Q,2): 49
		Idle, BTnoconn, SI,0012, SJ,0012 : 43/42	(discovery difficult) 43
		Idle, BTnoconn, SI,0019, SJ,0019 : 44/43	(discovery ok)	(Seems to be an ideal setting in idle: -15mW compared to default)
		Idle, BTnoconn, SI,0020, SJ,0020 : 44/43	(conn ok)
		Idle, BTnoconn, SI,0100, SJ,0100 : 61/59 	(default)
		Idle, BTnoconn, SI,0800, SJ,0800 : 177/167	(max window; power decreases after connection!)
		Idle, BTconn, SI,0012, SJ,0012 : 121/118
		Idle, BTnoconn, SI,0020, SJ,0020 : 118/115 (conn ok)
		Idle, BTnoconn, S@,1000:			61/58
		Idle, BTnoconn, SI,0019, SJ,0019, S|,0401:	x (connection difficult)
		Idle, BTnoconn, SI,0050, SJ,0050, S|,0401:	x (connection difficult)
		Idle, BTnoconn, SI,0100, SJ,0100, S|,0401:	49 (conn ok with moderate delay)
		Idle, BTnoconn, SI,0019, SJ,0019, S|,0301:	42/40
		Idle, BTnoconn, SI,0019, SJ,0019, S|,0201:	42 (conn ok)
		Idle, BTnoconn, SI,0019, SJ,0019, S|,0101:	43/42 (conn ok)
		Idle, BTnoconn, SW,9900:					(4 sec deep sleep, connection doesn't work) |
		Idle, BTnoconn, SW,8C80:					(2 sec deep sleep, connection doesn't work)	|	Not clear if needs a reset or not, or a sniff enabled dongle
		Idle, BTnoconn, SW,8640:					64/58 (1 sec deep sleep, conn ok)			|

		Idle, BTnoconn, S|,0101: 59
		Idle, BTnoconn, S|,0201: 55/51
		Idle, BTnoconn, S|,0801: 48/42						(long connection time, ~30 sec)
		S1. Idle w/sleep, BTnoconn, S|,0801: 23/21			(long connection time, ~30 sec)
		S2. Idle w/sleep, BTconn, S|,0801: 107 (should be identical to S4)
		S3. Idle w/ sleep, BTnoconn, 39/36
		S4. Idle w/ sleep, BTconn, 109/102
		
		
		TODO: S%,1000 (used on power up) or S@,1000 (used instantaneously)
		
*/

/*
	This requires:
	1) Interrupt reading function in fdevopen
	2) Enough buffer in rx buffer, as cts is not used.
*/

void rn41_CmdEnter(FILE *fileinfo,FILE *filebt)
{
	// No character 1 second before and after the enter command sequence
	//_delay_ms(1100);
	_delay_ms(1010);
	rn41_CmdResp(fileinfo,filebt,"$$$");
	_delay_ms(1010);
}
void rn41_CmdLeave(FILE *fileinfo,FILE *filebt)
{
	// No character 1 second before and after the enter command sequence
	_delay_ms(1100);
	rn41_CmdResp(fileinfo,filebt,"---\n");
}
void rn41_CmdResp(FILE *fileinfo,FILE *filebt,const char *cmd)
{
	char buffer[64];
	
	#if RN41DEBUG==1
		fprintf(fileinfo,"BT CmdResp '%s'\n",cmd);
	#endif
	fputs(cmd,filebt);
	fgets_timeout_100ms(buffer,64,filebt);
	#if RN41DEBUG==1
		fprintf(fileinfo,"BT CmdResp returned %d bytes\n",strlen(buffer));
	#endif
	buffer[strlen(buffer)-2] = 0;		// Strip cr-lf	
	#if RN41DEBUG==1
		fprintf(fileinfo,"BT CmdResp: %s\n",buffer);
	#endif
}
void rn41_CmdRespn(FILE *fileinfo,FILE *filebt,const char *cmd,unsigned char n)
{
	char buffer[256];
	//int l;
	
	//_delay_ms(1000);
	//l = fgettxbuflevel(filebt);
	//fprintf_P(fileinfo,PSTR("before cmd Buffer level: %d\n",l));
	
	fputs(cmd,filebt);
	//_delay_ms(1000);
	//l = fgettxbuflevel(filebt);
	//fprintf_P(fileinfo,PSTR("after cmd Buffer level: %d\n",l));
	for(unsigned char i=0;i<n;i++)
	{
		fgets_timeout_100ms(buffer,256,filebt);
		buffer[strlen(buffer)-2] = 0;		// Strip cr-lf
		fprintf(fileinfo,"CmdRespn %02d: %s\n",i,buffer);
		//l = fgettxbuflevel(filebt);
	}
}

void rn41_Reset(FILE *fileinfo)
{
	#if RN41DEBUG==1
		fprintf(fileinfo,"BT hard reset\n");
	#endif
	//fprintf_P(fileinfo,PSTR("PIND: %02X\n"),PIND);

	system_periph_setrn41resetpin(0);

	//fprintf_P(fileinfo,PSTR("BT hard reset: set to 0\n"));
	//fprintf_P(fileinfo,PSTR("PIND: %02X\n"),PIND);
	//_delay_ms(4);			// 4ms works  (3ms works also, 2ms doesn't)
	//HAL_Delay(300);
	HAL_Delay(500);
	//fprintf_P(fileinfo,PSTR("PIND: %02X\n"),PIND);
	//fprintf_P(fileinfo,PSTR("BT hard reset: set to 1\n"));

	system_periph_setrn41resetpin(1);

	HAL_Delay(500);

	//_delay_ms(200);		// Works
	#if RN41DEBUG==1
		fprintf(fileinfo,"BT hard reset done\n");
	#endif
}
unsigned char rn41_SetConfigTimer(FILE *fileinfo,FILE *filebt,unsigned char v)
{
	char buffer[64];
	sprintf(buffer,"ST,%d\n",v);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetPassData(FILE *fileinfo,FILE *filebt,unsigned char v)
{
	char buffer[64];
	sprintf(buffer,"T,%d\n",v?1:0);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetPIN(FILE *fileinfo,FILE *filebt,const char *pin)
{
	char buffer[64];
	sprintf(buffer,"SP,%s\n",pin);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetTempBaudrate(FILE *fileinfo,FILE *filebt,const char *b)
{
	char buffer[64];
	sprintf(buffer,"U,%s,N\n",b);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetName(FILE *fileinfo,FILE *filebt,char *name)
{
	char buffer[64];
	sprintf(buffer,"SN,%s\n",name);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetSerializedName(FILE *fileinfo,FILE *filebt,const char *name)
{
	char buffer[64];
	sprintf(buffer,"S-,%s\n",name);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetAuthentication(FILE *fileinfo,FILE *filebt,unsigned char a)
{
	char buffer[64];
	sprintf(buffer,"SA,%d\n",a);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetMode(FILE *fileinfo,FILE *filebt,unsigned char a)
{
	char buffer[64];
	sprintf(buffer,"SM,%d\n",a);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetServiceClass(FILE *fileinfo,FILE *filebt,unsigned short c)
{
	char buffer[64];
	sprintf(buffer,"SC,%04X\n",c);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetDeviceClass(FILE *fileinfo,FILE *filebt,unsigned short c)
{
	char buffer[64];
	sprintf(buffer,"SD,%04X\n",c);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetI(FILE *fileinfo,FILE *filebt,unsigned short c)
{
	char buffer[64];
	sprintf(buffer,"SI,%04X\n",c);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
/******************************************************************************
	function: rn41_SetJ
*******************************************************************************	
	Sets the page scan window.
	
	The page scan window command sets the amount of time the device spends enabling
	page scanning (connectability).
	The page scan interval is 0x800.
	
	Minimum value: 0x0012
	Default value: 0x0100
	Maximum value: 0x0800
	Disabled: 0x0000 (non connectable)
	
	Parameters:
		fileinfo	-	UI file 
		filebt		-	File on which the RN41 is available
		c			-	Page scan window
	
	Return:	
		0		-		Always
******************************************************************************/
unsigned char rn41_SetJ(FILE *fileinfo,FILE *filebt,unsigned short c)
{
	char buffer[64];
	sprintf(buffer,"SJ,%04X\n",c);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}

unsigned char rn41_SetSniff(FILE *fileinfo,FILE *filebt,unsigned short sniff)
{
	char buffer[64];
	sprintf(buffer,"SW,%04X\n",sniff);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_SetLowpower(FILE *fileinfo,FILE *filebt,unsigned short lowpower)
{
	char buffer[64];
	sprintf(buffer,"S|,%04X\n",lowpower);
	rn41_CmdResp(fileinfo,filebt,buffer);
	return 0;
}
unsigned char rn41_GetBasic(FILE *fileinfo,FILE *filebt)
{
	rn41_CmdRespn(fileinfo,filebt,"D\n",9);
	return 0;
}
unsigned char rn41_GetBasicParam(FILE *filebt,char *mac,char *name,char *baud,char *mode,char *auth,char *pin)
{
	char buffer[256];
	char *p;
	
	// Initialise the return values
	mac[0]=0;
	name[0]=0;
	baud[0]=0;
	mode[0]=0;
	auth[0]=0;
	pin[0]=0;

	fputs("D\n",filebt);



	fgets_timeout_100ms(buffer,256,filebt);				// Settings
	fgets_timeout_100ms(buffer,256,filebt);				// MAC
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	strcpy(mac,p+1);
	fgets_timeout_100ms(buffer,256,filebt);				// Name
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	strcpy(name,p+1);
	fgets_timeout_100ms(buffer,256,filebt);				// Baudrate
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	strcpy(baud,p+1);
	fgets_timeout_100ms(buffer,256,filebt);				// Mode
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	if(strcmp(p+1,"Slav")==0)
		*mode = 0;
	else 
		*mode = 255;
	fgets_timeout_100ms(buffer,256,filebt);				// Authentication
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	*auth = atoi(p);
	fgets_timeout_100ms(buffer,256,filebt);				// Pin
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	strcpy(pin,p+1);
	
	fgets_timeout_100ms(buffer,256,filebt);				// Bonded
	fgets_timeout_100ms(buffer,256,filebt);				// Rem
	
	return 0;
}

unsigned char rn41_GetExtended(FILE *fileinfo,FILE *filebt)
{
	rn41_CmdRespn(fileinfo,filebt,"E\n",11);
	return 0;
}

/******************************************************************************
	function: rn41_GetExtendedParam
*******************************************************************************	
	Reads the extended parameters of the RN41
	
	Parameters:
		filebt		-	File descriptor to communicate with the RN41
		srvclass	-	Pointer to variable holding the service class
		devclass	-	Pointer to variable holding the device class
		cfgtimer	-	Pointer to variable holding the configuration timeout parameter
		inqw		-	Pointer to variable holding the inquiry window length
		pagw		-	Pointer to variable holding the paging window length
	
	Return:	
		0		-		Always
		This function returns the RN41 parameters through the srvclass,
		devclass, cfgtimer, inqw, pagw variables.
******************************************************************************/
unsigned char rn41_GetExtendedParam(FILE *filebt,unsigned short *srvclass,unsigned short *devclass,unsigned char *cfgtimer,unsigned short *inqw,unsigned short *pagw)
{
	char buffer[256];
	char *p;
	
	fputs("E\n",filebt);

	fgets_timeout_100ms(buffer,256,filebt);				// Settings
	fgets_timeout_100ms(buffer,256,filebt);				// SrvName
	fgets_timeout_100ms(buffer,256,filebt);				// SrvClass
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	*srvclass = strtoul(p+1,0,16);
	fgets_timeout_100ms(buffer,256,filebt);				// DevClass
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	*devclass = strtoul(p+1,0,16);
	fgets_timeout_100ms(buffer,256,filebt);				// InqWindw
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	*inqw = strtoul(p+1,0,16);
	fgets_timeout_100ms(buffer,256,filebt);				// PagWindw
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	*pagw = strtoul(p+1,0,16);
	fgets_timeout_100ms(buffer,256,filebt);				// CfgTimer
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	*cfgtimer = atoi(p+1);
	fgets_timeout_100ms(buffer,256,filebt);				// StatusStr
	fgets_timeout_100ms(buffer,256,filebt);				// HidFlags
	fgets_timeout_100ms(buffer,256,filebt);				// DTRTimer
	fgets_timeout_100ms(buffer,256,filebt);				// KeySwapr
	
	return 0;	
}

unsigned char rn41_GetOtherParam(FILE *filebt,unsigned short *sniff,unsigned short *lp)
{
	char buffer[256];
	char *p;
	
	fputs("O\n",filebt);

	fgets_timeout_100ms(buffer,256,filebt);				// Settings
	fgets_timeout_100ms(buffer,256,filebt);				// Profil
	fgets_timeout_100ms(buffer,256,filebt);				// CfgChar
	fgets_timeout_100ms(buffer,256,filebt);				// SniffEna
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	*sniff = strtoul(p+1,0,16);
	fgets_timeout_100ms(buffer,256,filebt);				// LowPower
	buffer[strlen(buffer)-2] = 0;
	p = strchr(buffer,'=');
	*lp = strtoul(p+1,0,16);
	fgets_timeout_100ms(buffer,256,filebt);				// TX Power
	fgets_timeout_100ms(buffer,256,filebt);				// IOPorts
	fgets_timeout_100ms(buffer,256,filebt);				// IOValues
	fgets_timeout_100ms(buffer,256,filebt);				// Sleeptmr
	fgets_timeout_100ms(buffer,256,filebt);				// DebugMod
	fgets_timeout_100ms(buffer,256,filebt);				// RoleSwch
	
	return 0;
}


void rn41_PrintBasicParam(FILE *file,char *mac,char *name,char *baud,char mode,char auth,char *pin)
{
	fprintf(file,"Basic parameters:\n");
	fprintf(file,"\tMAC: %s\n",mac);
	fprintf(file,"\tname: %s\n",name);
	fprintf(file,"\tbaud: %s\n",baud);
	fprintf(file,"\tmode: %d\n",mode);
	fprintf(file,"\tauth: %d\n",auth);
	fprintf(file,"\tpin: %s\n",pin);
}
void rn41_PrintExtParam(FILE *file,unsigned short srvclass,unsigned short devclass, unsigned char cfgtimer,unsigned short inqw,unsigned short pagw)
{
	fprintf(file,"Extended parameters:\n");
	fprintf(file,"\tsrvclass: %04X\n",srvclass);
	fprintf(file,"\tdevclass: %04X\n",devclass);
	fprintf(file,"\tcfgtimer: %d\n",cfgtimer);
	fprintf(file,"\tinqwin: %04X\n",inqw);
	fprintf(file,"\tpagwin: %04X\n",pagw);
}
void rn41_PrintOtherParam(FILE *file,unsigned short sniff,unsigned short lp)
{
	fprintf(file,"Other parameters:\n");
	fprintf(file,"\tsniff: %04X\n",sniff);
	fprintf(file,"\tLowPower: %04X\n",lp);
}

/******************************************************************************
	function: rn41_Setup
*******************************************************************************	
	Initialises the RN41, performing:
	- Reset
	- Set-up of the RN41 parameters (name, class, etc)
	- Sets the low-power parameters: reduces the inquiry and scan window, low-power duty cycle 25%
	

	Parameters:
		file		-		Device where to send information about the setup (typically USB)
		filebt		-		Device to which the BT chip is connected to
		devname		-		Name that the Bluetooth chip must advertise

	Returns:
		-
******************************************************************************/
#warning Not clear if devname is input or output
void rn41_Setup(FILE *file,FILE *filebt,unsigned char *devname)
{
	/*
		Default:
			CmdRespn 02: 'SrvClass=0000'
			CmdRespn 03: 'DevClass=1F00'
	*/
	// Current parameters
	char mac[16],name[24],baud[8],mode,auth,pin[8];
	unsigned short srvclass,devclass,inqw,pagw,sniff,lowpower;
	unsigned char cfgtimer;
	// Desired parameters
	unsigned short p_srvclass=0b00001001000;			// Service: capturing, positioning
	unsigned short p_devclass=0b0011100011000;			// Device: wearable, wearable computer
	
	
	
	
	
#if (BLUETOOTH_POWERSAVE==0) || (BLUETOOTH_POWERSAVE==1)
	#if BLUETOOTH_POWERSAVE==0
	// Default (high power):
	unsigned short p_inqw = 0x100;					// Default: 0100
	unsigned short p_pagw = 0x100;					// Default: 0100
	unsigned short p_lowpower = 0x0000;				// Default: 0000
	#else
	// BLUETOOTH_POWERSAVE==1
	// Optimised (low power):
	unsigned short p_inqw = 0x20;
	unsigned short p_pagw = 0x20;						// Inquiry and page at 0x20 reduce power by -15mW. 
	unsigned short p_lowpower = 0x0301;					// Off 3 seconds, on 1 second. Reduces by -2mW over inquiry/page settings
	#endif
#else
	#error BLUETOOTH_POWERSAVE undefined or invalid
#endif
	
	// Various tries
	//unsigned short p_inqw = 0x12;
	//unsigned short p_pagw = 0x12;		
	//unsigned short p_lowpower = 0x0000;
	
	unsigned short p_sniff = 0x0000;					// Sniff deactivated (only useful during connection, but during streaming maximum throughput and minimum latency is desired)
	
	
	//unsigned char p_cfgtimer = 255;					// 255=unlimited local/remote; 0=local only when not connected. 255 leads to instability (reboots) under high throughput, also lowers bitrate
	unsigned char p_cfgtimer = 0;						// 255=unlimited local/remote; 0=local only when not connected. 255 leads to instability (reboots) under high throughput, also lowers bitrate
	char p_mode = 0;									// Slave
	char p_auth = 0;									// No authentication
	const char *p_pin="0000";							// PIN
	//const char *p_pin="1111";							// PIN
	//const char *p_name="BlueSense4";					// Name
	const char *p_name="BlueSens4";					// Name
	
	
	fprintf(file,"RN41 setup\n");
	

	// Communicate with the BT chip even if not connected remotely
	fbufferwhendisconnected(filebt,1);


	//fprintf_P(file,PSTR("RN41 reset and command mode\n"));
	rn41_Reset(file);		
	//fprintf_P(file,PSTR("cmd\n"));
	rn41_CmdEnter(file,filebt);
	//fprintf_P(file,PSTR("Status\n"));
	//rn41_GetBasic();
	//rn41_GetExtended();
	
	
	// Get basic params
	
	//fprintf_P(file,PSTR("Getting basic params\n"));
	rn41_GetBasicParam(filebt,mac,name,baud,&mode,&auth,pin);
	rn41_PrintBasicParam(file,mac,name,baud,mode,auth,pin);
	
	//fprintf_P(file,PSTR("Getting extended params\n"));	
	rn41_GetExtendedParam(filebt,&srvclass,&devclass,&cfgtimer,&inqw,&pagw);
	rn41_PrintExtParam(file,srvclass,devclass,cfgtimer,inqw,pagw);
	
	// Get other parameters
	rn41_GetOtherParam(filebt,&sniff,&lowpower);
	rn41_PrintOtherParam(file,sniff,lowpower);
	
	
	// Check if discrepancy between desired and target params
	char config = 0;
	if(p_srvclass != srvclass)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid srvclass\n");
#endif
	}
	if(p_devclass != devclass)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid devclass\n");
#endif

	}
	if(p_cfgtimer != cfgtimer)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid cfgtimer\n");
#endif

	}
	if(p_mode != mode)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid mode\n");
#endif

	}
	if(p_auth != auth)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid auth\n");
#endif

	}
	if(p_inqw != inqw)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid inqw\n");
#endif
	}
	if(p_pagw != pagw)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid pagw\n");
#endif
	}
	if(p_sniff != sniff)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid sniff\n");
#endif
	}
	if(p_lowpower != lowpower)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid lowpower\n");
#endif
	}
	if(strcmp(p_pin,pin)!=0)
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid pin\n");
#endif
	}
	if(strncmp(p_name,name,strlen(p_name))!=0 || strlen(name)!=strlen(p_name)+5)	// If the name does not start by p_name followed by 5 characters (-xxxx) then reconfig
	{
		config = 1;
#if RN41DEBUG==1
		fprintf(file,"Invalid name\n");
#endif
	}
	
	if(config==1)
	{
		fprintf(file,"RN41 configuration\n");
		fprintf(file,"Set authentication\n");
		rn41_SetAuthentication(file,filebt,p_auth);
		fprintf(file,"Set configuration timer\n");
		rn41_SetConfigTimer(file,filebt,p_cfgtimer);
		fprintf(file,"Set name\n");
		rn41_SetSerializedName(file,filebt,p_name);		
		fprintf(file,"Set PIN\n");
		rn41_SetPIN(file,filebt,p_pin);
		fprintf(file,"Set mode\n");
		rn41_SetMode(file,filebt,p_mode);						
		fprintf(file,"Set service class\n");
		rn41_SetServiceClass(file,filebt,p_srvclass);
		fprintf(file,"Set device class\n");
		rn41_SetDeviceClass(file,filebt,p_devclass);				
		
		
		fprintf(file,"Set inquiry window\n");
		rn41_SetI(file,filebt,p_inqw);	
		fprintf(file,"Set page window\n");
		rn41_SetJ(file,filebt,p_pagw);
		
		fprintf(file,"Set sniff\n");
		rn41_SetSniff(file,filebt,p_sniff);
		fprintf(file,"Set low power\n");
		rn41_SetLowpower(file,filebt,p_lowpower);
			
		
		fprintf(file,"RN41 reset and verify\n");
		rn41_Reset(file);		
		rn41_CmdEnter(file,filebt);
		rn41_GetBasicParam(filebt,mac,name,baud,&mode,&auth,pin);
		rn41_PrintBasicParam(file,mac,name,baud,mode,auth,pin);	
		rn41_GetExtendedParam(filebt,&srvclass,&devclass,&cfgtimer,&inqw,&pagw);
		rn41_PrintExtParam(file,srvclass,devclass,cfgtimer,inqw,pagw);
		rn41_GetOtherParam(filebt,&sniff,&lowpower);
		rn41_PrintOtherParam(file,sniff,lowpower);
	}
	else
		fprintf(file,"RN41 configuration OK\n");
		
	// Copy the device name
	if(devname)
	{
		for(int i=0;i<4;i++) devname[i] = name[strlen(p_name)+1+i];
		devname[4]=0;
	}
	
	//printf("Set pass data in cmd mode\n");
	//rn41_SetPassData(0);	
	
	if(0)
	{	// Increase the baud rate
		rn41_SetTempBaudrate(file,filebt,"230K");
		serial_uart_changespeed(230400);
	}
	if(0)
	{	// Increase the baud rate
		rn41_SetTempBaudrate(file,filebt,"460K");
		serial_uart_changespeed(460800);
	}
	if(1)
	{	// Increase the baud rate
		rn41_SetTempBaudrate(file,filebt,"921K");
		serial_uart_changespeed(921600);
	}

	rn41_CmdLeave(file,filebt);
	
	/*if(1)
	{
		//rn41_CmdEnter(file,filebt);
		
		//rn41_SetTempBaudrate("460K");
		//uart1_init(2,1);	// 460800bps  @ 11.06 Mhz
		
		rn41_SetTempBaudrate(file,filebt,"230K");
		uart1_init(2,0);	// 230400bps  @ 11.06 Mhz
		
		// Change the baudrate 
		//rn41_SetTempBaudrate("115K");
		
		//rn41_SetTempBaudrate("57.6");		
		
		
		//uart1_init(5,0);	// 115200bps  @ 11.06 Mhz
		//uart1_init(11,0);	// 57600bps @ 11.06 Mhz		
		
		
		
	}		*/
	
	// Here we should stop buffering if not remotely connected
	fbufferwhendisconnected(filebt,0);

}


