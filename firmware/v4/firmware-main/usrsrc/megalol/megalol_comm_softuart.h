#ifndef __MEGALOL_COMM_SOFTUART_H
#define __MEGALOL_COMM_SOFTUART_H


// The buffersize must hold at least enough samples to reconstruct one byte. Internally a buffer twice the size is used to ensure a complete byte can be decoded, even if the frame does not allign with the byte.
// At 500KHz sample rate and 115.2kbps, 512 holds about 100 bytes. However, if the bitrate is 100x lower (1.2kbps) then this buffer is not large enough.
#define COMM_SOFTUART_BINARYINBUFFERSIZE 512
//#define COMM_SOFTUART_BUFFERSIZE 256
//#define COMM_SOFTUART_BINARYINBUFFERSIZE 1024

// Circular buffer size
#define COMM_SOFTUART_RX_BUFFERSIZE 1024

extern unsigned char testbinarysignal_0[];

FILE *comm_softuart_open(int sr,int br);
void comm_softuart_decode_init(int sr,int br);
void _comm_softuart_decode_addrxbyte(unsigned char byte);
unsigned _comm_softuart_decode_buffer();
unsigned comm_softuart_decode_frame(unsigned char *frame,unsigned len);
void comm_softuart_test_decode();
void comm_softuart_test_decode_file();
void comm_softuart_bench_decode();

void comm_softuart_printstat();
unsigned comm_softuart_level();
unsigned comm_softuart_get(char * restrict buffer,unsigned n);

ssize_t _comm_softuart_cookieread_int(void *__cookie, char *__buf, size_t __n);

#endif
