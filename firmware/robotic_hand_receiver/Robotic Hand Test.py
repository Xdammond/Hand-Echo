from machine import Pin, I2C
import time

# PCA9685 Register addresses
PCA9685_ADDRESS = 0x40
MODE1 = 0x00
PRESCALE = 0xFE
LED0_ON_L = 0x06

class PCA9685:
    def __init__(self, i2c, address=0x40):
        self.i2c = i2c
        self.address = address
        self.reset()
        self.set_pwm_freq(50)  # 50Hz for servos
    
    def reset(self):
        self.i2c.writeto_mem(self.address, MODE1, b'\x00')
    
    def set_pwm_freq(self, freq_hz):
        prescaleval = 25000000.0    # 25MHz
        prescaleval /= 4096.0       # 12-bit
        prescaleval /= float(freq_hz)
        prescaleval -= 1.0
        prescale = int(prescaleval + 0.5)
        
        oldmode = self.i2c.readfrom_mem(self.address, MODE1, 1)[0]
        newmode = (oldmode & 0x7F) | 0x10    # sleep
        self.i2c.writeto_mem(self.address, MODE1, bytes([newmode]))
        self.i2c.writeto_mem(self.address, PRESCALE, bytes([prescale]))
        self.i2c.writeto_mem(self.address, MODE1, bytes([oldmode]))
        time.sleep(0.005)
        self.i2c.writeto_mem(self.address, MODE1, bytes([oldmode | 0x80]))
    
    def set_pwm(self, channel, on, off):
        self.i2c.writeto_mem(self.address, LED0_ON_L + 4 * channel, bytes([on & 0xFF]))
        self.i2c.writeto_mem(self.address, LED0_ON_L + 4 * channel + 1, bytes([on >> 8]))
        self.i2c.writeto_mem(self.address, LED0_ON_L + 4 * channel + 2, bytes([off & 0xFF]))
        self.i2c.writeto_mem(self.address, LED0_ON_L + 4 * channel + 3, bytes([off >> 8]))
    
    def set_servo_angle(self, channel, angle):
        # Convert angle (0-180) to pulse width (typically 500-2500 microseconds)
        # For 50Hz (20ms period), 4096 steps
        min_pulse = 102  # ~0.5ms (500us)
        max_pulse = 512  # ~2.5ms (2500us)
        pulse = int(min_pulse + (angle / 180.0) * (max_pulse - min_pulse))
        self.set_pwm(channel, 0, pulse)

# ---- CONFIGURE YOUR I2C PINS HERE ----
# Standard I2C pins for Raspberry Pi Pico
SDA_PIN = 0  # GP0
SCL_PIN = 1  # GP1

# Initialize I2C
i2c = I2C(0, scl=Pin(SCL_PIN), sda=Pin(SDA_PIN), freq=400000)

# Check if PCA9685 is connected
devices = i2c.scan()
print("I2C devices found:", [hex(device) for device in devices])

if 0x40 not in devices:
    print("WARNING: PCA9685 not found at address 0x40!")
    print("Check your wiring:")
    print("  - SDA to GP0")
    print("  - SCL to GP1")
    print("  - VCC to 3.3V or 5V")
    print("  - GND to GND")
else:
    print("PCA9685 found!")

# Initialize PCA9685
pca = PCA9685(i2c)

# ---- CONFIGURE YOUR SERVO CHANNELS HERE ----
# PCA9685 has 16 channels (0-15)
# Assign channels for each finger
THUMB = 0
INDEX = 3
MIDDLE = 8
RING = 12
PINKY = 15

servo_channels = [THUMB, INDEX, MIDDLE, RING, PINKY]
servo_names = ["Thumb", "Index", "Middle", "Ring", "Pinky"]

def move_servo(channel, angle):
    """Move servo on specified channel to angle (0-180)"""
    angle = max(0, min(180, angle))  # Clamp angle
    pca.set_servo_angle(channel, angle)

def test_servos():
    """Test each servo individually"""
    for i, channel in enumerate(servo_channels):
        print(f"Testing {servo_names[i]} (Channel {channel})")
        move_servo(channel, 0)
        time.sleep(0.5)
        move_servo(channel, 90)
        time.sleep(0.5)
        move_servo(channel, 180)
        time.sleep(0.5)
        move_servo(channel, 90)
        time.sleep(0.5)

def close_hand():
    """Close all fingers"""
    print("Closing hand...")
    for channel in servo_channels:
        move_servo(channel, 180)
    time.sleep(1)

def open_hand():
    """Open all fingers"""
    print("Opening hand...")
    for channel in servo_channels:
        move_servo(channel, 0)
    time.sleep(1)

def peace_sign():
    """Make peace sign (index and middle up)"""
    print("Peace sign...")
    move_servo(THUMB, 90)
    move_servo(INDEX, 0)
    move_servo(MIDDLE, 0)
    move_servo(RING, 180)
    move_servo(PINKY, 180)
    time.sleep(1)

def thumbs_up():
    """Thumbs up gesture"""
    print("Thumbs up...")
    move_servo(THUMB, 0)
    move_servo(INDEX, 180)
    move_servo(MIDDLE, 180)
    move_servo(RING, 180)
    move_servo(PINKY, 180)
    time.sleep(1)

print("\nStarting robotic hand test...")
print("Press Ctrl+C to stop\n")

try:
    while True:
        test_servos()
        print("\nFull hand test complete.")
        
        open_hand()
        close_hand()
        peace_sign()
        thumbs_up()
        open_hand()
        
        print("\nRepeating in 5 seconds...\n")
        time.sleep(5)
        
except KeyboardInterrupt:
    print("\nTest ended. Returning to neutral position...")
    open_hand()
    print("Done!")