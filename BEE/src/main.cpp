#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>

// ======================================================
// SMART BEEHIVE MONITORING SYSTEM
// ESP32 + BMP180 + MQ135 + MIC + OLED
// Smooth Values + Animation + Status
// ======================================================

// ---------------- PIN DEFINITIONS ----------------

#define MIC_PIN 34
#define MQ135_PIN 35

// ---------------- OLED SETTINGS ----------------

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ---------------- BMP180 ----------------

Adafruit_BMP085 bmp;

// ======================================================
// SENSOR VARIABLES
// ======================================================

float hiveTemperature = 0.0;
float pressure = 0.0;

float smoothTemperature = 0.0;
float smoothPressure = 0.0;
float smoothMic = 0.0;
float smoothMQ135 = 0.0;

int micRaw = 0;
int mq135Raw = 0;

// ======================================================
// DISPLAY VARIABLES
// ======================================================

int displayPage = 0;
int animationFrame = 0;

unsigned long lastPageChange = 0;
unsigned long lastSensorRead = 0;
unsigned long lastSerialPrint = 0;
unsigned long lastAnimation = 0;

// Page switches every 4 seconds
const unsigned long PAGE_TIME = 4000;

// Sensor reading interval
const unsigned long SENSOR_TIME = 150;

// Animation refresh
const unsigned long ANIMATION_TIME = 120;

// Exponential smoothing
float alpha = 0.15;

// ======================================================
// FUNCTION DECLARATIONS
// Required for PlatformIO main.cpp
// ======================================================

void startupAnimation();

void readSensors();

String getTemperatureCondition();
String getAirCondition();
String getSoundCondition();
String getPressureCondition();

void drawStatusIcon(String status);
void drawTemperatureIcon();

void temperaturePage();
void airQualityPage();
void soundPage();
void pressurePage();

void printSerialData();

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println(" SMART BEEHIVE MONITOR");
  Serial.println("==============================");

  // ESP32 I2C
  // SDA = GPIO21
  // SCL = GPIO22
  Wire.begin(21, 22);

  pinMode(MIC_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);

  // ---------------- OLED ----------------

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {

    Serial.println("OLED initialization failed!");

    while (1) {
      delay(10);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  // ---------------- BMP180 ----------------

  if (!bmp.begin()) {

    Serial.println("BMP180 not detected!");

    display.clearDisplay();

    display.setTextSize(1);

    display.setCursor(20, 15);
    display.println("BMP180 ERROR");

    display.setCursor(10, 32);
    display.println("CHECK SDA / SCL");

    display.setCursor(17, 47);
    display.println("AND POWER");

    display.display();

    while (1) {
      delay(10);
    }
  }

  Serial.println("OLED   : OK");
  Serial.println("BMP180 : OK");
  Serial.println("MIC    : READY");
  Serial.println("MQ135  : READY");
  Serial.println("==============================");

  // Startup animation
  startupAnimation();

  // First readings
  hiveTemperature = bmp.readTemperature();

  pressure =
    bmp.readPressure() / 100.0;

  micRaw =
    analogRead(MIC_PIN);

  mq135Raw =
    analogRead(MQ135_PIN);

  // Initialize smooth values
  smoothTemperature =
    hiveTemperature;

  smoothPressure =
    pressure;

  smoothMic =
    micRaw;

  smoothMQ135 =
    mq135Raw;

  lastPageChange = millis();
}

// ======================================================
// STARTUP ANIMATION
// ======================================================

void startupAnimation() {

  // Bee-like moving dot animation

  for (int x = 0; x < 128; x += 4) {

    display.clearDisplay();

    display.setTextSize(1);

    display.setCursor(23, 5);
    display.println("SMART BEEHIVE");

    display.setCursor(31, 18);
    display.println("MONITOR");

    // Moving "bee"
    display.fillCircle(
      x,
      38,
      2,
      SSD1306_WHITE
    );

    display.drawLine(
      x - 3,
      35,
      x - 1,
      37,
      SSD1306_WHITE
    );

    display.drawLine(
      x + 1,
      37,
      x + 3,
      35,
      SSD1306_WHITE
    );

    display.drawRect(
      10,
      52,
      108,
      7,
      SSD1306_WHITE
    );

    int progress =
      map(
        x,
        0,
        124,
        0,
        104
      );

    display.fillRect(
      12,
      54,
      progress,
      3,
      SSD1306_WHITE
    );

    display.display();

    delay(25);
  }

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(25, 18);
  display.println("READY");

  display.setTextSize(1);
  display.setCursor(19, 45);
  display.println("Hive Monitoring");

  display.display();

  delay(700);
}

// ======================================================
// READ SENSOR VALUES
// ======================================================

void readSensors() {

  float temp =
    bmp.readTemperature();

  float press =
    bmp.readPressure() / 100.0;

  int mic =
    analogRead(MIC_PIN);

  int mq =
    analogRead(MQ135_PIN);

  hiveTemperature = temp;
  pressure = press;

  micRaw = mic;
  mq135Raw = mq;

  // ---------------- SMOOTH VALUES ----------------

  smoothTemperature =
    alpha * hiveTemperature +
    (1.0 - alpha) *
    smoothTemperature;

  smoothPressure =
    alpha * pressure +
    (1.0 - alpha) *
    smoothPressure;

  smoothMic =
    alpha * micRaw +
    (1.0 - alpha) *
    smoothMic;

  smoothMQ135 =
    alpha * mq135Raw +
    (1.0 - alpha) *
    smoothMQ135;
}

// ======================================================
// TEMPERATURE CONDITION
// BMP180 temperature used as hive climate
// ======================================================

String getTemperatureCondition() {

  if (
    smoothTemperature >= 32.0 &&
    smoothTemperature <= 36.0
  ) {

    return "NORMAL";
  }

  else if (
    (
      smoothTemperature >= 29.0 &&
      smoothTemperature < 32.0
    )
    ||
    (
      smoothTemperature > 36.0 &&
      smoothTemperature <= 38.0
    )
  ) {

    return "OK";
  }

  else if (
    (
      smoothTemperature >= 26.0 &&
      smoothTemperature < 29.0
    )
    ||
    (
      smoothTemperature > 38.0 &&
      smoothTemperature <= 40.0
    )
  ) {

    return "WARNING";
  }

  else {

    return "CRITICAL";
  }
}

// ======================================================
// MQ135 CONDITION
// Prototype raw ADC thresholds
// ======================================================

String getAirCondition() {

  if (smoothMQ135 < 1200) {

    return "NORMAL";
  }

  else if (smoothMQ135 < 2000) {

    return "OK";
  }

  else if (smoothMQ135 < 2800) {

    return "WARNING";
  }

  else {

    return "CRITICAL";
  }
}

// ======================================================
// MICROPHONE CONDITION
// Raw ADC thresholds - calibrate for your module
// ======================================================

String getSoundCondition() {

  if (smoothMic < 1200) {

    return "NORMAL";
  }

  else if (smoothMic < 2200) {

    return "OK";
  }

  else if (smoothMic < 3200) {

    return "WARNING";
  }

  else {

    return "CRITICAL";
  }
}

// ======================================================
// PRESSURE CONDITION
// ======================================================

String getPressureCondition() {

  if (
    smoothPressure >= 990 &&
    smoothPressure <= 1025
  ) {

    return "NORMAL";
  }

  else if (
    smoothPressure >= 970 &&
    smoothPressure <= 1040
  ) {

    return "OK";
  }

  else {

    return "WARNING";
  }
}

// ======================================================
// STATUS ICON
// ======================================================

void drawStatusIcon(String status) {

  int x = 116;
  int y = 7;

  if (status == "NORMAL") {

    display.drawCircle(
      x,
      y,
      5,
      SSD1306_WHITE
    );

    display.drawPixel(
      x - 2,
      y - 1,
      SSD1306_WHITE
    );

    display.drawPixel(
      x + 2,
      y - 1,
      SSD1306_WHITE
    );

    display.drawLine(
      x - 2,
      y + 2,
      x + 2,
      y + 2,
      SSD1306_WHITE
    );
  }

  else if (status == "OK") {

    display.drawCircle(
      x,
      y,
      5,
      SSD1306_WHITE
    );

    display.drawLine(
      x - 2,
      y,
      x,
      y + 2,
      SSD1306_WHITE
    );

    display.drawLine(
      x,
      y + 2,
      x + 3,
      y - 2,
      SSD1306_WHITE
    );
  }

  else if (status == "WARNING") {

    display.drawTriangle(
      x,
      y - 5,
      x - 5,
      y + 4,
      x + 5,
      y + 4,
      SSD1306_WHITE
    );

    display.drawPixel(
      x,
      y - 1,
      SSD1306_WHITE
    );

    display.drawPixel(
      x,
      y,
      SSD1306_WHITE
    );

    display.drawPixel(
      x,
      y + 2,
      SSD1306_WHITE
    );
  }

  else {

    // blinking critical icon
    if (animationFrame % 2 == 0) {

      display.fillCircle(
        x,
        y,
        6,
        SSD1306_WHITE
      );

      display.setTextColor(
        SSD1306_BLACK
      );

      display.setTextSize(1);

      display.setCursor(
        x - 2,
        y - 3
      );

      display.print("!");

      display.setTextColor(
        SSD1306_WHITE
      );
    }

    else {

      display.drawCircle(
        x,
        y,
        6,
        SSD1306_WHITE
      );
    }
  }
}

// ======================================================
// TEMPERATURE ICON
// ======================================================

void drawTemperatureIcon() {

  int x = 8;
  int y = 21;

  // thermometer tube
  display.drawRect(
    x + 3,
    y,
    5,
    20,
    SSD1306_WHITE
  );

  // bulb
  display.drawCircle(
    x + 5,
    y + 23,
    5,
    SSD1306_WHITE
  );

  int tempLevel =
    constrain(
      (int)smoothTemperature,
      20,
      45
    );

  int level =
    map(
      tempLevel,
      20,
      45,
      1,
      18
    );

  display.fillRect(
    x + 5,
    y + 19 - level,
    2,
    level,
    SSD1306_WHITE
  );

  display.fillCircle(
    x + 5,
    y + 23,
    3,
    SSD1306_WHITE
  );
}

// ======================================================
// PAGE 1 - HIVE CLIMATE
// ======================================================

void temperaturePage() {

  String status =
    getTemperatureCondition();

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    25,
    0
  );

  display.print(
    "HIVE CLIMATE"
  );

  drawStatusIcon(status);

  display.drawLine(
    0,
    13,
    127,
    13,
    SSD1306_WHITE
  );

  drawTemperatureIcon();

  // Main value
  display.setTextSize(2);

  display.setCursor(
    30,
    20
  );

  display.print(
    smoothTemperature,
    1
  );

  display.print(" C");

  // Status
  display.setTextSize(1);

  display.setCursor(
    30,
    43
  );

  display.print("Status:");

  display.setCursor(
    30,
    53
  );

  display.print(status);

  // Animated bottom indicator
  int pulseWidth =
    20 +
    ((animationFrame % 10) * 9);

  if (pulseWidth > 116) {
    pulseWidth = 116;
  }

  display.drawRect(
    5,
    61,
    118,
    2,
    SSD1306_WHITE
  );

  display.fillRect(
    6,
    61,
    pulseWidth,
    2,
    SSD1306_WHITE
  );

  display.display();
}

// ======================================================
// PAGE 2 - AIR QUALITY
// ======================================================

void airQualityPage() {

  String status =
    getAirCondition();

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    27,
    0
  );

  display.print(
    "AIR QUALITY"
  );

  drawStatusIcon(status);

  display.drawLine(
    0,
    13,
    127,
    13,
    SSD1306_WHITE
  );

  // Animated particles
  for (int i = 0; i < 5; i++) {

    int x =
      (
        animationFrame * 7 +
        i * 25
      ) % 128;

    int y =
      19 +
      (i % 3) * 6;

    display.drawCircle(
      x,
      y,
      1,
      SSD1306_WHITE
    );
  }

  display.setTextSize(2);

  display.setCursor(
    30,
    29
  );

  display.print(
    (int)smoothMQ135
  );

  display.setTextSize(1);

  display.setCursor(
    7,
    48
  );

  display.print("Status: ");

  display.print(status);

  // Bar graph
  display.drawRect(
    5,
    57,
    118,
    6,
    SSD1306_WHITE
  );

  int barValue =
    map(
      constrain(
        (int)smoothMQ135,
        0,
        4095
      ),
      0,
      4095,
      0,
      114
    );

  display.fillRect(
    7,
    59,
    barValue,
    2,
    SSD1306_WHITE
  );

  display.display();
}

// ======================================================
// PAGE 3 - BEE SOUND
// ======================================================

void soundPage() {

  String status =
    getSoundCondition();

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    34,
    0
  );

  display.print(
    "BEE SOUND"
  );

  drawStatusIcon(status);

  display.drawLine(
    0,
    13,
    127,
    13,
    SSD1306_WHITE
  );

  // Animated waveform

  int centerY = 28;

  for (int x = 0; x < 128; x += 6) {

    int wave =
      (
        x +
        animationFrame * 4
      ) % 18;

    int height =
      map(
        wave,
        0,
        17,
        2,
        10
      );

    display.drawLine(
      x,
      centerY - height,
      x,
      centerY + height,
      SSD1306_WHITE
    );
  }

  display.setTextSize(1);

  display.setCursor(
    5,
    43
  );

  display.print("Sound: ");

  display.print(
    (int)smoothMic
  );

  display.setCursor(
    5,
    54
  );

  display.print("Status: ");

  display.print(status);

  display.display();
}

// ======================================================
// PAGE 4 - PRESSURE
// ======================================================

void pressurePage() {

  String status =
    getPressureCondition();

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    35,
    0
  );

  display.print(
    "PRESSURE"
  );

  drawStatusIcon(status);

  display.drawLine(
    0,
    13,
    127,
    13,
    SSD1306_WHITE
  );

  // Animated cloud
  int cloudX =
    6 +
    (animationFrame % 12);

  display.drawCircle(
    cloudX + 10,
    26,
    6,
    SSD1306_WHITE
  );

  display.drawCircle(
    cloudX + 18,
    23,
    7,
    SSD1306_WHITE
  );

  display.drawCircle(
    cloudX + 26,
    27,
    6,
    SSD1306_WHITE
  );

  display.drawLine(
    cloudX + 5,
    33,
    cloudX + 30,
    33,
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    45,
    21
  );

  display.print(
    smoothPressure,
    0
  );

  display.setTextSize(1);

  display.setCursor(
    91,
    28
  );

  display.print(
    "hPa"
  );

  display.setCursor(
    10,
    48
  );

  display.print(
    "Status: "
  );

  display.print(status);

  display.display();
}

// ======================================================
// SERIAL OUTPUT
// ======================================================

void printSerialData() {

  Serial.println();
  Serial.println("==============================");
  Serial.println("     BEEHIVE SENSOR DATA");
  Serial.println("==============================");

  Serial.print("Hive Temperature : ");
  Serial.print(
    smoothTemperature,
    2
  );
  Serial.print(" C  | ");
  Serial.println(
    getTemperatureCondition()
  );

  Serial.print("Pressure         : ");
  Serial.print(
    smoothPressure,
    2
  );
  Serial.print(" hPa | ");
  Serial.println(
    getPressureCondition()
  );

  Serial.print("Bee Sound Raw    : ");
  Serial.print(
    (int)smoothMic
  );
  Serial.print(" | ");
  Serial.println(
    getSoundCondition()
  );

  Serial.print("MQ135 Raw        : ");
  Serial.print(
    (int)smoothMQ135
  );
  Serial.print(" | ");
  Serial.println(
    getAirCondition()
  );

  Serial.println("------------------------------");
}

// ======================================================
// MAIN LOOP
// ======================================================

void loop() {

  unsigned long currentMillis =
    millis();

  // ---------------- SENSOR READ ----------------

  if (
    currentMillis -
    lastSensorRead >=
    SENSOR_TIME
  ) {

    lastSensorRead =
      currentMillis;

    readSensors();
  }

  // ---------------- ANIMATION ----------------

  if (
    currentMillis -
    lastAnimation >=
    ANIMATION_TIME
  ) {

    lastAnimation =
      currentMillis;

    animationFrame++;

    if (animationFrame > 50) {
      animationFrame = 0;
    }
  }

  // ---------------- PAGE CHANGE ----------------

  if (
    currentMillis -
    lastPageChange >=
    PAGE_TIME
  ) {

    lastPageChange =
      currentMillis;

    displayPage++;

    if (displayPage > 3) {
      displayPage = 0;
    }
  }

  // ---------------- OLED DISPLAY ----------------

  switch (displayPage) {

    case 0:
      temperaturePage();
      break;

    case 1:
      airQualityPage();
      break;

    case 2:
      soundPage();
      break;

    case 3:
      pressurePage();
      break;
  }

  // ---------------- SERIAL MONITOR ----------------

  if (
    currentMillis -
    lastSerialPrint >=
    1000
  ) {

    lastSerialPrint =
      currentMillis;

    printSerialData();
  }

  delay(20);
}