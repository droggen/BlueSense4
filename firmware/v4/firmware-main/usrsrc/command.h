#ifndef __COMMAND_H
#define __COMMAND_H


#define COMMANDMAXSIZE 96



typedef struct {
	unsigned char cmd;
	unsigned char (*parser)(char *,unsigned char size);
	const char *help;
} COMMANDPARSER;

extern const COMMANDPARSER *CommandParsersCurrent;
extern unsigned char CommandParsersCurrentNum;


//void _CommandPrint(void);
unsigned char CommandProcess(const COMMANDPARSER *CommandParsers,unsigned char CommandParsersNum);
unsigned char CommandGet(const COMMANDPARSER *CommandParsers,unsigned char CommandParsersNum,unsigned char *msgid);
unsigned char CommandDecodeExec(const COMMANDPARSER *CommandParsers,unsigned char CommandParsersNum,char *buffer,unsigned char size,unsigned char *msgid);
void CommandSet(char *script,unsigned char n);

#endif
