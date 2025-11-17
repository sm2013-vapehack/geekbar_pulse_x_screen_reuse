#include <Arduino.h>
#include <SPI.h>

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

void clearDigits() { memset(frame, 0, sizeof(frame)); }

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

  Serial.println("Commands:");
  Serial.println(" num N           -> Show number N (0-999999)");
  Serial.println(" digit X Y       -> Show digit Y on position X (1-6)");
  Serial.println(" digits a b c d e f -> Show 6 digits (use -1 for blank)");
  Serial.println(" set X Y         -> Turn ON byte X bit Y");
  Serial.println(" clr X Y         -> Turn OFF byte X bit Y");
  Serial.println(" all 1           -> Turn all segments ON");
  Serial.println(" all 0           -> Turn all segments OFF");
  Serial.println(" clear           -> Clear display");
}

void loop() {
  // Refresh display
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh >= 20) {
    write_pls916h(frame);
    lastRefresh = millis();
  }

  // Handle Serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("num")) {
      long val;
      sscanf(cmd.c_str(), "num %ld", &val);
      displayNumber(val);
      Serial.println("OK");
    }
    else if (cmd.startsWith("digit")) {
      int pos, val;
      sscanf(cmd.c_str(), "digit %d %d", &pos, &val);
      clearDigits();
      displayDigit(pos - 1, val);
      Serial.println("OK");
    }
    else if (cmd.startsWith("digits")) {
      int a,b,c,d,e,f;
      sscanf(cmd.c_str(), "digits %d %d %d %d %d %d", &a,&b,&c,&d,&e,&f);
      displayDigits(a,b,c,d,e,f);
      Serial.println("OK");
    }
    else if (cmd.startsWith("set")) {
      int x, y;
      sscanf(cmd.c_str(), "set %d %d", &x, &y);
      setBit(x, y, true);
      Serial.println("OK");
    }
    else if (cmd.startsWith("clr")) {
      int x, y;
      sscanf(cmd.c_str(), "clr %d %d", &x, &y);
      setBit(x, y, false);
      Serial.println("OK");
    }
    else if (cmd == "all 1") {
      memset(frame, 0xFF, sizeof(frame));
      Serial.println("All ON");
    }
    else if (cmd == "all 0") {
      memset(frame, 0x00, sizeof(frame));
      Serial.println("All OFF");
    }
    else if (cmd == "clear") {
      clearDigits();
      Serial.println("Cleared");
    }
    else {
      Serial.println("Unknown command");
    }
  }
}
