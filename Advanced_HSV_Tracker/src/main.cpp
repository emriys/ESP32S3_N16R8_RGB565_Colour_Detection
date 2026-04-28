#include <Arduino.h>
#include "hue_RGB.h"

void setup() {
  Serial.begin(115200);
  initHueVision();
}

void loop() {
  runHueVision();
}