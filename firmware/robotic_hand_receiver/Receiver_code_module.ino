/*
 * Wireless Robotic Glove - RECEIVER
 * Arduino Uno + PCA9685 + 5x LFD-01 Servos
 *
 * Protocol: 24-bit packet, bit-packed (matches transmitter exactly)
 *   [SYNC: 4 bits = 0xA] [Thumb: 4] [Index: 4] [Middle: 4] [Ring: 4] [Pinky: 4]
 *   Each finger: 0-15 (16 levels)
 *   Sync nibble validation rejects ~94% of random noise.
 *
 * Wiring:
 * --- 433MHz Receiver ---
 * DATA -> D2 (interrupt 0)
 * VCC  -> 5V
 * GND  -> GND
 *   17.3cm antenna wire on RX module ANT pad
 *
 * --- PCA9685 (I2C) ---
 * VCC -> 5V, GND -> GND, SDA -> A4, SCL -> A5
 * V+  -> External 5V (servo power, separate from logic if possible)
 *
 * --- Servos on PCA9685 ---
 * CH0 -> Thumb, CH1 -> Index, CH2 -> Middle, CH3 -> Ring, CH4 -> Pinky
 */

#include <RCSwitch.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

RCSwitch mySwitch = RCSwitch();
Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);

// --- Servo timing ---
// 600-2400us avoids hitting hard stops on most hobby servos. Wider ranges
// cause stall current and supply noise that can deafen the 433MHz receiver.
#define SERVO_MIN_PULSE  600
#define SERVO_MAX_PULSE  2400
#define PCA_FREQ         50

// --- Protocol (must match transmitter exactly) ---
#define NUM_FINGERS       5
#define PACKET_BITS       24
#define SYNC_NIBBLE       0xA
#define LEVELS_PER_FINGER 16
#define MAX_LEVEL         15

// --- Smoothing ---
#define SMOOTH_FACTOR    0.40f

// Precomputed PWM tick conversion: at 50Hz, 4096 ticks per 20000us period.
// tick = (us * 4096 * PCA_FREQ) / 1,000,000 — pure integer math.
#define US_TO_TICK(us)   ((int)(((long)(us) * 4096L * PCA_FREQ) / 1000000L))

const int servoChannel[NUM_FINGERS] = { 0, 1, 2, 3, 4 };

const char* fingerName[NUM_FINGERS] = {
  "Thumb", "Index", "Middle", "Ring", "Pinky"
};

// Mechanical inversion: thumb is mounted opposite the other fingers in this
// hand assembly, so its servo direction is not inverted while the other four
// are. Update this array if you swap or remount any servos.
const bool invertFinger[NUM_FINGERS] = {
  false,  // Thumb
  true,   // Index
  true,   // Middle
  true,   // Ring
  true    // Pinky
};

float smoothedAngle[NUM_FINGERS] = { 0, 0, 0, 0, 0 };
bool firstPacket = true;

#define DEBUG_PRINT 1

void setServoAngle(int channel, int angle) {
  angle = constrain(angle, 0, 180);
  int pulseUs = map(angle, 0, 180, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
  pca.setPWM(channel, 0, US_TO_TICK(pulseUs));
}

// Unpack 24-bit packet:
//   [SYNC: 4 bits] [Thumb: 4] [Index: 4] [Middle: 4] [Ring: 4] [Pinky: 4]
// Returns true if sync nibble matches (valid packet), false otherwise.
bool unpackPacket(unsigned long packet, int outLevels[NUM_FINGERS]) {
  // Validate sync nibble (top 4 bits of 24-bit packet)
  unsigned long sync = (packet >> 20) & 0x0F;
  if (sync != SYNC_NIBBLE) return false;

  outLevels[0] = (packet >> 16) & 0x0F;  // Thumb
  outLevels[1] = (packet >> 12) & 0x0F;  // Index
  outLevels[2] = (packet >> 8)  & 0x0F;  // Middle
  outLevels[3] = (packet >> 4)  & 0x0F;  // Ring
  outLevels[4] =  packet        & 0x0F;  // Pinky

  return true;
}

void applyAngles(const int levels[NUM_FINGERS]) {
  #if DEBUG_PRINT
    Serial.print("OK | ");
  #endif

  for (int i = 0; i < NUM_FINGERS; i++) {
    int angle = map(levels[i], 0, MAX_LEVEL, 0, 180);
    if (invertFinger[i]) angle = 180 - angle;

    if (firstPacket) {
      smoothedAngle[i] = angle;
    } else {
      smoothedAngle[i] = (SMOOTH_FACTOR * angle) +
                         ((1.0f - SMOOTH_FACTOR) * smoothedAngle[i]);
    }

    int finalAngle = (int)smoothedAngle[i];
    setServoAngle(servoChannel[i], finalAngle);

    #if DEBUG_PRINT
      Serial.print(fingerName[i]);
      Serial.print(":");
      Serial.print(finalAngle);
      Serial.print(" ");
    #endif
  }
  firstPacket = false;

  #if DEBUG_PRINT
    Serial.println();
  #endif
}

void startupSequence() {
  Serial.println("Step 1: Closing hand...");
  for (int angle = 0; angle <= 180; angle++) {
    for (int i = 0; i < NUM_FINGERS; i++) {
      int a = invertFinger[i] ? (180 - angle) : angle;
      setServoAngle(servoChannel[i], a);
    }
    delay(20);
  }
  delay(500);

  Serial.println("Step 2: Opening hand...");
  for (int angle = 180; angle >= 0; angle--) {
    for (int i = 0; i < NUM_FINGERS; i++) {
      int a = invertFinger[i] ? (180 - angle) : angle;
      setServoAngle(servoChannel[i], a);
    }
    delay(20);
  }
  delay(500);

  Serial.println("Startup complete - now receiving data...");
  Serial.println("=================================");
}

void setup() {
  Serial.begin(115200);

  pca.begin();
  pca.setOscillatorFrequency(27000000);
  pca.setPWMFreq(PCA_FREQ);
  delay(100);

  Serial.println("=================================");
  Serial.println("Robotic Glove - Receiver");
  Serial.println("   24-bit packed, sync 0xA");
  Serial.println("=================================");

  startupSequence();

  // Enable RF receive after startup so servo PWM transients during the
  // calibration sweep don't get misread as data on the interrupt pin.
  mySwitch.enableReceive(0);  // interrupt 0 = D2 on Uno
}

void loop() {
  if (mySwitch.available()) {
    unsigned long received = mySwitch.getReceivedValue();
    int bitlen = mySwitch.getReceivedBitlength();

    // First filter: must be exactly 24 bits.
    if (bitlen == PACKET_BITS) {
      int levels[NUM_FINGERS];
      // Second filter: sync nibble must match. Together these reject
      // virtually all noise-induced false decodes.
      if (unpackPacket(received, levels)) {
        applyAngles(levels);
      } else {
        #if DEBUG_PRINT
          Serial.print("Bad sync: 0x");
          Serial.println(received, HEX);
        #endif
      }
    }
    // Silently drop wrong-length packets — they're almost always noise.

    mySwitch.resetAvailable();
  }
}