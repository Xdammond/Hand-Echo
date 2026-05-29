# Hand Echo

## Overview
Hand Echo is a wireless robotic hand teleoperation system designed to replicate user hand movements through a sensor-equipped glove and servo-actuated robotic hand.

## Project Goals
- Capture finger motion using wearable sensors
- Transmit motion data wirelessly between microcontrollers
- Control a robotic hand in real time
- Build a modular system with separate glove, receiver, and mechanical subsystems
- Document hardware, firmware, testing, and lessons learned


## Subsystems

### Glove Controller
The glove controller measures finger movement using sensors mounted to a wearable glove. Sensor data is read by a microcontroller, processed, and prepared for wireless transmission.

### Wireless Communication
The wireless link sends motion data from the glove controller to the robotic hand receiver. This subsystem includes packet formatting, transmission timing, and reliability testing.

### Robotic Hand Receiver
The robotic hand receiver interprets incoming wireless data and controls servos to reproduce finger motion on the robotic hand.

## Repository Structure
docs/          System documentation and engineering notes
hardware/      Schematics, wiring diagrams, and BOMs
firmware/      Microcontroller code
media/         Images, videos, and diagrams
test_data/     Test results and measurements

## Status
Completed
