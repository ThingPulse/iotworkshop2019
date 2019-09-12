#include <Arduino.h>
#include <Wire.h>
#include "SSD1306Wire.h"

#define SSD1306_ADDRESS 0x3c
#define I2C_SDA 21
#define I2C_SCL 22

SSD1306Wire display(SSD1306_ADDRESS, I2C_SDA, I2C_SCL);

void setup() {
  Serial.begin(115200);

  log_i("Initializing display");

  display.init();
  display.flipScreenVertically();

  log_i("Writing to display buffer");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Hello world");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 10, "Hello world");
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 26, "Hello world");

  log_i("Flushing display buffer");
  display.display();
}

void loop() {
  // put your main code here, to run repeatedly:
}