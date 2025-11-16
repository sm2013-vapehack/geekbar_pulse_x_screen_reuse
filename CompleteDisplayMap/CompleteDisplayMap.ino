#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>   // sscanf
#include <string.h>

// ==================== PLS916H Protocol/Frame ====================
struct PLS916H_Packet {
  uint8_t header[13];
  uint8_t data[144];
  uint8_t checksum;
  uint8_t tail[4];
} __attribute__((packed));

// Buffers
static uint8_t frameDigits[144];  // digit map
static uint8_t frameHalo[144];    // halo / loose LEDs
static uint8_t frameOut[144];     // final composition

struct Segment { int byteIndex; int bitIndex; };

// ==================== Per-digit Mapping ====================
// Segment order: A,B,C,D,E,F,G
// For 0 and 1 (partial digits) only two segments exist (to draw a "1").
static Segment digitSegments[6][7] = {
  // Digit 0 (partial, upper row, “hundreds”):      A        B       C        D        E        F        G
  {{-1,-1}, { 7,3}, {15,3}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}},
  // Digit 1 (partial, lower row, “hundreds”)
  {{-1,-1}, {47,3}, {55,3}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}},
  // Full digits (two upper and two lower)
  {{0,3},{1,3},{2,3},{3,3},{4,3},{5,3},{6,3}},              // Digit 2 (upper tens)
  {{8,3},{9,3},{10,3},{11,3},{12,3},{13,3},{14,3}},         // Digit 3 (upper ones)
  {{40,3},{41,3},{42,3},{43,3},{44,3},{45,3},{46,3}},       // Digit 4 (lower tens)
  {{48,3},{49,3},{50,3},{51,3},{52,3},{53,3},{54,3}}        // Digit 5 (lower ones)
};

// Table 0–9 (A..G)
static const bool numberTable[10][7] = {
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

// ==================== Halo ====================
static const Segment haloMap[] = {
   {30,5},{31,5},{32,5},{33,5},{34,5},{35,5},{36,5},{37,5},{38,5},{39,5},{56,5},{57,5},{58,5},{59,5}
};
static const int HALO_COUNT = sizeof(haloMap) / sizeof(haloMap[0]);

// ==================== Frame Utilities ====================
static inline void setBitBuf(uint8_t *buf, int byteIndex, int bitIndex, bool on) {
  if (byteIndex < 0 || byteIndex >= 144) return;
  if (bitIndex < 0 || bitIndex > 7) return;
  if (on) buf[byteIndex] |=  (uint8_t)(1 << bitIndex);
else    buf[byteIndex] &= ~(uint8_t)(1 << bitIndex);
}
static inline void setDigitBit(int byteIndex, int bitIndex, bool on) { setBitBuf(frameDigits, byteIndex, bitIndex, on); }
static inline void setHaloBitRaw(int byteIndex, int bitIndex, bool on) { setBitBuf(frameHalo, byteIndex, bitIndex, on); }

static inline void composeFrame() {
  for (int i = 0; i < 144; i++) frameOut[i] = (uint8_t)(frameDigits[i] | frameHalo[i]);
}

static uint8_t chk8_pls916h(const uint8_t data[144]) {
  uint8_t b = 0;
  for (int i = 0; i < 144; i++) b = (uint8_t)(b + data[i]);
  return b;
}

static void write_pls916h(const uint8_t data[144]) {
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

  // flush pulse
  SPI.beginTransaction(SPISettings(200000, MSBFIRST, SPI_MODE0));
  SPI.transfer(0);
  SPI.endTransaction();
}

static inline void pushFrameNow() { composeFrame(); write_pls916h(frameOut); }

// ==================== Digit Control ====================
static void digitsClear() { memset(frameDigits, 0, sizeof(frameDigits)); }
static void haloClear()   { memset(frameHalo,   0, sizeof(frameHalo));   }
static void allClear()    { digitsClear(); haloClear(); }

static void clearDigit(int digitIndex) {
  if (digitIndex < 0 || digitIndex > 5) return;
  for (int seg = 0; seg < 7; seg++) {
    Segment s = digitSegments[digitIndex][seg];
    if (s.byteIndex >= 0) setDigitBit(s.byteIndex, s.bitIndex, false);
  }
}

// *********** MAIN FIX: partial digits no longer erase anything unless showing "1" ***********
static void displayDigit(int digitIndex, int value) {
  if (digitIndex < 0 || digitIndex > 5 || value < 0 || value > 9) return;

  // Partial digits (digitIndex 0 and 1)
  if (digitIndex <= 1) {
    if (value != 1) return;
    clearDigit(digitIndex);
    for (int seg = 0; seg < 7; seg++) {
      Segment s = digitSegments[digitIndex][seg];
      if (s.byteIndex >= 0) setDigitBit(s.byteIndex, s.bitIndex, true);
    }
    return;
  }

  // Full digits
  clearDigit(digitIndex);
  for (int seg = 0; seg < 7; seg++) {
    if (numberTable[value][seg]) {
      Segment s = digitSegments[digitIndex][seg];
      if (s.byteIndex >= 0) setDigitBit(s.byteIndex, s.bitIndex, true);
    }
  }
}

// Kept for "digits" command
static void displayDigits6(int d1, int d2, int d3, int d4, int d5, int d6) {
  digitsClear();
  const int v[6] = {d1,d2,d3,d4,d5,d6};
  for (int i = 0; i < 6; i++)
    if (v[i] >= 0 && v[i] <= 9) displayDigit(i, v[i]);
}

// ==================== Halo helpers ====================
static void setHalo(int idx, bool on) {
  if (idx < 0 || idx >= HALO_COUNT) return;
  const Segment s = haloMap[idx];
  setHaloBitRaw(s.byteIndex, s.bitIndex, on);
}
static void haloAll(bool on) { for (int i = 0; i < HALO_COUNT; i++) setHalo(i, on); }
static void haloChase(int delayMs = 40) {
  for (int i = 0; i < HALO_COUNT; i++) {
    setHalo(i, true); pushFrameNow(); delay(delayMs);
    setHalo(i, false); pushFrameNow();
  }
}

// ==================== Scanner ====================
static void blinkBitDigits(int byteIndex, int bitIndex, int msOn = 120, int msOff = 80) {
  if (byteIndex < 0 || byteIndex >= 144 || bitIndex < 0 || bitIndex > 7) return;
  uint8_t old = frameDigits[byteIndex];

  frameDigits[byteIndex] = (uint8_t)(old & ~(1 << bitIndex));
  pushFrameNow(); delay(msOff);

  frameDigits[byteIndex] = (uint8_t)(old |  (1 << bitIndex));
  pushFrameNow(); delay(msOn);

  frameDigits[byteIndex] = old;
  pushFrameNow();
}

static void scanAll() {
  Serial.println(F("SCAN start (byte 0..143, bit 0..7). Watch and note."));
  uint8_t haloBackup[144]; memcpy(haloBackup, frameHalo, 144);
  memset(frameHalo, 0, 144); pushFrameNow();

  for (int b = 0; b < 144; b++) for (int bit = 0; bit < 8; bit++) {
    Serial.print(F("Blink ")); Serial.print(b); Serial.print(F(",")); Serial.println(bit);
    blinkBitDigits(b, bit, 120, 80);
  }

  memcpy(frameHalo, haloBackup, 144); pushFrameNow();
  Serial.println(F("SCAN done"));
}

// ==================== Help ====================
static void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  num N                 -> Show number N (0..999999) in 2 rows (3+3)"));
  Serial.println(F("  dclear                -> Clear DIGITS only"));
  Serial.println(F("  hclear                -> Clear HALO only"));
  Serial.println(F("  all 1|0               -> All bits ON/OFF"));
  Serial.println(F("  clear                 -> Clear both buffers"));
  Serial.println(F("  scan                  -> Scan all bits (blink)"));
  Serial.println(F("  hall 1|0              -> HALO ON/OFF"));
  Serial.println(F("  hchase [ms]           -> HALO chase effect"));
  Serial.println(F("  digits a b c d e f    -> Set 6 digits directly"));
  Serial.println(F("  <byte>,<bit> / !<byte>,<bit> / h: / d:  -> Set/Clear raw bit"));
}

// ==================== Parser "<byte>,<bit>" ====================
static bool handleCommaAddress(String cmd) {
  bool toHalo   = false;
  bool toDigits = true;
  bool turnOn   = true;

  cmd.trim();
  if (cmd.startsWith("h:") || cmd.startsWith("H:")) { toHalo = true; toDigits = false; cmd = cmd.substring(2); cmd.trim(); }
  else if (cmd.startsWith("d:") || cmd.startsWith("D:")) { toHalo = false; toDigits = true; cmd = cmd.substring(2); cmd.trim(); }
  if (cmd.startsWith("!")) { turnOn = false; cmd = cmd.substring(1); cmd.trim(); }

  int comma = cmd.indexOf(',');
  if (comma < 0) return false;
  String sb = cmd.substring(0, comma); sb.trim();
  String st = cmd.substring(comma + 1); st.trim();
  if (sb.length() == 0 || st.length() == 0) return false;

  int b = sb.toInt();
  int bit = st.toInt();
  if (!(b >= 0 && b < 144 && bit >= 0 && bit <= 7)) return false;

  if (toHalo)   setHaloBitRaw(b, bit, turnOn);
  if (toDigits) setDigitBit(b, bit, turnOn);
  pushFrameNow();
  return true;
}

// ==================== Render by rows ====================
// row 0 = top (digits: 0 [partial], 2, 3)
// row 1 = bottom (digits: 1 [partial], 4, 5)
// value < 0 clears the row
static void displayRow(int row, int value) {
  const int idx_parcial = (row == 0) ? 0 : 1;
  const int idx_tens    = (row == 0) ? 2 : 4;
  const int idx_ones    = (row == 0) ? 3 : 5;

  clearDigit(idx_parcial);
  clearDigit(idx_tens);
  clearDigit(idx_ones);

  if (value < 0) return;

  int hundreds = value / 100;
  int tens     = (value / 10) % 10;
  int ones     = value % 10;

  if (hundreds == 1) displayDigit(idx_parcial, 1);

  if (value >= 10) displayDigit(idx_tens, tens);
  displayDigit(idx_ones, ones);
}

// N in two rows: <=999 only top row; >999 splits into 3+3
static void displayNumberTwoRows(long num) {
  if (num < 0) num = 0;
  if (num > 999999) num = 999999;

  digitsClear();

  if (num <= 999) {
    displayRow(0, (int)num);
    displayRow(1, -1);
  } else {
    int top    = (int)(num / 1000L);
    int bottom = (int)(num % 1000L);
    displayRow(0, top);
    displayRow(1, bottom);
  }
  pushFrameNow();
}

void setup() {
  SPI.begin();
  Serial.begin(115200);
  allClear();

 

  // Turn on specific digit LED bits
  setDigitBit(1, 3, true);
  setDigitBit(2, 3, true);
  setDigitBit(4, 3, true);
  setDigitBit(5, 3, true);
  setDigitBit(6, 3, true);
  setDigitBit(9, 3, true);
  setDigitBit(10, 3, true);


   // ===== INITIAL LED STATE =====
  haloChase(80); 

  // ===== END INITIAL LED STATE =====

  pushFrameNow();

  Serial.println(F("PLS916H display ready."));
  printHelp();
}

void loop() {
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh >= 20) {
    pushFrameNow();
    lastRefresh = millis();
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (handleCommaAddress(cmd)) { Serial.println(F("OK")); return; }

    if (cmd == "help") {
      printHelp();
    }
    else if (cmd.startsWith("num")) {
      long val = 0;
      if (sscanf(cmd.c_str(), "num %ld", &val) == 1) {
        displayNumberTwoRows(val);
        Serial.println(F("OK"));
      } else {
        Serial.println(F("ERR"));
      }
    }
    else if (cmd.startsWith("digit")) {
      int pos = 0, val = 0;
      if (sscanf(cmd.c_str(), "digit %d %d", &pos, &val) == 2 && pos >= 1 && pos <= 6 && val >= 0 && val <= 9) {
        digitsClear(); displayDigit(pos - 1, val); pushFrameNow(); Serial.println(F("OK"));
      } else Serial.println(F("ERR"));
    }
    else if (cmd.startsWith("digits")) {
      int a,b,c,d,e,f;
      if (sscanf(cmd.c_str(), "digits %d %d %d %d %d %d", &a,&b,&c,&d,&e,&f) == 6) {
        displayDigits6(a,b,c,d,e,f); pushFrameNow(); Serial.println(F("OK"));
      } else Serial.println(F("ERR"));
    }
    else if (cmd.startsWith("set")) {
      int x,y; if (sscanf(cmd.c_str(), "set %d %d", &x,&y) == 2) { setDigitBit(x,y,true);  pushFrameNow(); Serial.println(F("OK")); }
      else Serial.println(F("ERR"));
    }
    else if (cmd.startsWith("clr")) {
      int x,y; if (sscanf(cmd.c_str(), "clr %d %d", &x,&y) == 2) { setDigitBit(x,y,false); pushFrameNow(); Serial.println(F("OK")); }
      else Serial.println(F("ERR"));
    }
    else if (cmd == "dclear") { digitsClear(); pushFrameNow(); Serial.println(F("OK")); }
    else if (cmd == "hclear") { haloClear();   pushFrameNow(); Serial.println(F("OK")); }
    else if (cmd == "clear")  { allClear();    pushFrameNow(); Serial.println(F("OK")); }
    else if (cmd == "scan")   { scanAll();     Serial.println(F("OK")); }
    else if (cmd.startsWith("blink")) {
      int x,y,onMs=120,offMs=80;
      int n = sscanf(cmd.c_str(), "blink %d %d %d %d", &x,&y,&onMs,&offMs);
      if (n >= 2) { blinkBitDigits(x,y,onMs,offMs); Serial.println(F("OK")); } else Serial.println(F("ERR"));
    }
    else if (cmd.startsWith("hset")) {
      int idx,on; if (sscanf(cmd.c_str(), "hset %d %d", &idx,&on) == 2) { setHalo(idx,on!=0); pushFrameNow(); Serial.println(F("OK")); }
      else Serial.println(F("ERR"));
    }
    else if (cmd.startsWith("hall")) {
      int on; if (sscanf(cmd.c_str(), "hall %d", &on) == 1) { haloAll(on!=0); pushFrameNow(); Serial.println(F("OK")); }
      else Serial.println(F("ERR"));
    }
    else if (cmd.startsWith("hchase")) {
      int ms = 40; sscanf(cmd.c_str(), "hchase %d", &ms); haloChase(ms); Serial.println(F("OK"));
    }
    else if (cmd.startsWith("hraw")) {
      int x,y,on; if (sscanf(cmd.c_str(), "hraw %d %d %d", &x,&y,&on) == 3) { setHaloBitRaw(x,y,on!=0); pushFrameNow(); Serial.println(F("OK")); }
      else Serial.println(F("ERR"));
    }
    else if (cmd.startsWith("all")) {
      int on; if (sscanf(cmd.c_str(), "all %d", &on) == 1) {
        memset(frameDigits, on ? 0xFF : 0x00, sizeof(frameDigits));
        memset(frameHalo,   on ? 0xFF : 0x00, sizeof(frameHalo));
        pushFrameNow();
        Serial.println(on ? F("All ON") : F("All OFF"));
      } else Serial.println(F("ERR"));
    }
    else {
      Serial.println(F("Unknown command. Type 'help'."));
    }
  }
}



