#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht;

struct PLS916H_Packet {
  uint8_t header[13];
  uint8_t data[144];
  uint8_t checksum;
  uint8_t tail[4];
} __attribute__((packed));

uint8_t frame[144];

struct Segment { int byteIndex; int bitIndex; };

// Segment mapping
Segment digitSegments[6][7] = {
  {{-1,-1},{-1,-1},{-1,-1},{-1,-1},{15,7},{7,7},{-1,-1}}, // Digit 1
  {{-1,-1},{-1,-1},{-1,-1},{-1,-1},{55,7},{47,7},{-1,-1}}, // Digit 2
  {{0,7},{1,7},{2,7},{3,7},{4,7},{5,7},{6,7}},             // Digit 3
  {{8,7},{9,7},{10,7},{11,7},{12,7},{13,7},{14,7}},        // Digit 4
  {{40,7},{41,7},{42,7},{43,7},{44,7},{45,7},{46,7}},      // Digit 5
  {{48,7},{49,7},{50,7},{51,7},{52,7},{53,7},{54,7}}       // Digit 6
};

// Segment patterns for numbers 0–9
const bool numberTable[10][7] = {
  {1,1,1,1,1,1,0}, // 0
  {0,1,1,0,0,0,0}, // 1
  {1,1,0,1,1,0,1}, // 2
  {1,1,1,1,0,0,1}, // 3
  {0,1,1,0,0,1,1}, // 4
  {1,0,1,1,0,1,1}, // 5
  {1,0,1,1,1,1,1}, // 6
  {1,1,1,0,0,0,0}, // 7
  {1,1,1,1,1,1,1}, // 8
  {1,1,1,1,0,1,1}  // 9
};

uint8_t chk8_pls916h(uint8_t data[144]) {
  uint8_t b = 0;
  for (int i = 0; i < 144; i++) b += data[i];
  return b;
}

void clearDigits() { memset(frame, 0, sizeof(frame)); }

void write_pls916h(uint8_t data[144]) {
  static PLS916H_Packet pkt = {
    {0x5A, 0xFF, 0x01, 0x5A, 0x24, 0x21, 0x3D, 0x01, 0x83, 0x5A, 0xFF, 0x02, 0x5B},
    {0}, 0,
    {0x5A, 0xFF, 0x04, 0x5D}
  };
  memcpy(pkt.data, data, 144);
  pkt.checksum = chk8_pls916h(data);

  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  uint8_t *raw = (uint8_t *)&pkt;
  for (size_t i = 0; i < sizeof(pkt); i++) SPI.transfer(raw[i]);
  SPI.endTransaction();

  SPI.beginTransaction(SPISettings(200000, MSBFIRST, SPI_MODE0));
  SPI.transfer(0);
  SPI.endTransaction();
}

void setBit(int byteIndex, int bitIndex, bool on) {
  if (byteIndex < 0 || byteIndex >= 144) return;
  if (on) frame[byteIndex] |= (1 << bitIndex);
  else frame[byteIndex] &= ~(1 << bitIndex);
}


void displayDigit(int digitIndex, int value) {
  if (digitIndex < 0 || digitIndex > 5 || value < 0 || value > 9) return;

  // Clear this digit
  for (int seg = 0; seg < 7; seg++) {
    Segment s = digitSegments[digitIndex][seg];
    if (s.byteIndex >= 0) setBit(s.byteIndex, s.bitIndex, false);
  }
  // Set new value
  for (int seg = 0; seg < 7; seg++) {
    if (numberTable[value][seg]) {
      Segment s = digitSegments[digitIndex][seg];
      if (s.byteIndex >= 0) setBit(s.byteIndex, s.bitIndex, true);
    }
  }
}

void displayNumber(long num) {
  clearDigits();
  for (int i = 5; i >= 0; i--) {
    int digit = num % 10;
    num /= 10;
    displayDigit(i, digit);
    if (num == 0) break;
  }
}

void displayDigits(int d1, int d2, int d3, int d4, int d5, int d6) {
  clearDigits();
  int values[6] = {d1, d2, d3, d4, d5, d6};
  for (int i = 0; i < 6; i++) {
    if (values[i] >= 0 && values[i] <= 9)
      displayDigit(i, values[i]);
  }
}

void setup() {
  SPI.begin();
  Serial.begin(9600);
  memset(frame, 0, sizeof(frame));

  Serial.println("Adafruit AHT10/AHT20 demo!");

  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");
}


void loop() {
  // --- Continuous display refresh ---
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh >= 20) {
    write_pls916h(frame);
    lastRefresh = millis();
  }

  // --- Read sensor data every second ---
  static unsigned long lastRead = 0;
  static float temperatureC = 0;
  static float temperatureF = 0;
  static float humidityVal = 0;
  if (millis() - lastRead >= 1000) {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    temperatureC = temp.temperature;
    temperatureF = (temperatureC * 1.8) + 32;
    humidityVal = humidity.relative_humidity;
    lastRead = millis();
  }

  // --- Cycle between display modes every 3 seconds ---
  static unsigned long lastToggle = 0;
  static int displayMode = 0; // 0 = F, 1 = C, 2 = Humidity
  if (millis() - lastToggle >= 3000) {
    displayMode = (displayMode + 1) % 3;
    lastToggle = millis();
  }

  // --- Update display ---
  clearDigits();
  switch (displayMode) {
    case 0:  // °F
      displayNumber((int)temperatureF);
      Serial.println("Showing °F");
      setBit(16,7,1);
      setBit(17,7,1);
      setBit(18,7,1);
      setBit(19,7,1);
      setBit(20,7,1);
      setBit(21,7,1);
      setBit(22,7,1);
      setBit(23,7,1);
      setBit(24,7,1);
      setBit(25,7,1);
      setBit(26,7,1);
      setBit(27,7,1);
      setBit(5,7,1);
      setBit(6,7,1);
      setBit(4,7,1);
      setBit(0,7,1);
      break;

    case 1:  // °C
      displayNumber((int)temperatureC);
      Serial.println("Showing °C");
      setBit(16,7,1);
      setBit(17,7,1);
      setBit(18,7,1);
      setBit(19,7,1);
      setBit(20,7,1);
      setBit(21,7,1);
      setBit(22,7,1);
      setBit(23,7,1);
      setBit(24,7,1);
      setBit(25,7,1);
      setBit(26,7,1);
      setBit(27,7,1);
      setBit(5,7,1);
      setBit(3,7,1);
      setBit(4,7,1);
      setBit(0,7,1);
      break;

    case 2:  // Humidity
      displayNumber((int)humidityVal);
      Serial.println("Showing % humidity");
      setBit(16,7,1);
      setBit(17,7,1);
      setBit(18,7,1);
      setBit(19,7,1);
      setBit(20,7,1);
      setBit(21,7,1);
      setBit(22,7,1);
      setBit(23,7,1);
      setBit(24,7,1);
      setBit(25,7,1);
      setBit(26,7,1);
      setBit(27,7,1);
      setBit(6,7,1);
      setBit(5,7,1);
      setBit(1,7,1);
      setBit(4,7,1);
      setBit(2,7,1);
      break;
  }
}
