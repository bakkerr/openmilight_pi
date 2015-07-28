/**
 */

#include <cstdlib>
#include <iostream>
#include <string.h>
#include <stdio.h>

#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>

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

void udp()
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

    if(n == 7){
      if(debug){
        printf("UDP --> Received hex value\n");
      }
      uint8_t data[7];
      for(int i = 0; i < 7; i++){
        data[i] = (uint8_t)mesg[i];
      }
      send(data);
    }
    else {
      fprintf(stderr, "Message has invalid size %d (expecting 7)!\n", n);
    }
  }
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
  printf("   -u                       UDP mode\n");
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

int main(int argc, char** argv)
{
  int do_receive = 0;
  int do_udp     = 0;
  int do_strobe  = 0;
  int do_fade    = 0;
  int do_command = 0;

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

  const char *options = "hdfslun:p:q:r:c:b:k:v:w:";

  while((c = getopt(argc, argv, options)) != -1){
    switch(c){
      case 'h':
        usage(argv[0], options);
        exit(0);
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
    printf("UDP mode, press Ctrl-C to end\n"); 
    udp();
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
