/*
 * Wireless Robotic Glove - TRANSMITTER
 * Arduino Nano + 74HC4051 mux + 5x flex sensors
 *
 * Protocol: 24-bit packet, bit-packed
 *   [SYNC: 4 bits = 0xA] [Thumb: 4] [Index: 4] [Middle: 4] [Ring: 4] [Pinky: 4]
 *   Each finger: 0-15 (16 levels, ~12 deg per step)
 *   Sync nibble 0xA rejects ~94% of random noise on receiver side.
 *
 * Wiring:
 * --- 74HC4051 Multiplexer ---
 * VCC -> 5V, GND -> GND, VEE -> GND, E -> GND
 * COM/OUT -> A0, S0 -> D4, S1 -> D5, S2 -> D6
 *
 * --- Flex Sensors ---
 * Thumb -> Y0, Index -> Y1, Middle -> Y2, Ring -> Y3, Pinky -> Y4
 *   (one end 5V, other end Y# + 47kohm to GND)
 *
 * --- 433MHz Transmitter ---
 * DATA -> D7, VCC -> 5V, GND -> GND
 *   17.3cm antenna wire on TX module ANT pad
 */

#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

// --- Pins ---
#define MUX_S0   4
#define MUX_S1   5
#define MUX_S2   6
#define MUX_COM  A0
#define TX_PIN   7

// --- Sampling ---
#define NUM_SAMPLES   3
#define NUM_FINGERS   5

// --- Protocol (must match receiver exactly) ---
#define PACKET_BITS       24
#define SYNC_NIBBLE       0xA
#define LEVELS_PER_FINGER 16
#define MAX_LEVEL         15

// --- Transmission policy ---
#define CHANGE_THRESH_LEVELS  1     // re-send if any finger changes >= 1 level
#define HEARTBEAT_MS          250   // re-send every 250ms even with no change
#define LOOP_DELAY_MS         5     // ~30Hz upper bound (limited by RCSwitch send time)

const int muxChannel[NUM_FINGERS] = { 0, 1, 2, 3, 4 };
const char* fingerName[NUM_FINGERS] = {
  "Thumb", "Index", "Middle", "Ring", "Pinky"
};

// Per-finger calibration. Replace with values from a calibration routine
// (hand flat = straight, full fist = bent). 80/1020 are placeholders.
int flexStraight[NUM_FINGERS] = {  80,  80,  80,  80,  80 };
int flexBent[NUM_FINGERS]     = { 1020, 1020, 1020, 1020, 1020 };

int lastLevelSent[NUM_FINGERS] = { -1, -1, -1, -1, -1 };
unsigned long lastTxTime = 0;

#define DEBUG_PRINT 1

void selectMuxChannel(int channel) {
  digitalWrite(MUX_S0, (channel & 0x01) ? HIGH : LOW);
  digitalWrite(MUX_S1, (channel & 0x02) ? HIGH : LOW);
  digitalWrite(MUX_S2, (channel & 0x04) ? HIGH : LOW);
}

int readFlexSmoothed(int channel) {
  // Switch mux ONCE, then take samples. Don't switch per sample.
  selectMuxChannel(channel);
  delayMicroseconds(150);
  analogRead(MUX_COM);  // dummy read for ADC sample-and-hold settle

  long total = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    total += analogRead(MUX_COM);
  }
  return (int)(total / NUM_SAMPLES);
}

// Build packed 24-bit packet:
//   [SYNC: 4 bits] [Thumb: 4] [Index: 4] [Middle: 4] [Ring: 4] [Pinky: 4]
unsigned long buildPacket(const int levels[NUM_FINGERS]) {
  unsigned long packet = ((unsigned long)SYNC_NIBBLE) << 20;
  for (int i = 0; i < NUM_FINGERS; i++) {
    int shift = 16 - (i * 4);  // thumb=16, index=12, middle=8, ring=4, pinky=0
    packet |= ((unsigned long)(levels[i] & 0x0F)) << shift;
  }
  return packet;
}

void setup() {
  Serial.begin(115200);

  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  digitalWrite(MUX_S0, LOW);
  digitalWrite(MUX_S1, LOW);
  digitalWrite(MUX_S2, LOW);

  mySwitch.enableTransmit(TX_PIN);
  mySwitch.setProtocol(1);
  mySwitch.setPulseLength(350);
  mySwitch.setRepeatTransmit(3);  // 3 repeats balances reliability vs. throughput

  Serial.println("=================================");
  Serial.println("Robotic Glove - Transmitter");
  Serial.println("   24-bit packed, sync 0xA");
  Serial.println("=================================");
}

void loop() {
  int currentLevels[NUM_FINGERS];
  bool anyChanged = false;

  // Read all five fingers in one synchronized snapshot — every transmitted
  // packet then represents one consistent moment of hand state.
  for (int i = 0; i < NUM_FINGERS; i++) {
    int raw = readFlexSmoothed(muxChannel[i]);
    int angle = map(raw, flexStraight[i], flexBent[i], 0, 180);
    angle = constrain(angle, 0, 180);

    int level = map(angle, 0, 180, 0, MAX_LEVEL);
    level = constrain(level, 0, MAX_LEVEL);

    currentLevels[i] = level;
    if (abs(level - lastLevelSent[i]) >= CHANGE_THRESH_LEVELS) {
      anyChanged = true;
    }
  }

  unsigned long now = millis();
  bool heartbeatDue = (now - lastTxTime) >= HEARTBEAT_MS;

  if (anyChanged || heartbeatDue) {
    unsigned long packet = buildPacket(currentLevels);
    mySwitch.send(packet, PACKET_BITS);
    lastTxTime = now;
    for (int i = 0; i < NUM_FINGERS; i++) lastLevelSent[i] = currentLevels[i];

    #if DEBUG_PRINT
      Serial.print(anyChanged ? "TX " : "HB ");
      Serial.print("0x");
      Serial.print(packet, HEX);
      Serial.print(" | ");
      for (int i = 0; i < NUM_FINGERS; i++) {
        Serial.print(fingerName[i]);
        Serial.print(":");
        Serial.print(currentLevels[i]);
        Serial.print(" ");
      }
      Serial.println();
    #endif
  }

  delay(LOOP_DELAY_MS);
}