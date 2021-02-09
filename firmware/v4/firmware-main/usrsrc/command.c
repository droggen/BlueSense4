/*#include "cpu.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <util/atomic.h>*/
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "command.h"
#include "serial.h"


/*
	File: command
	
	Command line processing functions
*/

	

char CommandBuffer[COMMANDMAXSIZE];
unsigned char CommandBufferPtr=0;				// Index into CommandBuffer

const char CommandSuccess[] = "CMDOK\n";
const char CommandInvalid[] = "CMDINV\n";
const char CommandError[] = "CMDERR\n";
const char CommandVT100HighlightOk[] = "\x1b[32m";
const char CommandVT100HighlightErr[] = "\x1b[31m";
const char CommandVT100Normal[] = "\x1b[0m";

const COMMANDPARSER *CommandParsersCurrent;
unsigned char CommandParsersCurrentNum;

/******************************************************************************
	function: CommandProcessVT100Highlight
*******************************************************************************
	Parameters:
		mode		-		0=normal; 1=ok; 2=error
******************************************************************************/
void CommandProcessVT100Highlight(unsigned char mode)
{
	if(__mode_vt100)
	{
		switch(mode)
		{
		case 0:
			fputs(CommandVT100Normal,file_pri);
			break;
		case 1:
			fputs(CommandVT100HighlightOk,file_pri);
			break;
		default:
			fputs(CommandVT100HighlightErr,file_pri);
		}
	}
}

/******************************************************************************
	function: CommandProcessOK
*******************************************************************************
******************************************************************************/
void CommandProcessOK()
{
	CommandProcessVT100Highlight(1);
	fputs(CommandSuccess,file_pri);
	CommandProcessVT100Highlight(0);
}
/******************************************************************************
	function: CommandProcessInvalid
*******************************************************************************
******************************************************************************/
void CommandProcessInvalid()
{
	CommandProcessVT100Highlight(2);
	fputs(CommandInvalid,file_pri);
	CommandProcessVT100Highlight(0);
}

/******************************************************************************
	function: CommandProcessError
*******************************************************************************
******************************************************************************/
void CommandProcessError()
{
	CommandProcessVT100Highlight(2);
	fputs(CommandError,file_pri);
	CommandProcessVT100Highlight(0);
}



/******************************************************************************
	function: CommandProcess
*******************************************************************************	
	Reads file_pri while data available or a command is received, then execute the 
	command.
	
	Returns:
		0		-	Nothing to process anymore
		1		-	Something was processed	
******************************************************************************/
unsigned char CommandProcess(const COMMANDPARSER *CommandParsers,unsigned char CommandParsersNum)
{
	unsigned char rv,msgid;
	
	// Assign to current commands; used by help function
	CommandParsersCurrent = CommandParsers;
	CommandParsersCurrentNum = CommandParsersNum;
	
	rv = CommandGet(CommandParsers,CommandParsersNum,&msgid);
	//fprintf(file_pri,"CommandGet return: %d\n",rv);
	if(!rv)
	{
		return 0;
	}
	if(rv==3)
	{
		CommandProcessInvalid();
		return 1;
	}
	if(rv==1)
	{
		CommandProcessOK();
		return 1;
	}
	CommandProcessError();

	return 1;
}

/******************************************************************************
	function: _CommandPrint
*******************************************************************************	
	Pretty-print what is currently in the command buffer.
	
	Parameters:
		-
	
	Returns:
		-
******************************************************************************/
/*void _CommandPrint(void)
{
	fprintf(file_pri,"CommandBuffer %d/%d: ",CommandBufferPtr,COMMANDMAXSIZE);
	prettyprint_hexascii(file_pri,CommandBuffer,CommandBufferPtr);
}*/
/******************************************************************************
	function: CommandGet
*******************************************************************************	
	Reads file_pri until empty into the command buffer.
	
	Afterwards, process the command buffer and identifies which command to run and returns.
	If a message is received its ID is placed in msgid.
	
	The command separators are CR, LF and ;.
	
	If the command were to use a ; within it (e.g. to set a boot script), the 
	command must be enclosed in double quotation marks ".
	CR or LF delimiters are still detected within the quotation block.
	
	If a single quotation mark only is present the command is treated as quoted 
	from then until the next separator.
	
	A command can be a maximum of COMMANDMAXSIZE length; this function ensures
	there is never more than COMMANDMAXSIZE stored in buffers.
	
	Returns:
		0	-	no message
		1	-	message execution ok (message valid)
		2	-	message execution error (message valid)
		3	-	message invalid 
******************************************************************************/
unsigned char CommandGet(const COMMANDPARSER *CommandParsers,unsigned char CommandParsersNum,unsigned char *msgid)
{
	unsigned char rv;
	short c;
	static unsigned char quote=0;			// Indicates whether we are in a quotation mode, where the semicolon separator is not used to separate commands
	
	// Fast path: check if any key
	if(!fischar(file_pri))
		return 0;

	// If connected to a primary source, read that source until the source is empty or the command buffer is full
	// CommandBufferPtr indicates how many bytes are in the command buffer. The code below limits this to maximum COMMANDMAXSIZE.
	if(file_pri)
	{
		while(CommandBufferPtr<COMMANDMAXSIZE)
		{
			if((c=fgetc(file_pri)) == -1)
				break;
			CommandBuffer[CommandBufferPtr++] = c;
		}
	}
	
	// Fast path: command buffer empty
	if(CommandBufferPtr==0)
		return 0;
	
	//_CommandPrint();		// Debug
	
	// Process the command buffer: search for the first command delimiter (cr, lf or ;)
	for(unsigned char i=0;i<CommandBufferPtr;i++)
	{
		if(CommandBuffer[i]=='"')
		{
			//printf("Quote at %d\n",i);
			quote=1-quote;			// Toggle the quotation mode
			// the quotation byte must 'disappear' from the command string: shift left the string by 1 and decrease string length by 1
			memmove(&CommandBuffer[i],&CommandBuffer[i+1],COMMANDMAXSIZE-1-i);			// Shift left by 1
			CommandBufferPtr--;															// Decrease size of string by 1
			// Decrease index by one, so that the loop processes the new character that moved in the current index.
			i--;
			//_CommandPrint();		// Debug
			continue;
		}
		if(CommandBuffer[i]==10 || CommandBuffer[i]==13 || (CommandBuffer[i]==';' && quote==0) )
		{
			// Found a command delimiter
			//fprintf_P(file_pri,PSTR("Separator at %d\n"),i);		// Debug
			
			// Exit the quote mode. An unclosed quote in a command terminated by a CR or LF should not spread to the next command
			quote=0;
			
			if(i==0)
			{
				// The command is empty: store the return value 'no command'.
				// Do not return immediately, as the command buffer must be shifted by one, which is done below.
				rv=0;		
			}
			else
			{
				// The command is non empty. Null-terminate the command
				CommandBuffer[i] = 0;
				
				//fprintf_P(file_pri,PSTR("cmd string is '%s'\n"),CommandBuffer);			// Debug
				// Decode the command
				rv = CommandDecodeExec(CommandParsers,CommandParsersNum,CommandBuffer,i,msgid);
				//fprintf_P(file_pri,PSTR("rv is: %d\n"),rv);								// Debug
			}
			//_CommandPrint();			// Debug
			// Remove the processed command or character from the buffer by shifting the data out.
			memmove(CommandBuffer,CommandBuffer+1+i,COMMANDMAXSIZE-1-i);
			CommandBufferPtr-=1+i;
			//_CommandPrint();			// Debug
			return rv;
		}
	}
	// If we arrive at this stage: either the command separator has not been read yet, or the command buffer is full.
	if(CommandBufferPtr<COMMANDMAXSIZE)
	{
		// Do nothing if the command separator has not been read yet and the buffer is not full.
		//printf("no nr|lf yet\n");
		return 0;
	}
	// Here problem: the command buffer is full and no command separator has been received.
	// This should not happen, as the command buffer is large enough compared to the command buffer.
	// Likely gibberish is sent to the device.
	// Choose to clear the buffer.
	//fprintf_P(file_pri,PSTR("buff full clear\n"));
	
	// Clear buffer if buffer is full.
	CommandBufferPtr=0;
	// Return invalid message
	return 3;	
}



/******************************************************************************
	function: CommandDecodeExec
*******************************************************************************	
	Identify which parser is appropriate and call it for decoding and execution.


	Parameters:
		CommandParsers		-		Array of COMMANDPARSER
		CommandParsersNum	-		Total number of available commands
		buffer				-		Buffer containing the command; must be null terminated
		size				-		Size of the command in the buffer
		msgid				-		Returns the ID of the command, if a valid command was found

	Returns:
		0		-	No message
		1		-	Message execution ok (message valid)
		2		-	Message execution error (message valid)
		3		-	Message invalid 
		
	Parsers must return:
		0		-	Message execution ok (message valid)
		1		-	Message execution error (message valid)
		2		-	Message invalid 		
******************************************************************************/
unsigned char CommandDecodeExec(const COMMANDPARSER *CommandParsers,unsigned char CommandParsersNum,char *buffer,unsigned char size,unsigned char *msgid)
{
	unsigned char rv;
	
	if(size==0)
		return 0;
		
	buffer[size]=0;
	
	//fprintf_P(file_pri,PSTR("Got message of len %d: '%s'\n"),size,buffer);
	
	for(unsigned char i=0;i<CommandParsersNum;i++)
	{
		if(CommandParsers[i].cmd == buffer[0])
		{
			*msgid = i;
			rv = CommandParsers[i].parser(buffer+1,size-1);
			//fprintf_P(file_pri,PSTR("Parser %d: %d\n"),i,rv);
			return rv+1;
		}		
	}
	// Message invalid
	return 3;
}

/******************************************************************************
	Function: CommandSet
*******************************************************************************	
	Sets commands (one or more, delimited by any of the accepted separators) 
	in the command buffer.
	The existing content of the command buffer is erased. 
	
	This can be used to set a command script to execute e.g. on startup.

	Parameters:
		script		-		command/script to copy in buffer
		n			-		size of script; note that only up to COMMANDMAXSIZE
							bytes are copied in the command buffer.
******************************************************************************/
void CommandSet(char *script,unsigned char n)
{
	if(n>COMMANDMAXSIZE)
		n=COMMANDMAXSIZE;
	memcpy(CommandBuffer,script,n);
	CommandBufferPtr=n;
}










