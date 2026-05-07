#define DATA  8   // DS (SER) pini
#define LATCH 9   // ST_CP (RCLK) pini
#define CLOCK 10   // SH_CP (SRCLK) pini
#include "TrafficMonitor.h"
#include "sistem.h"
#include "blinkAll.h"
#include "animations.h"





void setup() {
  Serial.begin(9600);
  beginTrafficMonitor();
  grup2.begin(9600);
  pinMode(DATA, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  bekle(2000);
  
  mode0(100);
}



void loop() {

}

