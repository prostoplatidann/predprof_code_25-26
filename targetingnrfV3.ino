// TARGETING MODULE FOR ARDUINO NANO - DEBUG ВЕРСИЯ
// CE=9, CSN=10

#include <RF24.h>
#include <Servo.h>
#include <SPI.h>

RF24 radio(9, 10);

const byte PIPE_RX[] = "targe1";
const byte PIPE_TX[] = "main1";
const int RF_CHAN = 100;

const int PIN_X = 5;
const int PIN_Y = 6;

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

Packet pkt;
Response resp;
Servo sx, sy;

int16_t cx = 0, cy = 0;
uint8_t mode = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║  TARGETING MODULE DEBUG - INITIALIZING ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  // Servos
  Serial.print("[INIT] Attaching servos...");
  sx.attach(PIN_X);
  sy.attach(PIN_Y);
  Serial.println(" OK");
  
  // RF24
  Serial.print("[INIT] RF24 begin...");
  if (!radio.begin()) {
    Serial.println(" FAILED!");
    Serial.println("[ERROR] NRF24 did not initialize");
    while(1) {
      Serial.println("[ERROR] NRF24 still not working...");
      delay(1000);
    }
  }
  Serial.println(" OK");
  
  Serial.print("[INIT] Setting channel "); Serial.print(RF_CHAN); Serial.println("...");
  radio.setChannel(RF_CHAN);
  Serial.println(" OK");
  
  Serial.print("[INIT] Setting PA level...");
  radio.setPALevel(RF24_PA_MIN);
  Serial.println(" OK");
  
  Serial.print("[INIT] Setting data rate...");
  radio.setDataRate(RF24_250KBPS);
  Serial.println(" OK");
  
  Serial.print("[INIT] Setting CRC...");
  radio.setCRCLength(RF24_CRC_16);
  Serial.println(" OK");
  
  Serial.print("[INIT] Setting retries...");
  radio.setRetries(15, 15);
  Serial.println(" OK");
  
  Serial.print("[INIT] Opening RX pipe: ");
  Serial.println((const char*)PIPE_RX);
  radio.openReadingPipe(1, PIPE_RX);
  
  Serial.print("[INIT] Opening TX pipe: ");
  Serial.println((const char*)PIPE_TX);
  radio.openWritingPipe(PIPE_TX);
  
  Serial.print("[INIT] Starting listening...");
  radio.startListening();
  Serial.println(" OK");
  
  set_pos(0, 0);
  send_resp(0);
  
  Serial.println();
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   TARGETING READY - WAITING COMMANDS   ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  Serial.println("[INFO] Listening on pipe: targe1");
  Serial.println("[INFO] Ready to receive commands from commander");
  Serial.println();
}

void loop() {
  if (radio.available()) {
    Serial.println("[RX] Data available!");
    radio.read(&pkt, sizeof(pkt));
    
    Serial.print("[RX] Received: cmd=");
    Serial.print(pkt.cmd);
    Serial.print(" x=");
    Serial.print(pkt.x);
    Serial.print(" y=");
    Serial.print(pkt.y);
    Serial.print(" ok='");
    Serial.print(pkt.ok);
    Serial.println("'");
    
    if (strcmp(pkt.ok, "OK") == 0) {
      print_cmd(pkt.cmd);
      exec_cmd(pkt.cmd, pkt.x, pkt.y);
    } else {
      Serial.println("[ERROR] Invalid packet signature!");
    }
  }
  
  send_resp(mode);
  delay(100);
}

void print_cmd(uint8_t c) {
  Serial.print("[RX] Команда: ");
  switch(c) {
    case 1: Serial.println("ГОРИЗОНТАЛЬНОЕ СКАНИРОВАНИЕ (h)"); break;
    case 2: Serial.println("ВЕРТИКАЛЬНОЕ СКАНИРОВАНИЕ (v)"); break;
    case 3: Serial.println("ДИАГОНАЛЬНОЕ СКАНИРОВАНИЕ (d)"); break;
    case 4: Serial.println("ПОЛНОЕ СКАНИРОВАНИЕ (f)"); break;
    case 5: 
      Serial.print("ДВИЖЕНИЕ: X=");
      Serial.print(pkt.x / 100.0, 1);
      Serial.print("° Y=");
      Serial.print(pkt.y / 100.0, 1);
      Serial.println("°");
      break;
    case 6: Serial.println("ЦЕНТР (c)"); break;
    case 7: Serial.println("СТОП (s)"); break;
    default: Serial.print("UNKNOWN ("); Serial.print(c); Serial.println(")");
  }
}

void exec_cmd(uint8_t c, int16_t x, int16_t y) {
  switch (c) {
    case 1: hscan(); break;
    case 2: vscan(); break;
    case 3: dscan(); break;
    case 4: fscan(); break;
    case 5: move_to(x, y); break;
    case 6: move_to(0, 0); break;
    case 7: mode = 0; break;
  }
  Serial.println("[OK] Команда выполнена\n");
}

void set_pos(int16_t x, int16_t y) {
  cx = constrain(x, -4000, 4000);
  cy = constrain(y, -4000, 4000);
  sx.write(cx);
  sy.write(cy);
  /*
  int px = 1500 + (cx / 100.0) * 11.11;
  int py = 1500 + (cy / 100.0) * 11.11;
  
  sx.writeMicroseconds(constrain(px, 1000, 2000));
  sy.writeMicroseconds(constrain(py, 1000, 2000));
  */
}

void hscan() {
  mode = 1;
  Serial.println("   ▶ Начало горизонтального сканирования...");
  for (int y = -4000; y <= 4000; y += 1000) {
    set_pos(0, y);
    delay(3000);
  }
  set_pos(0, 0);
  mode = 0;
  Serial.println("   ✓ Завершено");
}

void vscan() {
  mode = 1;
  Serial.println("   ▶ Начало вертикального сканирования...");
  for (int x = -4000; x <= 4000; x += 1000) {
    set_pos(x, 0);
    delay(3000);
  }
  set_pos(0, 0);
  mode = 0;
  Serial.println("   ✓ Завершено");
}

void dscan() {
  mode = 1;
  Serial.println("   ▶ Начало диагонального сканирования...");
  for (int p = -4000; p <= 4000; p += 1000) {
    set_pos(p, p);
    delay(3000);
  }
  set_pos(0, 0);
  mode = 0;
  Serial.println("   ✓ Завершено");
}

void fscan() {
  mode = 1;
  Serial.println("   ▶ ПОЛНОЕ сканирование (81 позиция)...");
  for (int x = -4000; x <= 4000; x += 1000) {
    for (int y = -4000; y <= 4000; y += 1000) {
      set_pos(x, y);
      delay(2000);
      Serial.print("   [");
      Serial.print(x / 100.0, 1);
      Serial.print("°, ");
      Serial.print(y / 100.0, 1);
      Serial.println("°]");
    }
  }
  set_pos(0, 0);
  mode = 0;
  Serial.println("   ✓ Завершено");
}

void move_to(int16_t x, int16_t y) {
  mode = 2;
  x = constrain(x, -4000, 4000);
  y = constrain(y, -4000, 4000);
  
  Serial.print("   ▶ Движение на X=");
  Serial.print(x / 100.0, 1);
  Serial.print("° Y=");
  Serial.print(y / 100.0, 1);
  Serial.println("°");
  
  int16_t sx_start = cx, sy_start = cy;
  unsigned long st = millis();
  
  while (millis() - st < 2000) {
    float p = (float)(millis() - st) / 2000.0;
    int16_t nx = sx_start + (x - sx_start) * p;
    int16_t ny = sy_start + (y - sy_start) * p;
    set_pos(nx, ny);
    delay(50);
  }
  
  set_pos(x, y);
  Serial.print("   ✓ Позиция: X=");
  Serial.print(cx / 100.0, 1);
  Serial.print("° Y=");
  Serial.print(cy / 100.0, 1);
  Serial.println("°");
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
