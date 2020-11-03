/*Non-Canonical Input Processing*/
#include "reader.h"

int esp = 0;
struct termios oldtio, newtio;

int main(int argc, char **argv)
{
  int fd;
  int msg_size = 0;
  unsigned char *msg_final;
  int start_size = 0;
  unsigned char *start;
  off_t final_size = 0;
  unsigned char *buffer;
  off_t index = 0;

  if ((argc < 2) ||
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

  LLOPEN(fd);
  start = LLREAD(fd, &start_size);

  unsigned char *name = "pinguim2.gif";
  final_size = file_size_start(start);

  buffer = (unsigned char *)malloc(final_size);

  while (TRUE)
  {
    msg_final = LLREAD(fd, &msg_size);
    if (msg_size == 0)
      continue;
    if (check_end(start, start_size, msg_final, msg_size))
    {
      printf("End message received\n");
      break;
    }

    int sizeWithoutHeader = 0;

    msg_final = remove_header(msg_final, msg_size, &sizeWithoutHeader);

    memcpy(buffer + index, msg_final, sizeWithoutHeader);
    index += sizeWithoutHeader;
  }

  printf("data: \n");
  int i = 0;
  for (; i < final_size; i++)
  {
    printf("%x", buffer[i]);
  }

  create_file(buffer, &final_size, name);

  LLCLOSE(fd);

  sleep(1);

  close(fd);
  return 0;
}

void LLCLOSE(int fd)
{
  read_cw(fd, DISC_C);
  printf("DISC recieved\n");
  send_cw(fd, DISC_C);
  printf("DISC sent\n");
  read_cw(fd, UA_C);
  printf("UA recieved\n");
  printf("Disconnected\n");

  tcsetattr(fd, TCSANOW, &oldtio);
}

void LLOPEN(int fd)
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

  newtio.c_cc[VTIME] = 1; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 0;  /* blocking read until 5 chars received */

  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
  */

  tcflush(fd, TCIOFLUSH);

  printf("New termios structure set\n");

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  if (read_cw(fd, SET_C))
  {
    printf("SET recieved\n");
    send_cw(fd, UA_C);
    printf("UA sent\n");
  }
}

unsigned char *LLREAD(int fd, int *msg_size)
{
  unsigned char *data = (unsigned char *)malloc(0);
  *msg_size = 0;
  unsigned char c_read;
  int trama = 0;
  int sending = FALSE;
  unsigned char c;
  int state = 0;

  while (state != 6)
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
      if (c == C10)
      {
        trama = 0;
        c_read = c;
        state = 3;
      }
      else if (c == C11)
      {
        trama = 1;
        c_read = c;
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
      if (c == (A ^ c_read))
        state = 4;
      else
        state = 0;
      break;
    case 4:
      if (c == FLAG)
      {
        if (check_bcc(data, *msg_size))
        {
          if (trama == 0)
            send_cw(fd, RR_C1);
          else
            send_cw(fd, RR_C0);

          state = 6;
          sending = TRUE;
          printf(" RR, T: %d\n", trama);
        }
        else
        {
          if (trama == 0)
            send_cw(fd, REJ_C1);
          else
            send_cw(fd, REJ_C0);
          state = 6;
          sending = FALSE;
          printf("REJ, T: %d\n", trama);
        }
      }
      else if (c == Escape)
      {
        state = 5;
      }
      else
      {
        data = (unsigned char *)realloc(data, ++(*msg_size));
        data[*msg_size - 1] = c;
      }
      break;
    case 5:
      if (c == escapeFlag)
      {
        data = (unsigned char *)realloc(data, ++(*msg_size));
        data[*msg_size - 1] = FLAG;
      }
      else
      {
        if (c == escapeEscape)
        {
          data = (unsigned char *)realloc(data, ++(*msg_size));
          data[*msg_size - 1] = Escape;
        }
        else
        {
          perror("Non valid character after escape character");
          exit(-1);
        }
      }
      state = 4;
      break;
    }
  }
  printf("Message size: %d\n", *msg_size);
  data = (unsigned char *)realloc(data, *msg_size - 1);

  *msg_size = *msg_size - 1;
  if (sending)
  {
    if (trama == esp)
    {
      esp ^= 1;
    }
    else
      *msg_size = 0;
  }
  else
    *msg_size = 0;
  return data;
}

int check_bcc(unsigned char *message, int sizeMessage)
{
  int i = 1;
  unsigned char BCC2 = message[0];
  for (; i < sizeMessage - 1; i++)
  {
    BCC2 ^= message[i];
  }
  if (BCC2 == message[sizeMessage - 1])
  {
    return TRUE;
  }
  else
    return FALSE;
}

void send_cw(int fd, unsigned char C)
{
  unsigned char message[5];
  message[0] = FLAG;
  message[1] = A;
  message[2] = C;
  message[3] = message[1] ^ message[2];
  message[4] = FLAG;
  write(fd, message, 5);
}

int read_cw(int fd, unsigned char C)
{
  int state = 0;
  unsigned char c;

  while (state != 5)
  {
    read(fd, &c, 1);
    switch (state)
    {
    //recebe flag
    case 0:
      if (c == FLAG)
        state = 1;
      break;
    //recebe A
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
    //recebe C
    case 2:
      if (c == C)
        state = 3;
      else
      {
        if (c == FLAG)
          state = 1;
        else
          state = 0;
      }
      break;
    //recebe BCC
    case 3:
      if (c == (A ^ C))
        state = 4;
      else
        state = 0;
      break;
    //recebe FLAG final
    case 4:
      if (c == FLAG)
      {
        //printf("Recebeu mensagem\n");
        state = 5;
      }
      else
        state = 0;
      break;
    }
  }
  return TRUE;
}

unsigned char *remove_header(unsigned char *toRemove, int sizeToRemove, int *sizeRemoved)
{
  int i = 0;
  int j = 4;
  unsigned char *messageRemovedHeader = (unsigned char *)malloc(sizeToRemove - 4);
  for (; i < sizeToRemove; i++, j++)
  {
    messageRemovedHeader[i] = toRemove[j];
  }
  *sizeRemoved = sizeToRemove - 4;
  return messageRemovedHeader;
}

int check_end(unsigned char *start, int sizeStart, unsigned char *end, int sizeEnd)
{
  int s = 1;
  int e = 1;
  if (sizeStart != sizeEnd)
    return FALSE;
  else
  {
    if (end[0] == C2End)
    {
      for (; s < sizeStart; s++, e++)
      {
        if (start[s] != end[e])
          return FALSE;
      }
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }
}

off_t file_size_start(unsigned char *start)
{
  return (start[3] << 24) | (start[4] << 16) | (start[5] << 8) | (start[6]);
}

unsigned char *file_name_start(unsigned char *start)
{

  int L2 = (int)start[8];
  unsigned char *name = (unsigned char *)malloc(L2 + 1);

  int i;
  for (i = 0; i < L2; i++)
  {
    name[i] = start[9 + i];
  }

  name[L2] = '\0';
  return name;
}

void create_file(unsigned char *mensagem, off_t *sizeFile, unsigned char filename[])
{
  FILE *file = fopen((char *)filename, "wb+");
  fwrite((void *)mensagem, 1, *sizeFile, file);
  printf("%zd\n", *sizeFile);
  printf("New file created\n");
  fclose(file);
}
