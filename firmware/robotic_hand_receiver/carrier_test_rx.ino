/*
 * Carrier Test - Arduino Uno
 * Reads the receiver DATA pin and prints state to Serial.
 * Should print HIGH continuously when transmitter is keyed.
 * Once confirmed, move on to Manchester test sketches.
 *
 * Wiring:
 *   Receiver DATA out -> D2
 */

#define RX_PIN 2

void setup() {
  Serial.begin(115200);
  pinMode(RX_PIN, INPUT);
  Serial.println(F("Carrier test ready - watching D2"));
}

void loop() {
  Serial.println(digitalRead(RX_PIN) ? F("HIGH") : F("LOW"));
  delay(500);
}
