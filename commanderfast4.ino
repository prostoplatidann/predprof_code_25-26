
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>


RF24 radio(9, 10);

const byte PIPE_CMD[]    = "targe1"; 
const byte PIPE_RX_LOG[] = "cmdr1";
const int  RF_CHAN       = 100;


struct DataPackage {
  char    mode;
  int16_t x;   
  int16_t y;   
  uint8_t laser;
};

DataPackage data;

void setup() {
  Serial.begin(115200);
  Serial.println(F("COMMANDER READY."));
  Serial.println(F("Examples: G1 X10 Y20 U1 | M1 | G28 | Z"));

  radio.begin();
  radio.setChannel(RF_CHAN);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);

  radio.openWritingPipe(PIPE_CMD);

  radio.openReadingPipe(1, PIPE_RX_LOG);

  radio.startListening();
}

void loop() {
  if (radio.available()) {
    DataPackage pkt;
    radio.read(&pkt, sizeof(pkt));
    if (pkt.mode == 'L') {
      Serial.print(F("TARGET LOG: X="));
      Serial.print(pkt.x);
      Serial.print(F(" Y="));
      Serial.println(pkt.y);
    }
  }

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();
    if (input.length() == 0) return;

    radio.stopListening();
    parseAndSend(input);
    radio.startListening();
  }
}

void parseAndSend(String s) {
  memset(&data, 0, sizeof(data));

  if (s.startsWith("G")) {
    int gCode = s.substring(1).toInt();

    if (gCode == 28) {
      data.mode = 'H';
      Serial.println(F(">> Sent: HOME (G28)"));
    }
    else if (gCode == 1) {
      data.mode = 'G';

      data.x     = extractValue(s, 'X');
      data.y     = extractValue(s, 'Y');
      data.laser = extractValue(s, 'U');

      Serial.print(F(">> Sent: Move X")); Serial.print(data.x);
      Serial.print(F(" Y")); Serial.print(data.y);
      Serial.print(F(" Laser:")); Serial.println(data.laser);
    }
  }

  else if (s.startsWith("M")) {
    data.mode = 'M';
    data.x = s.substring(1).toInt();

    Serial.print(F(">> Sent: Macro M")); Serial.println(data.x);
  }

  else if (s.startsWith("Z")) {
    data.mode = 'Z';
    Serial.println(F(">> Sent: Set Zero"));
  }

  radio.write(&data, sizeof(data));
}

int extractValue(String s, char key) {
  int idx = s.indexOf(key);
  if (idx == -1) return 0;

  int endIdx = idx + 1;
  while (endIdx < s.length() && (isDigit(s[endIdx]) || s[endIdx] == '-')) {
    endIdx++;
  }

  return s.substring(idx + 1, endIdx).toInt();
}
