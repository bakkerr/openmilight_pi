
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/select.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

#include <RF24/RF24.h>

#include "PL1167_nRF24.h"
#include "MiLightRadio.h"

RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_1MHZ);

PL1167_nRF24 prf(radio);
MiLightRadio mlr(prf);

static int debug = 0;

static int dupesPrinted = 0;

void receive()
{
  while(1){
    if(mlr.available()) {
      printf("\n");
      uint8_t packet[7];
      size_t packet_length = sizeof(packet);
      mlr.read(packet, packet_length);

      for(size_t i = 0; i < packet_length; i++) {
        printf("%02X ", packet[i]);
        fflush(stdout);
      }
    }

    int dupesReceived = mlr.dupesReceived();
    for (; dupesPrinted < dupesReceived; dupesPrinted++) {
      printf(".");
    }
    fflush(stdout);
  } 
}

void send(uint8_t data[8])
{
  static uint8_t seq = 1;

  uint8_t resends = data[7];
  if(data[6] == 0x00){
    data[6] = seq;
    seq++;
  }

  if(debug){
    printf("2.4GHz --> Sending: ");
    for (int i = 0; i < 7; i++) {
      printf("%02X ", data[i]);
    }
    printf(" [x%d]\n", resends);
  }

  mlr.write(data, 7);
    
  for(int i = 0; i < resends; i++){
    mlr.resend();
  }

}

void send(uint64_t v)
{
  uint8_t data[8];
  data[7] = (v >> (7*8)) & 0xFF;
  data[0] = (v >> (6*8)) & 0xFF;
  data[1] = (v >> (5*8)) & 0xFF;
  data[2] = (v >> (4*8)) & 0xFF;
  data[3] = (v >> (3*8)) & 0xFF;
  data[4] = (v >> (2*8)) & 0xFF;
  data[5] = (v >> (1*8)) & 0xFF;
  data[6] = (v >> (0*8)) & 0xFF;

  send(data);
}

void send(uint8_t color, uint8_t bright, uint8_t key,
          uint8_t remote = 0x01, uint8_t remote_prefix = 0x00,
	  uint8_t prefix = 0xB8, uint8_t seq = 0x00, uint8_t resends = 10)
{
  uint8_t data[8];
  data[0] = prefix;
  data[1] = remote_prefix;
  data[2] = remote;
  data[3] = color;
  data[4] = bright;
  data[5] = key;
  data[6] = seq;
  data[7] = resends;

  send(data);
}

double getTime()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + ((double)tv.tv_usec) * 1e-6;
}

void fade(uint8_t prefix, uint8_t rem_p, uint8_t remote, uint8_t color, uint8_t bright, uint8_t resends)
{
  while(1){
    color++;
    send(color, 0x00, 0x0F, 0x44, 0x00);
    usleep(20000);
  }
}

void strobe(uint8_t prefix, uint8_t rem_p, uint8_t remote, uint8_t bright, uint8_t resends)
{
  while(1){
    uint8_t color = rand() % 255;
    send(color, bright, 0x0F, remote, resends);
    usleep(50000);
  }
}

void udp_raw()
{
  int sockfd;
  struct sockaddr_in servaddr, cliaddr;
  char mesg[42];

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(8899);
  bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  while(1){
    socklen_t len = sizeof(cliaddr);
    int n = recvfrom(sockfd, mesg, 41, 0, (struct sockaddr *)&cliaddr, &len);

    mesg[n] = '\0';

    if(n == 8){
      if(debug){
        printf("UDP --> Received hex value\n");
      }
      uint8_t data[8];
      for(int i = 0; i < 8; i++){
        data[i] = (uint8_t)mesg[i];
      }
      send(data);
    }
    else {
      fprintf(stderr, "Message has invalid size %d (expecting 8)!\n", n);
    }
  }
}

void handle_milight_command(uint16_t remote, char *mesg, int n){

  int disco = -1;

  static uint8_t first = 1;
  static uint8_t data[8];
  if(first){
    data[0] = 0xB8;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x01;
    data[7] = 0x09;
    first = 0;
  }
  data[1] = (remote >> 8) & 0xFF;
  data[2] = remote & 0xFF;
 
  if(n == 2 || n == 3){
    if(debug){
      printf("UDP --> Received hex value (%02x, %02x, %02x)\n", mesg[0], mesg[1], mesg[2]);
    }

    data[0] = 0xB8;

    switch(mesg[0]){
      /* Color */
      case 0x40:
        disco = -1;
        data[5] = 0x0F;
        data[3] = (0xC8 - mesg[1] + 0x100) & 0xFF;
        data[0] = 0xB0;
        break;
      /* All Off */
      case 0x41:
        data[5] = 0x02;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* All On */
      case 0x42:
        data[4] = (data[4] & 0xF8);
        data[5] = 0x01;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Disco slower */
      case 0x43:
        data[5] = 0x0C;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Disco faster */
      case 0x44:
        data[5] = 0x0B;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Z1 On */
      case 0x45:
        data[4] = (data[4] & 0xF8) | 0x01;
        data[5] = 0x03;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Z1 Off */
      case 0x46:
        data[5] = 0x04;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Z2 On */
      case 0x47:
        data[4] = (data[4] & 0xF8) | 0x02;
        data[5] = 0x05;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Z2 Off */
      case 0x48:
        data[5] = 0x06;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Z3 On */
      case 0x49:
        data[4] = (data[4] & 0xF8) | 0x03;
        data[5] = 0x07;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Z3 Off */
      case 0x4A:
        data[5] = 0x08;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
       break;
      /* Z4 On */
      case 0x4B:
        data[4] = (data[4] & 0xF8) | 0x04;
        data[5] = 0x09;
         if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Z4 Off */
      case 0x4C:
        data[5] = 0x0A;
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break;
      /* Disco */
      case 0x4D:
        disco = (disco + 1) % 9;
        data[0] = 0xB0 + disco;
        data[5] = 0x0D;
        break;
      /* Brightness */
      case 0x4E:
        data[5] = 0x0E;
        data[4] = ((0x90 - (mesg[1] * 8) + 0x100) & 0xFF) | (data[4] & 0x07);
        if(disco > 0){
          data[0] = 0xB0 + disco;
        }
        break; 
      /* All White */
      case 0xC2:
        disco = -1;
        data[5] = 0x11;
        break;
      /* Z1 White. */
      case 0xC5:
        disco = -1;
        data[5] = 0x13;
        break;
      /* Z2 White. */
      case 0xC7:
        disco = -1;
        data[5] = 0x15;
        break;
      /* Z3 White. */
      case 0xC9:
        disco = -1;
        data[5] = 0x17;
        break;
      /* Z4 White. */
      case 0xCB:
        disco = -1;
        data[5] = 0x19;
        break;
      /* All Night */
      case 0xC1:
        disco = -1;
        data[5] = 0x12;
        break;
      /* Z1 Night */
      case 0xC6:
        disco = -1;
        data[5] = 0x14;
        break;
      /* Z2 Night */
      case 0xC8:
        disco = -1;
        data[5] = 0x16;
        break;
      /* Z3 Night */
      case 0xCA:
        disco = -1;
        data[5] = 0x18;
        break;
      /* Z4 Night */
      case 0xCC:
        disco = -1;
        data[5] = 0x1A;
        break;
      default:
        fprintf(stderr, "Unknown command %02x!\n", mesg[0]);
        return;
    } /* End case command */

    /* Send command */
    send(data);
    data[6]++;
  }
  else {
    fprintf(stderr, "Message has invalid size %d (expecting 2 or 3)!\n", n);
  } /* End message size check */

}

void udp_server(uint16_t remote)
{
  fd_set socks;
  int discover_fd, milight_fd, raw_fd;
  struct sockaddr_in discover_addr, milight_addr, raw_addr, cliaddr;
  char mesg[90];
  char reply[35] = "192.168.1.14,BABECAFEBABE,";

  discover_fd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&discover_addr, sizeof(discover_addr));
  discover_addr.sin_family = AF_INET;
  discover_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  discover_addr.sin_port = htons(48899);
  bind(discover_fd, (struct sockaddr *)&discover_addr, sizeof(discover_addr));

  milight_fd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&milight_addr, sizeof(milight_addr));
  milight_addr.sin_family = AF_INET;
  milight_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  milight_addr.sin_port = htons(8899);
  bind(milight_fd, (struct sockaddr *)&milight_addr, sizeof(milight_addr));

  raw_fd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&raw_addr, sizeof(raw_addr));
  raw_addr.sin_family = AF_INET;
  raw_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  raw_addr.sin_port = htons(8898);
  bind(raw_fd, (struct sockaddr *)&raw_addr, sizeof(raw_addr));

  /* 
   * The worst hack ever, but probably slightly better than hardcoded
   * Should move this to an ioctl command as there seems to be no better
   * of simpler option to retrieve the IP and MAC.
   */
  if(0){
    FILE *fd;
    size_t s1, s2;
    fd = popen("ifconfig | grep \"inet addr\" | cut -d ':' -f 2 | cut -d ' ' -f 1 | grep -v \"127.0.0.1\" | head -n 1 | tr -d [:space:]", "r");
    s1 = fread(reply, 1, 15, fd);
    reply[s1] = ',';
    s1++;
    fd = popen("ifconfig | grep \"HWaddr\" | cut -d ' ' -f 11 | tr -d [:space:] | tr -d ':' | tr [:lower:] [:upper:]", "r");
    s2 = fread(reply + s1, 1, 12 ,fd);
    reply[s1 + s2] = ',';
    s2++;
    reply[s1 + s2] = '\0';
  }

  if(debug){
    printf("Reply String: %s\n", reply);
    fflush(stdout);
  }

  while(1){
    socklen_t len = sizeof(cliaddr);

    FD_ZERO(&socks);
    FD_SET(discover_fd, &socks);
    FD_SET(milight_fd, &socks);
    FD_SET(raw_fd, &socks);

    if(select(FD_SETSIZE, &socks, NULL, NULL, NULL) >= 0){

      if(FD_ISSET(discover_fd, &socks)){
        int n = recvfrom(discover_fd, mesg, 90, 0, (struct sockaddr *)&cliaddr, &len);
        mesg[n] = '\0';
        
        if(debug){
          char str[INET_ADDRSTRLEN];
          long ip = cliaddr.sin_addr.s_addr;
          inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
          //printf("UDP --> Received discovery request (%s) from %s\n", mesg, str);  
          printf("From %s: ", str);
          for(size_t i = 0; i < strlen(mesg); i++){
            printf("%x ", mesg[i]);
          }
          printf("|\n");
        }

        if(!strncmp(mesg, "Link_Wi-Fi", 41)){
          sendto(discover_fd, reply, strlen(reply), 0, (struct sockaddr*)&cliaddr, len);
        }
      }
      
      if(FD_ISSET(milight_fd, &socks)){
        int n = recvfrom(milight_fd, mesg, 41, 0, (struct sockaddr *)&cliaddr, &len);

        mesg[n] = '\0';

        handle_milight_command(remote, mesg, n);
      } 

      if(FD_ISSET(raw_fd, &socks)){
        int n = recvfrom(raw_fd, mesg, 41, 0, (struct sockaddr *)&cliaddr, &len);

        mesg[n] = '\0';

        if(n == 8){
          if(debug){
            printf("UDP --> Received hex value\n");
          }
          uint8_t data[8];
          for(int i = 0; i < 8; i++){
            data[i] = (uint8_t)mesg[i];
          }
          send(data);
        }
        else {
          fprintf(stderr, "Message has invalid size %d (expecting 8)!\n", n);
        }
      }

    } /* End select */

  } /* While (1) */
  
}

void usage(const char *arg, const char *options){
  printf("\n");
  printf("Usage: sudo %s [%s]\n", arg, options);
  printf("\n");
  printf("   -h                       Show this help\n");
  printf("   -d                       Show debug output\n");
  printf("   -f                       Fade mode\n");
  printf("   -s                       Strobe mode\n");
  printf("   -l                       Listening (receiving) mode\n");
  printf("   -u                       UDP mode (raw)\n");
  printf("   -m                       UDP mode (milight)\n");
  printf("   -n NN<dec>               Resends of the same message\n");
  printf("   -p PP<hex>               Prefix value (Disco Mode)\n");
  printf("   -q RR<hex>               First byte of the remote\n");
  printf("   -r RR<hex>               Second byte of the remote\n");
  printf("   -c CC<hex>               Color byte\n");
  printf("   -b BB<hex>               Brightness byte\n");
  printf("   -k KK<hex>               Key byte\n");
  printf("   -v SS<hex>               Sequence byte\n");
  printf("   -w SSPPRRRRCCBBKKNN<hex> Complete message to send\n");
  printf("\n");
  printf(" Author: Roy Bakker (2015)\n");
  printf("\n");
  printf(" Inspired by sources from: - https://github.com/henryk/\n");
  printf("                           - http://torsten-traenkner.de/wissen/smarthome/openmilight.php\n");
  printf("\n");
}

void init_bulbs(int n, int rem)
{
  FILE *fp = fopen("config.txt", "r");
  size_t len = 0;
  ssize_t read;
  char *line = NULL;

  while((read = getline(&line, &len, fp)) != -1){
    //printf("Retrieved line of length %zu :\n", read);
    //printf("%s", line);
    line[4] = line[6] = '\0';
    uint16_t remote = strtol(line, NULL, 16);
    uint8_t zone = atoi(line + 5);
    for(int b = 7; b < read; b += 2)
    {
      line[b+1] = '\0';
      uint16_t bulb = atoi(line + b);
      printf("Configuring bulb %d for remote %x zone %d\n", bulb, remote, zone);
      char c;
      scanf("%c", &c);
      if(c != '\n') return;
      send(0x00, 0x08 + zone, 0x01 + zone * 2, remote & 0xFF, (remote >> 8) & 0xFF);
    }

  }

  free(line);

/*  for(int i = 0; i < n; i++){
    char c;
    printf("Reset Bulb %d: [y/n]", i);
    scanf("%c", &c);
    if(c == 'Y' || c == 'y'){
      send(0x00, 0x00, 0x03, rem+i, 0xCC, 0xB8);
    }
    while(scanf("%c", &c) < 0);
  }
*/

}

int main(int argc, char** argv)
{
  int do_receive = 0;
  int do_udp     = 0;
  int do_milight = 0;
  int do_strobe  = 0;
  int do_fade    = 0;
  int do_command = 0;
  int do_bind    = 0;

  uint8_t prefix   = 0xB8;
  uint8_t rem_p    = 0x00;
  uint8_t remote   = 0x01;
  uint8_t color    = 0x00;
  uint8_t bright   = 0x00;
  uint8_t key      = 0x01;
  uint8_t seq      = 0x00;
  uint8_t resends  =   10;

  uint64_t command = 0x00;

  int c;

  uint64_t tmp;

  const char *options = "hdfislumn:p:q:r:c:b:k:v:w:";

  while((c = getopt(argc, argv, options)) != -1){
    switch(c){
      case 'h':
        usage(argv[0], options);
        exit(0);
        break;
      case 'i':
        do_bind = 1;
        break;
      case 'd':
        debug = 1;
        break;
      case 'f':
        do_fade = 1;
       break;
      case 's':
        do_strobe = 1;
       break;
      case 'l':
        do_receive = 1;
       break;
      case 'u':
        do_udp = 1;
       break;
      case 'm':
        do_milight = 1;
       break;
      case 'n':
        tmp = strtoll(optarg, NULL, 10);
        resends = (uint8_t)tmp;
        break;
      case 'p':
        tmp = strtoll(optarg, NULL, 16);
        prefix = (uint8_t)tmp;
        break;
      case 'q':
        tmp = strtoll(optarg, NULL, 16);
        rem_p = (uint8_t)tmp;
        break;
      case 'r':
        tmp = strtoll(optarg, NULL, 16);
        remote = (uint8_t)tmp;
        break;
      case 'c':
        tmp = strtoll(optarg, NULL, 16);
        color = (uint8_t)tmp;
        break;
      case 'b':
        tmp = strtoll(optarg, NULL, 16);
        bright = (uint8_t)tmp;
        break;
      case 'k':
        tmp = strtoll(optarg, NULL, 16);
        key = (uint8_t)tmp;
        break;
      case 'v':
        tmp = strtoll(optarg, NULL, 16);
        seq = (uint8_t)tmp;
        break;
      case 'w':
        do_command = 1;
        command = strtoll(optarg, NULL, 16);
        break;
      case '?':
        if(optopt == 'n' || optopt == 'p' || optopt == 'q' || 
           optopt == 'r' || optopt == 'c' || optopt == 'b' ||
           optopt == 'k' || optopt == 'w'){
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        }
        else if(isprint(optopt)){
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        }
        else{
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        }
        return 1;
      default:
        fprintf(stderr, "Error parsing options");
        return -1;
    }
  }

  int ret = mlr.begin();

  if(ret < 0){
    fprintf(stderr, "Failed to open connection to the 2.4GHz module.\n");
    fprintf(stderr, "Make sure to run this program as root (sudo)\n\n");
    usage(argv[0], options);
    exit(-1);
  }

  if(do_receive){
    printf("Receiving mode, press Ctrl-C to end\n");
    receive();
  }
 
  if(do_udp){
    printf("UDP mode (raw), press Ctrl-C to end\n"); 
    udp_raw();
  } 

  if(do_milight){
    printf("UDP mode (milight), press Ctrl-C to end\n"); 
    udp_server(0x0044);
  }

  if(do_bind){
    init_bulbs(5, 0);
    return 0;
  }

  if(do_fade){
    printf("Fade mode, press Ctrl-C to end\n");
    fade(prefix, rem_p, remote, color, bright, resends);
  }

  if(do_strobe){
    printf("Strobe mode, press Ctrl-C to end\n");
    strobe(prefix, rem_p, remote, bright, resends);
  } 

  /*
  double from = getTime();

  for(int i = 0; i < 200; i++){
    send(color, 0x00, 0x0F);
    color += 64;
  }

  double time = getTime() - from;
  printf("Time: %f %f %f\n", time, time / 200, 1 / (time/200));
  */

  if(do_command){
    send(command);
  }
  else{
    send(color, bright, key, remote, rem_p, prefix, seq, resends);
  }

  return 0;
}
