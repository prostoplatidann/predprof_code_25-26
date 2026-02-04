 // ==========================================
// TARGETING MODULE V3 (Final) - Исполнитель
// ==========================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

// --- ПИНЫ ---
const int PIN_SERVO_X = 5;  // Ось наклона (или поворота)
const int PIN_SERVO_Y = 6;  // Ось поворота (или наклона)
const int PIN_LASER   = A0; // Лазер

// --- НАСТРОЙКИ РАДИО ---
RF24 radio(9, 10);
const byte PIPE_RX[] = "targe1";
const byte PIPE_TX[] = "main1";
const int RF_CHAN = 100;

// --- ПЕРЕМЕННЫЕ СИСТЕМЫ КООРДИНАТ ---
// 100 единиц = 1 градус. Диапазон +/- 9000 (90 градусов)
// 0 логический = Центр, заданный пользователем
int16_t offset_x = 0;    // Смещение нуля относительно физического центра
int16_t offset_y = 0;

int16_t current_x = 0;   // Текущая ЛОГИЧЕСКАЯ позиция X
int16_t current_y = 0;   // Текущая ЛОГИЧЕСКАЯ позиция Y

uint8_t mode = 0;        // Текущий режим работы

// --- ОБЪЕКТЫ ---
Servo sx, sy;
struct Packet { int16_t x; int16_t y; uint8_t cmd; char ok[3]; };
struct Response { int16_t x; int16_t y; uint8_t status; char done[5]; };
Packet pkt;
Response resp;

// --- ПРОТОТИПЫ ФУНКЦИЙ ---
void laser_on();
void laser_off();
void set_pos(int16_t rel_x, int16_t rel_y);
void exec_cmd(uint8_t c, int16_t x, int16_t y);
void send_resp(uint8_t st);
// Режимы сканирования
void hscan();
void vscan();
void dscan1();
void dscan2();
void full_cycle();
void move_to(int16_t x, int16_t y);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("[TARGETING V3] Starting..."));

  // Настройка серво
  sx.attach(PIN_SERVO_X);
  sy.attach(PIN_SERVO_Y);

  // Настройка лазера
  pinMode(PIN_LASER, OUTPUT);
  laser_off();

  // Настройка радио
  if (!radio.begin()) {
    Serial.println(F("[ERR] NRF24 FAIL"));
    while (1) delay(1000);
  }
  radio.setChannel(RF_CHAN);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(15, 15);
  radio.openReadingPipe(1, PIPE_RX);
  radio.openWritingPipe(PIPE_TX);
  radio.startListening();

  // Установка начального положения (физический центр 90 град)
  // Изначально смещение 0, поэтому 0 логический = 90 физический
  set_pos(0, 0); 
  send_resp(0);
  
  Serial.println(F("[TARGETING V3] Ready"));
}

void loop() {
  if (radio.available()) {
    radio.read(&pkt, sizeof(pkt));
    if (strcmp(pkt.ok, "OK") == 0) {
      exec_cmd(pkt.cmd, pkt.x, pkt.y);
    }
  }
}

// === ГЛАВНАЯ ФУНКЦИЯ ПОЗИЦИОНИРОВАНИЯ ===
void set_pos(int16_t rel_x, int16_t rel_y) {
  // 1. ЗАЩИТА АМПЛИТУДЫ (Логическая)
  // Запрещаем уходить дальше +/- 50 градусов от ТЕКУЩЕГО НУЛЯ
  rel_x = constrain(rel_x, -5000, 5000);
  rel_y = constrain(rel_y, -5000, 5000);

  // Обновляем глобальные переменные (где мы думаем, что мы находимся)
  current_x = rel_x;
  current_y = rel_y;

  // 2. РАСЧЕТ ФИЗИЧЕСКОГО УГЛА
  // Абсолютная позиция = Желаемая относительная + Смещение нуля
  int32_t abs_x = (int32_t)rel_x + offset_x;
  int32_t abs_y = (int32_t)rel_y + offset_y;

  // 3. ЗАЩИТА МЕХАНИКИ (Физическая)
  // Серво не может выйти за +/- 90 градусов (0..180 физических)
  if (abs_x < -9000) abs_x = -9000; 
  if (abs_x >  9000) abs_x =  9000;
  
  if (abs_y < -9000) abs_y = -9000; 
  if (abs_y >  9000) abs_y =  9000;

  // 4. УПРАВЛЕНИЕ СЕРВОПРИВОДАМИ
  // map: -9000 -> 0 град, 0 -> 90 град, 9000 -> 180 град
  int sx_val = map(abs_x, -9000, 9000, 0, 180);
  int sy_val = map(abs_y, -9000, 9000, 0, 180);

  sx.write(sx_val);
  sy.write(sy_val);
}

void exec_cmd(uint8_t c, int16_t x, int16_t y) {
  Serial.print(F("CMD: ")); Serial.println(c);
  
  switch (c) {
    case 1: hscan(); break;
    case 2: vscan(); break;
    case 3: dscan1(); break;
    case 4: dscan2(); break;
    
    case 5: move_to(x, y); break; // Ручное движение
    case 6: move_to(0, 0); break; // Возврат в ЛОГИЧЕСКИЙ ноль
    
    case 7: // STOP
      mode = 0;
      laser_off();
      send_resp(mode);
      break;
      
    case 8: full_cycle(); break;
    
    case 9: // Управление лазером
      if (x == 1) { laser_on(); Serial.println(F("L: ON")); }
      else        { laser_off(); Serial.println(F("L: OFF")); }
      send_resp(mode);
      break;

    case 11: // КОМАНДА "Z" - ОБНУЛЕНИЕ (TARE)
      // Добавляем текущую логическую позицию к смещению
      offset_x += current_x;
      offset_y += current_y;
      
      // Теперь мы логически в нуле
      current_x = 0;
      current_y = 0;
      
      Serial.print(F("NEW ZERO. Offsets: ")); 
      Serial.print(offset_x); Serial.print(" "); Serial.println(offset_y);
      
      // Физически сервоприводы не двигаются, но система координат обновилась
      set_pos(0, 0); 
      send_resp(mode);
      break;

    default: break;
  }
}

// === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===

void laser_on() { digitalWrite(PIN_LASER, HIGH); }
void laser_off() { digitalWrite(PIN_LASER, LOW); } // Исправлено на LOW

void send_resp(uint8_t st) {
  radio.stopListening();
  resp.x = current_x;
  resp.y = current_y;
  resp.status = st;
  strcpy(resp.done, "DONE");
  radio.write(&resp, sizeof(resp));
  radio.startListening();
}

void move_to(int16_t x, int16_t y) {
  mode = 2;
  // Интерполяция движения от current_x/y до x/y
  int16_t start_x = current_x;
  int16_t start_y = current_y;
  
  unsigned long st = millis();
  laser_on(); // Лазер включен при движении
  
  while (millis() - st < 2000) { // Движение занимает 2 секунды
    float p = (float)(millis() - st) / 2000.0;
    int16_t next_x = start_x + (int16_t)((x - start_x) * p);
    int16_t next_y = start_y + (int16_t)((y - start_y) * p);
    set_pos(next_x, next_y);
    delay(40); // ~25 кадров в секунду
  }
  set_pos(x, y); // Доводка до финала
  
  // Примечание: после ручного движения лазер оставляем как был или гасим? 
  // По ТЗ обычно гасят после цикла, но в ручном режиме удобнее видеть точку.
  // Тут оставим включенным, чтобы видеть, куда приехали.
  // Если нужно выключить - раскомментируйте строку ниже:
  // laser_off(); 
  
  mode = 0;
  send_resp(mode);
}

// === РЕЖИМЫ СКАНИРОВАНИЯ (АВТОМАТИКА) ===
// Все циклы работают относительно УСТАНОВЛЕННОГО НУЛЯ
// Амплитуда +/- 40 градусов (4000 единиц)

void hscan() {
  mode = 1;
  laser_on();
  // X: -40..40, Y: 0
  for (int a = -4000; a <= 4000; a += 1000) {
    set_pos(a, 0);
    delay(3000); // 3 секунды пауза по ТЗ
    send_resp(mode);
  }
  set_pos(0, 0); // Возврат в ноль
  laser_off();
  mode = 0;
  send_resp(mode);
}

void vscan() {
  mode = 1;
  laser_on();
  // X: 0, Y: -40..40
  for (int a = -4000; a <= 4000; a += 1000) {
    set_pos(0, a);
    delay(3000);
    send_resp(mode);
  }
  set_pos(0, 0);
  laser_off();
  mode = 0;
  send_resp(mode);
}

void dscan1() {
  mode = 1;
  laser_on();
  // X: -40..40, Y: -40..40
  for (int p = -4000; p <= 4000; p += 1000) {
    set_pos(p, p);
    delay(3000);
    send_resp(mode);
  }
  set_pos(0, 0);
  laser_off();
  mode = 0;
  send_resp(mode);
}

void dscan2() {
  mode = 1;
  laser_on();
  // X: -40..40, Y: 40..-40
  int16_t x = -4000;
  int16_t y =  4000;
  while (x <= 4000) {
    set_pos(x, y);
    delay(3000);
    send_resp(mode);
    x += 1000;
    y -= 1000;
  }
  set_pos(0, 0);
  laser_off();
  mode = 0;
  send_resp(mode);
}

void full_cycle() {
  mode = 3;
  hscan();
  vscan();
  dscan1();
  dscan2();
  move_to(0, 0);
}