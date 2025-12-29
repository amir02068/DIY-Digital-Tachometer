#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int SENSOR_PIN = 2;
volatile unsigned long pulseCount = 0;
unsigned long lastCalcTime = 0;
const unsigned long CALC_INTERVAL = 500;
float currentRPM = 0.0;
float filteredRPM = 0.0;
const float FILTER_WEIGHT = 0.4;

const int STATS_BUFFER_SIZE = 10;
float rpmBuffer[STATS_BUFFER_SIZE];
int bufferIndex = 0;
unsigned long lastAvgTime = 0;
unsigned long lastMinMaxTime = 0;
const unsigned long AVG_INTERVAL = 5000;
const unsigned long MINMAX_INTERVAL = 10000;
float avgRPM = 0.0;
float maxRPM = 0.0;
float minRPM = 9999.0;

int fanFrame = 0;
unsigned long lastFanUpdateTime = 0;
const int FAN_CX = 110;
const int FAN_CY = 32;
const int FAN_RADIUS = 15;

void countPulseISR() {
  pulseCount++;
}

void setup() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), countPulseISR, RISING);

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println(F("RPM Meter"));
  display.display();
  delay(1500);

  unsigned long initialTime = millis();
  lastCalcTime = initialTime;
  lastAvgTime = initialTime;
  lastMinMaxTime = initialTime;
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastCalcTime >= CALC_INTERVAL) {
    noInterrupts();
    unsigned long pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    unsigned long elapsedTime = currentTime - lastCalcTime;
    lastCalcTime = currentTime;

    currentRPM = (pulses * 60000.0) / elapsedTime;
    
    filteredRPM = (filteredRPM * (1.0 - FILTER_WEIGHT)) + (currentRPM * FILTER_WEIGHT);

    rpmBuffer[bufferIndex] = filteredRPM;
    bufferIndex = (bufferIndex + 1) % STATS_BUFFER_SIZE;
  }

  if (currentTime - lastAvgTime >= AVG_INTERVAL) {
    float sum = 0;
    for (int i = 0; i < STATS_BUFFER_SIZE; i++) {
      sum += rpmBuffer[i];
    }
    avgRPM = sum / STATS_BUFFER_SIZE;
    lastAvgTime = currentTime;
  }

  if (filteredRPM > maxRPM) maxRPM = filteredRPM;
  if (filteredRPM < minRPM) minRPM = filteredRPM;

  if (currentTime - lastMinMaxTime >= MINMAX_INTERVAL) {
    minRPM = filteredRPM;
    maxRPM = filteredRPM;
    lastMinMaxTime = currentTime;
  }
  
  if (filteredRPM > 5) {
    long updateInterval = map(filteredRPM, 5, 4000, 500, 30);
    if (currentTime - lastFanUpdateTime >= updateInterval) {
      fanFrame = (fanFrame + 1) % 8;
      lastFanUpdateTime = currentTime;
    }
  }

  drawScreen();
}

void drawScreen() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("RPM:"));
  display.setCursor(60, 0);
  display.print((int)filteredRPM);

  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print(F("Avg(5s): "));
  display.print((int)avgRPM);

  display.setCursor(0, 36);
  display.print(F("Min: "));
  display.print((int)minRPM);

  display.setCursor(0, 48);
  display.print(F("Max: "));
  display.print((int)maxRPM);
  
  drawFan(FAN_CX, FAN_CY, FAN_RADIUS, fanFrame);

  display.display();
}

void drawFan(int x, int y, int r, int frame) {
  float startAngle = frame * (M_PI / 4.0);

  for (int i = 0; i < 4; i++) {
    float angle = startAngle + i * (M_PI / 2.0);
    
    int x1 = x + r * cos(angle);
    int y1 = y + r * sin(angle);
    
    int x2 = x + (r/2.5) * cos(angle - 0.4);
    int y2 = y + (r/2.5) * sin(angle - 0.4);

    int x3 = x + (r/2.5) * cos(angle + 0.4);
    int y3 = y + (r/2.5) * sin(angle + 0.4);

    display.fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_WHITE);
  }
  display.fillCircle(x, y, r / 4, SSD1306_BLACK);
  display.drawCircle(x, y, r / 4, SSD1306_WHITE);
}
