#include <Adafruit_NeoPixel.h>

#define PIN 6
#define NUM_LEDS 64

#define WIDTH 8
#define HEIGHT 8

#define START_LED 6
#define POT_PIN A0

Adafruit_NeoPixel strip(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

// -------- GLOBAL STATE --------
int prevPot = 0;
unsigned long prevTime = 0;
float smoothSpeed = 0;
bool emergencyOn = false;
unsigned long lastBlink = 0;
bool emergencyActive = false;
int lastPot = 0;  // pot value track karne ke liye

// -------- MAPPING --------
int getIndex(int x, int y) {
  return START_LED + (y * 8 + x);
}

// -------- SAFE PIXEL --------
void setPixel(int x, int y, uint32_t color) {
  if (x == 0) return;
  int i = getIndex(x, y);
  if (i < NUM_LEDS)
    strip.setPixelColor(i, color);
}

// -------- CENTER --------
bool isCenter(int x, int y) {
  if ((x == 3 || x == 4) && (y == 3 || y == 4)) return true;
  if ((x == 3 || x == 4) && y == 2) return true;
  if (x == 5 && (y == 2 || y == 3 || y == 4)) return true;
  return false;
}

// -------- BREATHING --------
uint8_t breath(uint32_t t) {
  return 40 + (sin(t * 0.005) * 30);
}

// -------- SPEED DETECTION --------
float getChangeSpeed() {
  int currentPot = analogRead(POT_PIN);
  unsigned long currentTime = millis();

  int potDiff = abs(currentPot - prevPot);
  unsigned long timeDiff = currentTime - prevTime;

  float speed = 0;
  if (timeDiff > 0) {
    speed = (potDiff / (float)timeDiff) * 1000.0;
  }

  // Smoothing
  smoothSpeed = (smoothSpeed * 0.7) + (speed * 0.3);

  prevPot = currentPot;
  prevTime = currentTime;

  return smoothSpeed;
}

// -------- PROGRESSIVE FILL --------
void progressiveFill(int pot) {
  int rangePerRow = 1024 / HEIGHT;

  for (int y = 0; y < HEIGHT; y++) {
    int row = HEIGHT - 1 - y;
    int rowStart = y * rangePerRow;
    int rowEnd   = rowStart + rangePerRow;

    uint8_t brightness = 0;

    if (pot >= rowEnd) {
      brightness = 255;
    } else if (pot >= rowStart) {
      brightness = map(pot, rowStart, rowEnd, 0, 255);
    } else {
      brightness = 0;
    }

    for (int x = 0; x < WIDTH; x++) {
      if (x == 0) continue;
      if (isCenter(x, row)) continue;
      setPixel(x, row, brightness > 0 ? strip.Color(brightness, 0, 0) : 0);
    }
  }
}

// -------- CENTER DRAW --------
void drawCenter() {
  uint8_t b = breath(millis());
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (isCenter(x, y)) {
        setPixel(x, y, strip.Color(b, b, b));
      }
    }
  }
}

// -------- MEDIUM BRAKE --------
void mediumBrake(int pot) {
  uint8_t pulse = 128 + (sin(millis() * 0.02) * 127);

  int rangePerRow = 1024 / HEIGHT;

  for (int y = 0; y < HEIGHT; y++) {
    int row = HEIGHT - 1 - y;
    int rowStart = y * rangePerRow;
    int rowEnd   = rowStart + rangePerRow;

    uint8_t brightness = 0;
    if (pot >= rowEnd) {
      brightness = pulse;
    } else if (pot >= rowStart) {
      brightness = map(pot, rowStart, rowEnd, 0, pulse);
    } else {
      brightness = 0;
    }

    for (int x = 0; x < WIDTH; x++) {
      if (x == 0) continue;
      if (isCenter(x, row)) continue;
      setPixel(x, row, brightness > 0 ? strip.Color(brightness, 0, 0) : 0);
    }
  }
  drawCenter();
}

// -------- EMERGENCY BRAKE --------
void emergencyBrake() {
  unsigned long now = millis();

  if (now - lastBlink > 100) {
    emergencyOn = !emergencyOn;
    lastBlink = now;
  }

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (x == 0) continue;
      setPixel(x, y, emergencyOn ? strip.Color(255, 0, 0) : 0);
    }
  }
}

// -------- SETUP --------
void setup() {
  Serial.begin(9600);
  strip.begin();
  strip.setBrightness(255);
  strip.show();

  prevPot  = analogRead(POT_PIN);
  lastPot  = prevPot;
  prevTime = millis();
}

// -------- LOOP --------
void loop() {
  float currentSpeed = getChangeSpeed();
  int pot = analogRead(POT_PIN);

  Serial.print("Speed: ");
  Serial.print(currentSpeed);
  Serial.print("  Pot: ");
  Serial.println(pot);

  // Emergency trigger — speed > 500
  if (currentSpeed > 1000) {
    emergencyActive = true;
  }

  strip.clear(); 

  if (emergencyActive) {
    // 🔴 EMERGENCY — blink karta rahe
    emergencyBrake();

    // Sirf tab stop ho jab:
    // 1. Pot ki value DECREASE ho rahi ho (brake release)
    // 2. Speed bilkul slow ho gayi ho
    if (pot < lastPot && currentSpeed < 80) {
      emergencyActive = false;
    }

  } else if (currentSpeed > 500) {
    // 🟡 MEDIUM BRAKE — speed > 200
    mediumBrake(pot);

  } else {
    // 🟢 NORMAL
    progressiveFill(pot);
    drawCenter();
  }

  lastPot = pot;  // pot value save karo comparison ke liye

  strip.show();
  delay(20);
}