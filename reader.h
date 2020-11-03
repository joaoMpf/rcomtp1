#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A 0x03
#define SET_BCC A ^ SET_C
#define UA_BCC A ^ UA_C
#define SET_C 0x03
#define UA_C 0x07
#define C10 0x00
#define C11 0x40
#define RR_C0 0x05
#define RR_C1 0x85
#define REJ_C0 0x01
#define REJ_C1 0x81
#define DISC_C 0x0B
#define C2End 0x03

#define Escape 0x7D
#define escapeFlag 0x5E
#define escapeEscape 0x5D

/*-------------------------- Data --------------------------*/

// Start connection
void LLOPEN(int fd);

// Read packets and destuffing
unsigned char *LLREAD(int fd, int *sizeMessage);

// Stop connection
void LLCLOSE(int fd);

// Read control word
int read_cw(int fd, unsigned char C);

//send control word
void send_cw(int fd, unsigned char C);

//check bcc integrity
int check_bcc(unsigned char *message, int sizeMessage);

/*-------------------------- Application Layer --------------------------*/

int main(int argc, char **argv);

// Get file name from START packet
unsigned char *file_name_start(unsigned char *start);

// Get size name from START packet
off_t file_size_start(unsigned char *start);

// Remove header from packet
unsigned char *remove_header(unsigned char *toRemove, int sizeToRemove, int *sizeRemoved);

// Check for recieved END packet
int check_end(unsigned char *start, int sizeStart, unsigned char *end, int sizeEnd);

// Create file with recieved data
void create_file(unsigned char *mensagem, off_t *sizeFile, unsigned char *filename);
