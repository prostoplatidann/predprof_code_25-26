#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>


const int PIN_X = 5;
const int PIN_Y = 6;
const int PIN_LASER = A0;

RF24 radio(9, 10);
const byte PIPE_RX[] = "targe1";
const int RF_CHAN = 100;

Servo sx, sy;

float lut_pan_x[]  = {-58, -50, -42, -32, -20,  -5,   7,  16,  24};
float lut_tilt_x[] = {  8,   8,   8,   8,   8,   9,  10,  10,  10};

float lut_tilt_y[] = {-33, -25, -16,  -5,   8,  20,  32,  42,  51};
float lut_pan_y[]  = {-17, -17, -17, -17, -20, -20, -20, -20, -20};

const int PHY_CENTER_PAN = 90;
const int PHY_CENTER_TILT = 90;

float currentLogicalX = 0;
float currentLogicalY = 0;
float offsetX = 0;
float offsetY = 0;

struct DataPackage {
  char mode;
  int16_t x;
  int16_t y;
  uint8_t laser;
};
DataPackage data;

void setup() {
  Serial.begin(115200);
  
  sx.attach(PIN_X);
  sy.attach(PIN_Y);
  pinMode(PIN_LASER, OUTPUT);
  digitalWrite(PIN_LASER, LOW);

  radio.begin();
  radio.setChannel(RF_CHAN);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(1, PIPE_RX);
  radio.startListening();

  Serial.println(F("TARGETER V6 (LUT Mode) READY"));
  moveTo(0, 0); 
}

void loop() {
  if (radio.available()) {
    radio.read(&data, sizeof(data));
    processCommand();
  }
}

void processCommand() {
  switch (data.mode) {
    case 'G': 
      setLaser(data.laser);
      moveTo(data.x, data.y);
      break;
    case 'H': 
      setLaser(0);
      moveTo(0, 0);
      break;
    case 'Z': 
      offsetX += currentLogicalX;
      offsetY += currentLogicalY;
      currentLogicalX = 0;
      currentLogicalY = 0;
      Serial.println(F("Zero Set"));
      break;
    case 'M': 
      runMacro(data.x);
      break;
  }
}

float getInterpolatedValue(float val, float* table) {
  val = constrain(val, -40, 40);
  
  float indexFloat = (val + 40.0) / 10.0;
  
  int i = (int)indexFloat;
  float frac = indexFloat - i;

  if (i >= 8) return table[8];

  return table[i] + (table[i+1] - table[i]) * frac;
}

void moveTo(float inputX, float inputY) {
  currentLogicalX = inputX;
  currentLogicalY = inputY;

  float x = inputX + offsetX;
  float y = inputY + offsetY;

  float pan_from_x = getInterpolatedValue(x, lut_pan_x);
  float tilt_from_y = getInterpolatedValue(y, lut_tilt_y);

  float pan_shift_from_y = getInterpolatedValue(y, lut_pan_y);
  float tilt_shift_from_x = getInterpolatedValue(x, lut_tilt_x);

  float pan_center = lut_pan_x[4];
  float tilt_center = lut_tilt_y[4];

  float pan_correction = pan_shift_from_y - pan_center;
  float tilt_correction = tilt_shift_from_x - tilt_center;

  float finalPanVal = pan_from_x + pan_correction;
  float finalTiltVal = tilt_from_y + tilt_correction;

  int servoPan = PHY_CENTER_PAN + (int)round(finalPanVal);
  int servoTilt = PHY_CENTER_TILT + (int)round(finalTiltVal);

  servoPan = constrain(servoPan, 0, 180);
  servoTilt = constrain(servoTilt, 0, 180);

  sx.write(servoPan);
  sy.write(servoTilt);
}

void setLaser(uint8_t state) {
  digitalWrite(PIN_LASER, state ? HIGH : LOW);
}
bool br = 0;
bool b1 = 0;
void runMacro(int m) {
  setLaser(1); 
  if (m == 5) { runMacro(1); if (br1 == 1){br = 1;} runMacro(2); if (br1 == 1){br = 1;} runMacro(3); if (br1 == 1){br = 1;} runMacro(4); if (br1 == 1){br1 = 0; br = 0; break;} }
  else if (m == 1) { for(int i=-40; i<=40; i+=10) { moveTo(i, 0); delay(400); if (br == 1){br = 0; break;}} }
  else if (m == 2) { for(int i=-40; i<=40; i+=10) { moveTo(0, i); delay(400); if (br == 1){br = 0; break;}} }
  else if (m == 3) { for(int i=-40; i<=40; i+=10) { moveTo(i, i); delay(400); if (br == 1){br = 0; break;}} }
  else if (m == 4) { for(int i=-40; i<=40; i+=10) { moveTo(i, -i); delay(400); if (br == 1){br = 0; break;}} }
  else if (m == 0) {br = 1; br1 = 1; break;}
  moveTo(0, 0); setLaser(0);
}