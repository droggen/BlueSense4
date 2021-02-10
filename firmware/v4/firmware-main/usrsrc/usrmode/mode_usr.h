#ifndef MODE_USR_H_
#define MODE_USR_H_

// "User mode" for testing of functions


unsigned char CommandParserModeUser(char *buffer,unsigned char size);
void mode_user(void);
unsigned char CommandParserUSRTest1(char *buffer,unsigned char size);
unsigned char CommandParserUSRTest2(char *buffer,unsigned char size);

#endif
