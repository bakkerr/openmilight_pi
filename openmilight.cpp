/**
 * On a Raspberry Pi 2 compile with:
 *
 * g++ -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv7-a -mtune=arm1176jzf-s -I/usr/local/include -L/usr/local/lib -lrf24-bcm PL1167_nRF24.cpp MiLightRadio.cpp openmili.cpp -o openmilight
 *
 * for receiver mode run with:
 * sudo ./openmilight
 *
 * for sender mode run with:
 * sudo ./openmilight "B0 F2 EA 6D B0 02 f0"
 */

#include <cstdlib>
#include <iostream>
#include <string.h>
#include <stdio.h>

#include <sys/time.h>

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
    if (mlr.available()) {
      printf("\n");
      uint8_t packet[7];
      size_t packet_length = sizeof(packet);
      mlr.read(packet, packet_length);

      for (int i = 0; i < packet_length; i++) {
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

void send(uint8_t color, uint8_t bright, uint8_t key,
          uint8_t remote = 0x01, uint8_t remote_prefix = 0x00,
          uint8_t prefix = 0xB8)
{

    static uint8_t seq = 1;

    uint8_t data[7];
    data[0] = prefix;
    data[1] = remote_prefix;
    data[2] = remote;
    data[3] = color;
    data[4] = bright;
    data[5] = key;
    data[6] = seq;
    
    if(debug){
      printf("Sending: ");
      for (int i = 0; i < 7; i++) {
        printf("%02X ", data[i]);
      }
      printf("\n");
    }

    mlr.write(data, sizeof(data));
    
    for(int i = 0; i < 10; i++){
      mlr.resend();
    }

    seq++;
}

double getTime()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + ((double)tv.tv_usec) * 1e-6;
}

int main(int argc, char** argv)
{
  mlr.begin();

  int c;
  
  uint8_t prefix = 0xB8;
  uint8_t rem_p  = 0x00;
  uint8_t remote = 0x01;
  uint8_t color  = 0x01;
  uint8_t bright = 0x01;
  uint8_t key    = 0x01;
  uint64_t tmp;

  while((c = getopt(argc, argv, "dlq:r:c:b:k:w:")) != -1){
    switch(c){
      case 'd':
        debug = 1;
        break;
      case 'l':
        printf("Receiving mode, press Ctrl-C to end\n");
        receive();
        exit(0);
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
      case 'w':
        tmp = strtoll(optarg, NULL, 16);
        rem_p =  (tmp >> (4*8)) & 0xFF;
        remote = (tmp >> (3*8)) & 0xFF;
        color  = (tmp >> (2*8)) & 0xFF;
        bright = (tmp >> (1*8)) & 0xFF;
        key    = (tmp >> (0*8)) & 0xFF;
        break;
      case '?':
        if(optopt == 'q' || optopt == 'r' || optopt == 'c' || 
           optopt == 'b' || optopt == 'k' || optopt == 'w'){
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        }
        else if(isprint (optopt)){
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

  /*
  double from = getTime();

  for(int i = 0; i < 200; i++){
    send(color, 0x00, 0x0F);
    color += 64;
  }

  double time = getTime() - from;
  printf("Time: %f %f %f\n", time, time / 200, 1 / (time/200));
  */

  send(color, bright, key, remote, rem_p, prefix);

  return 0;
}
