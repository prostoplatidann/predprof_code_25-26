 // ==========================================
// COMMANDER V3 (Final) - Пульт управления
// ==========================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// --- НАСТРОЙКИ РАДИО ---
RF24 radio(9, 10); // CE, CSN
const byte PIPE_CMD[] = "targe1"; // Труба для отправки команд
const byte PIPE_RX[]  = "main1";  // Труба для приема ответов
const int  RF_CHAN    = 100;      // Канал (должен совпадать с модулем)

// --- СТРУКТУРЫ ДАННЫХ ---
struct Packet {
  int16_t x;
  int16_t y;
  uint8_t cmd;
  char    ok[3]; // Маркер "OK"
};

struct Response {
  int16_t x;
  int16_t y;
  uint8_t status;
  char    done[5];
};

Packet  pkt;
Response resp;

void send_cmd(uint8_t cmd, int16_t x, int16_t y);
void handle_serial();
void parse_move(char *buf);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("COMMANDER SYSTEM V3"));
  Serial.println(F("-------------------"));
  Serial.println(F("Auto:   h (hor), v (ver), d1, d2, t (full test)"));
  Serial.println(F("Manual: m X Y (move +/-50 deg), s (stop)"));
  Serial.println(F("Zero:   z (set current pos as 0,0), c (goto 0,0)"));
  Serial.println(F("Laser:  lon (on), loff (off)"));
  Serial.println(F("-------------------"));

  if (!radio.begin()) {
    Serial.println(F("CRITICAL ERROR: NRF24 NOT FOUND"));
    while (1) { delay(1000); }
  }

  radio.setChannel(RF_CHAN);
  radio.setPALevel(RF24_PA_MIN); // MIN для тестов на столе, MAX для поля
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(15, 15);
  radio.openWritingPipe(PIPE_CMD);
  radio.openReadingPipe(1, PIPE_RX);
  radio.startListening();
}

void loop() {
  // 1. Слушаем ответы от модуля
  if (radio.available()) {
    radio.read(&resp, sizeof(resp));
    
    // Переводим "тысячи" обратно в градусы для отображения
    float xDeg = resp.x / 100.0;
    float yDeg = resp.y / 100.0;

    Serial.print(F("[RX] Pos: "));
    Serial.print(xDeg, 1);
    Serial.print(F(" : "));
    Serial.print(yDeg, 1);
    Serial.print(F(" | Mode: "));
    Serial.print(resp.status);
    Serial.print(F(" | Msg: "));
    Serial.println(resp.done);
  }

  // 2. Слушаем команды с клавиатуры ПК
  if (Serial.available()) {
    handle_serial();
  }
}

void handle_serial() {
  static char buf[32];
  // Читаем строку до перевода строки
  uint8_t len = Serial.readBytesUntil('\n', buf, sizeof(buf) - 1);
  if (len == 0) return;
  buf[len] = 0;
  // Удаляем символ возврата каретки, если есть
  if (len > 0 && (buf[len - 1] == '\r')) buf[len - 1] = 0;

  // Приводим к нижнему регистру
  for (uint8_t i = 0; buf[i]; i++) {
    if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] = buf[i] - 'A' + 'a';
  }
  if (buf[0] == 0) return;

  // --- ОБРАБОТКА КОМАНД ---
  
  // Автоматические режимы
  if (strcmp(buf, "h") == 0) {
    send_cmd(1, 0, 0); Serial.println(F("[TX] Horizontal Scan"));
  } else if (strcmp(buf, "v") == 0) {
    send_cmd(2, 0, 0); Serial.println(F("[TX] Vertical Scan"));
  } else if (strcmp(buf, "d1") == 0) {
    send_cmd(3, 0, 0); Serial.println(F("[TX] Diag Scan 1"));
  } else if (strcmp(buf, "d2") == 0) {
    send_cmd(4, 0, 0); Serial.println(F("[TX] Diag Scan 2"));
  } else if (strcmp(buf, "t") == 0) {
    send_cmd(8, 0, 0); Serial.println(F("[TX] FULL TEST CYCLE"));
  } 
  
  // Управление движением
  else if (strcmp(buf, "c") == 0) {
    send_cmd(6, 0, 0); Serial.println(F("[TX] Go to Center (0,0)"));
  } else if (strcmp(buf, "s") == 0) {
    send_cmd(7, 0, 0); Serial.println(F("[TX] EMERGENCY STOP"));
  } else if (strncmp(buf, "m ", 2) == 0) {
    parse_move(buf);
  } 
  
  // Управление нулем и лазером
  else if (strcmp(buf, "z") == 0) {
    send_cmd(11, 0, 0); Serial.println(F("[TX] SET ZERO (Tare)"));
  } else if (strcmp(buf, "lon") == 0) {
    send_cmd(9, 1, 0); Serial.println(F("[TX] Laser ON"));
  } else if (strcmp(buf, "loff") == 0) {
    send_cmd(9, 0, 0); Serial.println(F("[TX] Laser OFF"));
  } 
  else {
    Serial.println(F("[ERR] Unknown command."));
  }
}

void parse_move(char *buf) {
  // Формат: "m X Y" (например, "m 30 -10")
  char *p = buf + 2; 
  while (*p == ' ') p++; // Пропуск пробелов
  char *pSpace = strchr(p, ' ');
  
  if (!pSpace) {
    Serial.println(F("Use: m X Y"));
    return;
  }
  *pSpace = 0;
  
  int xDeg = atoi(p);
  int yDeg = atoi(pSpace + 1);
  
  // ЗАЩИТА АМПЛИТУДЫ НА ПУЛЬТЕ
  // Не даем отправить команду больше +/- 50 градусов
  if (xDeg < -50) xDeg = -50; 
  if (xDeg >  50) xDeg =  50;
  if (yDeg < -50) yDeg = -50; 
  if (yDeg >  50) yDeg =  50;

  Serial.print(F("[TX] Move: ")); Serial.print(xDeg); 
  Serial.print(" "); Serial.println(yDeg);

  // Отправляем в сотых долях (градусы * 100)
  send_cmd(5, xDeg * 100, yDeg * 100);
}

void send_cmd(uint8_t cmd, int16_t x, int16_t y) {
  pkt.x = x;
  pkt.y = y;
  pkt.cmd = cmd;
  strcpy(pkt.ok, "OK");

  radio.stopListening();
  radio.write(&pkt, sizeof(pkt));
  radio.startListening();
}