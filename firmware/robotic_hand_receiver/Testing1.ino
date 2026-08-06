/*
 * Wireless Robotic Glove - TRANSMITTER (Final)
 * Arduino Nano + CD74HC4051 + Flex Sensor (Index Finger)
 *
 * Wiring:
 * --- CD74HC4051 Multiplexer ---
 * VCC      → 5V
 * GND      → GND
 * VEE      → GND
 * E        → GND
 * COM/OUT  → A0
 * S0       → D4
 * S1       → D5
 * S2       → D6
 *
 * --- Flex Sensor (MUX Channel Y0) ---
 * One end   → 5V
 * Other end → MUX Y0 + 47kΩ resistor to GND
 *
 * --- 433MHz Transmitter ---
 * DATA → D7
 * VCC  → 5V
 * GND  → GND
 */

#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

#define MUX_S0   4
#define MUX_S1   5
#define MUX_S2   6
#define MUX_COM  A0

// Calibrated with 47kΩ resistor
#define FLEX_STRAIGHT   80    // finger fully straight
#define FLEX_BENT       1020  // finger fully bent

// Smoothing
#define NUM_SAMPLES     20

void selectMuxChannel(int channel) {
  digitalWrite(MUX_S0, (channel & 0x01));
  digitalWrite(MUX_S1, (channel & 0x02));
  digitalWrite(MUX_S2, (channel & 0x04));
}

int readFlexSmoothed() {
  long total = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    selectMuxChannel(0);
    delayMicroseconds(50);
    total += analogRead(MUX_COM);
    delay(2);
  }
  return (int)(total / NUM_SAMPLES);
}

void setup() {
  Serial.begin(115200);

  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);

  mySwitch.enableTransmit(7);
  mySwitch.setProtocol(1);
  mySwitch.setPulseLength(350);
  mySwitch.setRepeatTransmit(10);

  Serial.println("=================================");
  Serial.println("Robotic Glove - Transmitter Ready");
  Serial.println("=================================");
}

void loop() {
  int flexRaw = readFlexSmoothed();

  // Straight (80) = 0°  |  Bent (1020) = 180°
  int angle = map(flexRaw, FLEX_STRAIGHT, FLEX_BENT, 0, 180);
  angle = constrain(angle, 0, 180);

  // Pack and transmit
  unsigned long packet = 900000UL + (unsigned long)angle;
  mySwitch.send(packet, 24);

  Serial.print("Flex Raw: ");
  Serial.print(flexRaw);
  Serial.print("  |  Angle: ");
  Serial.print(angle);
  Serial.print("°  |  Sent: ");
  Serial.println(packet);

  delay(50);
}