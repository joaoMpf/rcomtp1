/*Non-Canonical Input Processing*/
#include "writer.h"

int alarm_counter = 0;
int alarm_flag = FALSE;
int trama = 0;
int stopping = FALSE;
unsigned char packet_counter = 0;
int tramas_counter = 0;

struct termios oldtio, newtio;

void alarmHandler()
{
  printf("Alarm=%d\n", alarm_counter + 1);
  alarm_flag = TRUE;
  alarm_counter++;
}

int main(int argc, char **argv)
{
  int fd;
  off_t file_size; //tamanho do ficheiro em bytes
  off_t index = 0;
  int control = 0;

  if ((argc < 3) ||
      ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
       (strcmp("/dev/ttyS11", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }
  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */
  fd = open(argv[1], O_RDWR | O_NOCTTY);
  if (fd < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  // install alarm
  (void)signal(SIGALRM, alarmHandler);

  unsigned char *msg = open_file((unsigned char *)argv[2], &file_size);

  //starts connection
  if (!LLOPEN(fd, 0))
  {
    return -1;
  }

  int name_size = strlen(argv[2]);
  unsigned char *name = (unsigned char *)malloc(name_size);
  name = (unsigned char *)argv[2];
  unsigned char *start = control_I(C2Start, file_size, name, name_size, &control);

  LLWRITE(fd, start, control);
  printf("START sent\n");

  int sizePacket = sizePacketConst;

  while (sizePacket == sizePacketConst && index < file_size)
  {
    //split message in packets
    unsigned char *packet = split_data(msg, &index, &sizePacket, file_size);
    printf("packer nr %d sent\n", tramas_counter);
    // Add header
    int header_size = sizePacket;
    unsigned char *msg_header = add_header(packet, file_size, &header_size);
    //send data
    if (!LLWRITE(fd, msg_header, header_size))
    {
      printf("alarm limit !\n");
      return -1;
    }
  }

  unsigned char *end = control_I(C2End, file_size, name, name_size, &control);
  LLWRITE(fd, end, control);
  printf("END sent\n");

  LLCLOSE(fd);

  close(fd);
  return 0;
}

unsigned char *add_header(unsigned char *data, off_t file_size, int *packet_size)
{
  unsigned char *msg = (unsigned char *)malloc(file_size + 4);
  msg[0] = headerC;
  msg[1] = packet_counter % 255;
  msg[2] = (int)file_size / 256;
  msg[3] = (int)file_size % 256;
  memcpy(msg + 4, data, *packet_size);
  *packet_size += 4;
  packet_counter++;
  tramas_counter++;
  return msg;
}

unsigned char *split_data(unsigned char *data, off_t *index, int *packet_size, off_t file_size)
{
  unsigned char *packet;
  int i = 0;
  off_t j = *index;
  if (*index + *packet_size > file_size)
  {
    *packet_size = file_size - *index;
  }
  packet = (unsigned char *)malloc(*packet_size);
  for (; i < *packet_size; i++, j++)
  {
    packet[i] = data[j];
  }
  *index = j;
  return packet;
}

void ua_sm(int *state, unsigned char *c)
{

  switch (*state)
  {

  case 0:
    if (*c == FLAG)
      *state = 1;
    break;

  case 1:
    if (*c == A)
      *state = 2;
    else
    {
      if (*c == FLAG)
        *state = 1;
      else
        *state = 0;
    }
    break;

  case 2:
    if (*c == UA_C)
      *state = 3;
    else
    {
      if (*c == FLAG)
        *state = 1;
      else
        *state = 0;
    }
    break;

  case 3:
    if (*c == UA_BCC)
      *state = 4;
    else
      *state = 0;
    break;

  case 4:
    if (*c == FLAG)
    {
      stopping = TRUE;
      alarm(0);
      printf("UA recieved\n");
    }
    else
      *state = 0;
    break;
  }
}

int LLOPEN(int fd, int x)
{

  if (tcgetattr(fd, &oldtio) == -1)
  { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 1; /* inter-unsigned character timer unused */
  newtio.c_cc[VMIN] = 0;  /* blocking read until 5 unsigned chars received */

  /*
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a

  leitura do(s) pr�ximo(s) caracter(es)

  */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");

  unsigned char c;
  do
  {
    send_cw(fd, SET_C);
    alarm(TIMEOUT);
    alarm_flag = 0;
    int state = 0;

    while (!stopping && !alarm_flag)
    {
      read(fd, &c, 1);
      ua_sm(&state, &c);
    }
  } while (alarm_flag && alarm_counter < NUMMAX);
  printf("flag alarm %d\n", alarm_flag);
  printf("soma %d\n", alarm_counter);
  if (alarm_flag && alarm_counter == 3)
  {
    return FALSE;
  }
  else
  {
    alarm_flag = FALSE;
    alarm_counter = 0;
    return TRUE;
  }
}

int LLWRITE(int fd, unsigned char *data, int size)
{
  unsigned char bcc;
  unsigned char *stuffed = (unsigned char *)malloc(sizeof(unsigned char));
  unsigned char *msg_final = (unsigned char *)malloc((size + 6) * sizeof(unsigned char));
  int msg_size = size + 6;
  int bcc_size = 1;
  bcc = make_bcc(data, size);
  stuffed = stuffing(bcc, &bcc_size);
  int reject = FALSE;

  msg_final[0] = FLAG;
  msg_final[1] = A;
  if (trama == 0)
  {
    msg_final[2] = C10;
  }
  else
  {
    msg_final[2] = C11;
  }
  msg_final[3] = (msg_final[1] ^ msg_final[2]);

  int i = 0;
  int j = 4;
  for (; i < size; i++)
  {
    if (data[i] == FLAG)
    {
      msg_final = (unsigned char *)realloc(msg_final, ++msg_size);
      msg_final[j] = Escape;
      msg_final[j + 1] = escapeFlag;
      j = j + 2;
    }
    else
    {
      if (data[i] == Escape)
      {
        msg_final = (unsigned char *)realloc(msg_final, ++msg_size);
        msg_final[j] = Escape;
        msg_final[j + 1] = escapeEscape;
        j = j + 2;
      }
      else
      {
        msg_final[j] = data[i];
        j++;
      }
    }
  }

  if (bcc_size == 1)
    msg_final[j] = bcc;
  else
  {
    msg_final = (unsigned char *)realloc(msg_final, ++msg_size);
    msg_final[j] = stuffed[0];
    msg_final[j + 1] = stuffed[1];
    j++;
  }
  msg_final[j + 1] = FLAG;

  //mandar mensagem
  do
  {

    unsigned char *copia;
    copia = noise_bcc1(msg_final, msg_size); //altera bcc1
    copia = noise_bcc2(copia, msg_size);     //altera bcc2
    write(fd, copia, msg_size);

    alarm_flag = FALSE;
    alarm(TIMEOUT);
    unsigned char C = read_cw(fd);
    if ((C == CRR1 && trama == 0) || (C == CRR0 && trama == 1))
    {
      printf(" rr %x, trama = %d\n", C, trama);
      reject = FALSE;
      alarm_counter = 0;
      trama ^= 1;
      alarm(0);
    }
    else
    {
      if (C == CREJ1 || C == CREJ0)
      {
        reject = TRUE;
        printf(" rej %x, trama=%d\n", C, trama);
        alarm(0);
      }
    }
  } while ((alarm_flag && alarm_counter < NUMMAX) || reject);
  if (alarm_counter >= NUMMAX)
    return FALSE;
  else
    return TRUE;
}

void send_cw(int fd, unsigned char C)
{
  unsigned char msg[5];
  msg[0] = FLAG;
  msg[1] = A;
  msg[2] = C;
  msg[3] = msg[1] ^ msg[2];
  msg[4] = FLAG;
  write(fd, msg, 5);
}

unsigned char read_cw(int fd)
{
  int state = 0;
  unsigned char c;
  unsigned char C;

  while (!alarm_flag && state != 5)
  {
    read(fd, &c, 1);
    switch (state)
    {
    case 0:
      if (c == FLAG)
        state = 1;
      break;
    case 1:
      if (c == A)
        state = 2;
      else
      {
        if (c == FLAG)
          state = 1;
        else
          state = 0;
      }
      break;

    case 2:
      if (c == CRR0 || c == CRR1 || c == CREJ0 || c == CREJ1 || c == DISC)
      {
        C = c;
        state = 3;
      }
      else
      {
        if (c == FLAG)
          state = 1;
        else
          state = 0;
      }
      break;
    case 3:
      if (c == (A ^ C))
        state = 4;
      else
        state = 0;
      break;
    case 4:
      if (c == FLAG)
      {
        alarm(0);
        state = 5;
        return C;
      }
      else
        state = 0;
      break;
    }
  }
  return 0xFF;
}

unsigned char make_bcc(unsigned char *data, int size)
{
  unsigned char bcc = data[0];
  int i;
  for (i = 1; i < size; i++)
  {
    bcc ^= data[i];
  }
  return bcc;
}

unsigned char *stuffing(unsigned char bcc, int *size)
{
  unsigned char *stuffed;
  if (bcc == FLAG)
  {
    stuffed = (unsigned char *)malloc(2 * sizeof(unsigned char *));
    stuffed[0] = Escape;
    stuffed[1] = escapeFlag;
    (*size)++;
  }
  else
  {
    if (bcc == Escape)
    {
      stuffed = (unsigned char *)malloc(2 * sizeof(unsigned char *));
      stuffed[0] = Escape;
      stuffed[1] = escapeEscape;
      (*size)++;
    }
  }

  return stuffed;
}

unsigned char *open_file(unsigned char *name, off_t *file_size)
{
  FILE *file;
  struct stat meta;
  unsigned char *data;

  if ((file = fopen((char *)name, "rb")) == NULL)
  {
    perror("error opening file!");
    exit(-1);
  }
  stat((char *)name, &meta);
  (*file_size) = meta.st_size;
  printf("This file has %ld bytes \n", *file_size);

  data = (unsigned char *)malloc(*file_size);

  fread(data, sizeof(unsigned char), *file_size, file);
  return data;
}

unsigned char *control_I(unsigned char state, off_t file_size, unsigned char *name, int name_size, int *control_size)
{
  *control_size = 9 * sizeof(unsigned char) + name_size;
  unsigned char *packet = (unsigned char *)malloc(*control_size);

  if (state == C2Start)
    packet[0] = C2Start;
  else
    packet[0] = C2End;
  packet[1] = T1;
  packet[2] = L1;
  packet[3] = (file_size >> 24) & 0xFF;
  packet[4] = (file_size >> 16) & 0xFF;
  packet[5] = (file_size >> 8) & 0xFF;
  packet[6] = file_size & 0xFF;
  packet[7] = T2;
  packet[8] = name_size;
  int k = 0;
  for (; k < name_size; k++)
  {
    packet[9 + k] = name[k];
  }
  return packet;
}

void LLCLOSE(int fd)
{
  send_cw(fd, DISC);
  printf("DISCK sent\n");
  unsigned char C;
  //espera ler o DISC
  C = read_cw(fd);
  while (C != DISC)
  {
    C = read_cw(fd);
  }
  printf("DISC recieved\n");
  send_cw(fd, UA_C);
  printf("UA sentl\n");
  printf("Disconnected\n");

  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }
}

unsigned char *noise_bcc2(unsigned char *packet, int packet_size)
{
  unsigned char *copia = (unsigned char *)malloc(packet_size);
  memcpy(copia, packet, packet_size);
  int r = (rand() % 100) + 1;
  if (r <= bcc2ErrorPercentage)
  {
    int i = (rand() % (packet_size - 5)) + 4;
    unsigned char randomLetter = (unsigned char)('A' + (rand() % 26));
    copia[i] = randomLetter;
    printf("noise on BCC2\n");
  }
  return copia;
}

unsigned char *noise_bcc1(unsigned char *packet, int packet_size)
{
  unsigned char *copia = (unsigned char *)malloc(packet_size);
  memcpy(copia, packet, packet_size);
  int r = (rand() % 100) + 1;
  if (r <= bcc1ErrorPercentage)
  {
    int i = (rand() % 3) + 1;
    unsigned char randomLetter = (unsigned char)('A' + (rand() % 26));
    copia[i] = randomLetter;
    printf("Noise on BCC1\n");
  }
  return copia;
}
