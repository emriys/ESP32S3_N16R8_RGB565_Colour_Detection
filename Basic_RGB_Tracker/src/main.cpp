#include <Arduino.h>
#include "strict_RGB.h"

void setup() {
  Serial.begin(115200);
  initVision();
}

void loop() {
  runVision();
}