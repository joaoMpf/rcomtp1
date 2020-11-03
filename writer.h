#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define NUMMAX 3
#define TIMEOUT 3
#define sizePacketConst 100
#define bcc1ErrorPercentage 0
#define bcc2ErrorPercentage 0

#define FLAG 0x7E
#define A 0x03
#define SET_BCC (A ^ SET_C)
#define UA_BCC (A ^ UA_C)
#define UA_C 0x07
#define SET_C 0x03
#define C10 0x00
#define C11 0x40
#define C2Start 0x02
#define C2End 0x03
#define CRR0 0x05
#define CRR1 0x85
#define CREJ0 0x01
#define CREJ1 0x81
#define DISC 0x0B
#define headerC 0x01

#define Escape 0x7D
#define escapeFlag 0x5E
#define escapeEscape 0x5D

#define T1 0x00
#define T2 0x01
#define L1 0x04
#define L2 0x0B

/*--------------------------Data Link Layer --------------------------*/

// Start connection
int LLOPEN(int fd, int x);

// Stuffing and send packets
int LLWRITE(int fd, unsigned char *mensagem, int size);

// Stop connection
void LLCLOSE(int fd);

// recieve UA control word
void ua_sm(int *state, unsigned char *c);

// read control word
unsigned char read_cw(int fd);

// Send control word
void send_cw(int fd, unsigned char C);

// Calculate bcc
unsigned char make_bcc(unsigned char *mensagem, int size);

// Stuffing of bcc
unsigned char *stuffing(unsigned char BCC2, int *sizeBCC2);

// random noise on bcc1
unsigned char *noise_bcc1(unsigned char *packet, int sizePacket);

// random noise on bcc2
unsigned char *noise_bcc2(unsigned char *packet, int sizePacket);

/*-------------------------- Application Layer --------------------------*/

int main(int argc, char **argv);

// Create START and END packets
unsigned char *control_I(unsigned char state, off_t sizeFile, unsigned char *fileName, int sizeOfFilename, int *sizeControlPackageI);

// Open file
unsigned char *open_file(unsigned char *fileName, off_t *sizeFile);

//Add header to packet
unsigned char *add_header(unsigned char *mensagem, off_t sizeFile, int *sizePacket);

// Split data into packets
unsigned char *split_data(unsigned char *mensagem, off_t *indice, int *sizePacket, off_t sizeFile);
