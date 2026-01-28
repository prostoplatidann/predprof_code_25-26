// COMMANDER FOR ARDUINO NANO - DEBUG ВЕРСИЯ

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

const byte PIPE_CMD[] = "targe1";
const byte PIPE_RX[]  = "main1";
const int  RF_CHAN    = 100;

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

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║ COMMANDER DEBUG - INITIALIZING         ║");
  Serial.println("╚════════════════════════════════════════╝");

  Serial.print("[INIT] RF24 begin...");
  if (!radio.begin()) {
    Serial.println(" FAILED!");
    Serial.println("[ERROR] NRF24 did not initialize");
    while (1) {
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

  Serial.print("[INIT] Opening TX pipe: ");
  Serial.println((const char*)PIPE_CMD);
  radio.openWritingPipe(PIPE_CMD);

  Serial.print("[INIT] Opening RX pipe: ");
  Serial.println((const char*)PIPE_RX);
  radio.openReadingPipe(1, PIPE_RX);

  Serial.print("[INIT] Starting listening...");
  radio.startListening();
  Serial.println(" OK");

  Serial.println();
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║ COMMANDER READY - WAITING INPUT        ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  Serial.println("КОМАНДЫ:");
  Serial.println("  h  - горизонтальное сканирование");
  Serial.println("  v  - вертикальное сканирование");
  Serial.println("  d1 - диагональное сканирование 1 (-40,-40 → 40,40)");
  Serial.println("  d2 - диагональное сканирование 2 (-40,40 → 40,-40)");
  Serial.println("  c  - центр (0,0)");
  Serial.println("  s  - стоп");
  Serial.println("  m X Y - перемещение в точку (градусы)");
  Serial.println();
}

void loop() {
  if (radio.available()) {
    radio.read(&resp, sizeof(resp));
    Serial.print("[RX RESPONSE] x=");
    Serial.print(resp.x);
    Serial.print(" y=");
    Serial.print(resp.y);
    Serial.print(" status=");
    Serial.print(resp.status);
    Serial.print(" done='");
    Serial.print(resp.done);
    Serial.println("'");
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.length() == 0) {
      Serial.println("[SKIP] Empty command");
      return;
    }

    Serial.println();
    Serial.println("[SEND] Sending command: " + cmd);

    if (cmd == "h") {
      send(1, 0, 0);
      Serial.println("[SEND] Command 1 (h) sent");
    }
    else if (cmd == "v") {
      send(2, 0, 0);
      Serial.println("[SEND] Command 2 (v) sent");
    }
    else if (cmd == "d1") {
      send(3, 0, 0);
      Serial.println("[SEND] Command 3 (d1) sent");
    }
    else if (cmd == "d2") {
      send(4, 0, 0);
      Serial.println("[SEND] Command 4 (d2) sent");
    }
    else if (cmd == "c") {
      send(6, 0, 0);
      Serial.println("[SEND] Command 6 (c) sent");
    }
    else if (cmd == "s") {
      send(7, 0, 0);
      Serial.println("[SEND] Command 7 (s) sent");
    }
    else if (cmd.startsWith("m ")) {     
      Serial.println(cmd);
      parse_move(cmd);
    }
    else {
      Serial.println("[ERROR] Unknown command: " + cmd);
      Serial.println("Available: h v d1 d2 c s m X Y");
    }

    Serial.println();
    delay(50);
  }
}

void parse_move(String cmd) {
  cmd.remove(0, 1);
  cmd.trim();

  int spaceIndex = cmd.indexOf(' ');
  if (spaceIndex == -1) {
    Serial.println("G format: G x y   (deg, -40..40)");
    return;
  }

  String xStr = cmd.substring(0, spaceIndex);
  String yStr = cmd.substring(spaceIndex + 1);
  xStr.trim();
  yStr.trim();

  if (!xStr.length() || !yStr.length()) {
    Serial.println("G format: G x y   (deg, -40..40)");
    return;
  }

  int xDeg = xStr.toInt();
  int yDeg = yStr.toInt();

  if (xDeg < -40) xDeg = -40;
  if (xDeg >  40) xDeg =  40;
  if (yDeg < -40) yDeg = -40;
  if (yDeg >  40) yDeg =  40;

  int xHundreds = xDeg * 100;
  int yHundreds = yDeg * 100;

  Serial.print("GOTO: X=");
  Serial.print(xDeg);
  Serial.print(" deg, Y=");
  Serial.print(yDeg);
  Serial.println(" deg");

  send(5, xHundreds, yHundreds);
}

/*
void parse_move(String s) {
  Serial.println(s);
  s = s.substring(2);
  s.trim();
  Serial.println(s);

  int16_t x = 0, y = 0;
  int sp = s.indexOf(' ');
  Serial.println(sp);
  if (sp != -1) {
    x = s.substring(0, sp).toInt() * 100;
    y = s.substring(sp + 1).toInt() * 100;
  } else {
    Serial.println("[ERROR] Invalid format. Use: m X Y (e.g. m 20 -15)");
    return;
  }
  Serial.println(x);
  Serial.println(y);

  x = constrain(x, -4000, 4000);
  y = constrain(y, -4000, 4000);

  Serial.print("[SEND] Move command: X=");
  Serial.print(x / 100.0, 1);
  Serial.print("° Y=");
  Serial.print(y / 100.0, 1);
  Serial.println("°");

  send(5, x, y);
  Serial.println("[SEND] Command 5 (m) sent");
}
*/

void send(uint8_t cmd, int16_t x, int16_t y) {
  pkt.x = x;
  pkt.y = y;
  pkt.cmd = cmd;
  strcpy(pkt.ok, "OK");

  Serial.print("[PACKET] Building: cmd=");
  Serial.print(cmd);
  Serial.print(" x=");
  Serial.print(x);
  Serial.print(" y=");
  Serial.print(y);
  Serial.print(" ok='");
  Serial.print(pkt.ok);
  Serial.println("'");

  radio.stopListening();
  Serial.print("[TX] Sending...");
  bool ok = radio.write(&pkt, sizeof(pkt));
  if (ok) {
    Serial.println(" SUCCESS");
  } else {
    Serial.println(" FAILED");
  }

  radio.startListening();
  Serial.println("[TX] Back to listening mode");
}
