// TARGETING MODULE FOR ARDUINO NANO - OPTIMIZED FOR MEMORY
// CE=9, CSN=10

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

RF24 radio(9, 10);

const byte PIPE_RX[] = "targe1";
const byte PIPE_TX[] = "main1";
const int RF_CHAN = 100;

const int PIN_X     = 5;
const int PIN_Y     = 6;
const int LASER_PIN = 7;

struct Packet {
  int16_t x;
  int16_t y;
  uint8_t cmd;
  char ok[3];
};

struct Response {
  int16_t x;
  int16_t y;
  uint8_t status;
  char done[5];
};

Packet   pkt;
Response resp;
Servo sx, sy;

int16_t cx = 0, cy = 0;
uint8_t mode = 0;

void laser_on() {
  digitalWrite(LASER_PIN, HIGH);
}

void laser_off() {
  digitalWrite(LASER_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n[TARGETING] Starting..."));

  // Servos
  Serial.print(F("[INIT] Servos..."));
  sx.attach(PIN_X);
  sy.attach(PIN_Y);
  Serial.println(F(" OK"));
  pinMode(LASER_PIN, OUTPUT);
  laser_off();

  // RF24
  Serial.print(F("[INIT] RF24..."));
  if (!radio.begin()) {
    Serial.println(F(" FAIL"));
    while (1) {
      Serial.println(F("[ERR] NRF24 init failed"));
      delay(1000);
    }
  }
  Serial.println(F(" OK"));

  radio.setChannel(RF_CHAN);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(15, 15);
  radio.openReadingPipe(1, PIPE_RX);
  radio.openWritingPipe(PIPE_TX);
  radio.startListening();

  set_pos(0, 0);
  send_resp(0);

  Serial.println(F("[TARGETING] Ready"));
}

void loop() {
  if (radio.available()) {
    radio.read(&pkt, sizeof(pkt));

    if (strcmp(pkt.ok, "OK") == 0) {
      exec_cmd(pkt.cmd, pkt.x, pkt.y);
    } else {
      Serial.println(F("[ERR] Bad packet"));
    }

    send_resp(mode);
    delay(100);
  }
}

void exec_cmd(uint8_t c, int16_t x, int16_t y) {
  switch (c) {
    case 1: hscan();         break;
    case 2: vscan();         break;
    case 3: dscan1();        break;
    case 4: dscan2();        break;
    case 5: move_to(x, y);   break;
    case 6: move_to(0, 0);   break;
    case 7:
      mode = 0;
      laser_off();
      break;
  }
}

void set_pos(int16_t x, int16_t y) {
  cx = constrain(x, -4000, 4000);
  cy = constrain(y, -4000, 4000);

  sx.write(map(cx, -4000, 4000, 0, 180));
  sy.write(map(cy, -4000, 4000, 0, 180));
}

void hscan() {
  mode = 1;
  Serial.println(F("[HSCAN] Start"));
  laser_on();
  for (int y = -4000; y <= 4000; y += 1000) {
    set_pos(0, y);
    delay(3000);
    send_resp(mode);  // ← ДОБАВИТЬ
  }
  set_pos(0, 0);
  laser_off();
  mode = 0;
  send_resp(0);  // ← ДОБАВИТЬ
  Serial.println(F("[HSCAN] Done"));
}

void vscan() {
  mode = 1;
  Serial.println(F("[VSCAN] Start"));
  laser_on();
  for (int x = -4000; x <= 4000; x += 1000) {
    set_pos(x, 0);
    delay(3000);
    send_resp(mode);  // ← ДОБАВИТЬ
  }
  set_pos(0, 0);
  laser_off();
  mode = 0;
  send_resp(0);  // ← ДОБАВИТЬ
  Serial.println(F("[VSCAN] Done"));
}

void dscan1() {
  mode = 1;
  Serial.println(F("[DSCAN1] Start"));
  laser_on();
  for (int p = -4000; p <= 4000; p += 1000) {
    set_pos(p, p);
    delay(3000);
    send_resp(mode);  // ← ДОБАВИТЬ
  }
  set_pos(0, 0);
  laser_off();
  mode = 0;
  send_resp(0);  // ← ДОБАВИТЬ
  Serial.println(F("[DSCAN1] Done"));
}

void dscan2() {
  mode = 1;
  Serial.println(F("[DSCAN2] Start"));
  laser_on();
  int16_t x = -4000;
  int16_t y = 4000;
  while (x <= 4000 && y >= -4000) {
    set_pos(x, y);
    delay(3000);
    send_resp(mode);  // ← ДОБАВИТЬ
    x += 1000;
    y -= 1000;
  }
  set_pos(0, 0);
  laser_off();
  mode = 0;
  send_resp(0);  // ← ДОБАВИТЬ
  Serial.println(F("[DSCAN2] Done"));
}


void move_to(int16_t x, int16_t y) {
  mode = 2;

  x = constrain(x, -4000, 4000);
  y = constrain(y, -4000, 4000);

  Serial.print(F("[MOVE] X="));
  Serial.print(x / 100.0, 1);
  Serial.print(F(" Y="));
  Serial.println(y / 100.0, 1);

  int16_t sx_start = cx;
  int16_t sy_start = cy;

  unsigned long st = millis();
  laser_on();
  
  while (millis() - st < 2000) {
    float p = (float)(millis() - st) / 2000.0;
    int16_t nx = sx_start + (int16_t)((x - sx_start) * p);
    int16_t ny = sy_start + (int16_t)((y - sy_start) * p);
    set_pos(nx, ny);
    delay(50);
  }

  set_pos(x, y);
  laser_off();

  Serial.println(F("[MOVE] Done"));

  mode = 0;
}

void send_resp(uint8_t st) {
  radio.stopListening();
  resp.x = cx;
  resp.y = cy;
  resp.status = st;
  strcpy(resp.done, "DONE");
  radio.write(&resp, sizeof(resp));
  radio.startListening();
}