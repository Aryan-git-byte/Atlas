/*
 * ATLAS Development Board Firmware
 * ESP32-S3 with A9G GSM/GPS, RS485, SD Card
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <HardwareSerial.h>

#define BOOT_BTN 0
#define STATUS_LED 2

#define SD_CS 14
#define SD_MOSI 13
#define SD_MISO 12  
#define SD_CLK 11

#define A9G_RX 18
#define A9G_TX 17
#define A9G_PWR 16
#define A9G_RST 15

#define RS485_RX 36
#define RS485_TX 37
#define RS485_DE 38
#define RS485_RE 35

#define I2C_SDA 21
#define I2C_SCL 47

#define MIC_IN 1
#define SPK_OUT 2

#define IO_4 4
#define IO_5 5
#define IO_6 6
#define IO_7 7
#define IO_8 8
#define IO_9 9
#define IO_10 10

#define SERIAL_BAUD 115200
#define A9G_BAUD 115200
#define RS485_BAUD 9600

#define A9G_CMD_TIMEOUT 8000
#define A9G_BOOT_TIME 12000
#define RS485_RESPONSE_TIME 500

#define CMD_BUF_SIZE 256
#define A9G_BUF_SIZE 512
#define RS485_BUF_SIZE 128

HardwareSerial a9gSerial(1);
HardwareSerial rs485Serial(2);

struct {
  bool sd_ok;
  bool a9g_online;
  bool rs485_init;
  bool gps_lock;
  bool gsm_reg;
  uint32_t boot_time;
  float vbat;
  int rssi;
  unsigned long last_gps_update;
} sys;

char cmd_buffer[CMD_BUF_SIZE];
int cmd_idx = 0;
unsigned long last_blink = 0;
bool led_state = false;

#define VBAT_ADC_PIN 1
#define VDIV_RATIO 3.13
#define ADC_VREF 3.3
#define ADC_RES 4095.0

void log_msg(const char* msg) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] ");
  Serial.println(msg);
}

void log_msg(String msg) {
  log_msg(msg.c_str());
}

void blink_fast(int count) {
  for(int i = 0; i < count; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(80);
    digitalWrite(STATUS_LED, LOW);
    delay(80);
  }
}

bool init_sdcard() {
  log_msg("SD: Starting init...");
  
  SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  
  if(!SD.begin(SD_CS)) {
    log_msg("SD: Mount failed");
    sys.sd_ok = false;
    return false;
  }
  
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    log_msg("SD: No card detected");
    sys.sd_ok = false;
    return false;
  }
  
  String type_str = (cardType == CARD_MMC) ? "MMC" : 
                    (cardType == CARD_SD) ? "SD" : 
                    (cardType == CARD_SDHC) ? "SDHC" : "???";
  log_msg("SD: Type=" + type_str);
  
  uint64_t size_mb = SD.cardSize() / (1024 * 1024);
  log_msg("SD: Size=" + String((uint32_t)size_mb) + "MB");
  
  sys.sd_ok = true;
  return true;
}

bool sd_write(const char* path, const char* data) {
  if(!sys.sd_ok) return false;
  
  File f = SD.open(path, FILE_WRITE);
  if(!f) {
    log_msg("SD: Open failed - " + String(path));
    return false;
  }
  
  size_t written = f.print(data);
  f.close();
  
  return (written > 0);
}

bool sd_append(const char* path, const char* data) {
  if(!sys.sd_ok) return false;
  
  File f = SD.open(path, FILE_APPEND);
  if(!f) return false;
  
  size_t written = f.print(data);
  f.close();
  return (written > 0);
}

String sd_read(const char* path) {
  if(!sys.sd_ok) return "";
  
  File f = SD.open(path);
  if(!f) return "";
  
  String content;
  content.reserve(f.size());
  
  while(f.available()) {
    content += (char)f.read();
  }
  f.close();
  
  return content;
}

void sd_list_dir(const char* path) {
  if(!sys.sd_ok) return;
  
  File root = SD.open(path);
  if(!root || !root.isDirectory()) {
    log_msg("SD: Invalid dir");
    return;
  }
  
  File entry = root.openNextFile();
  while(entry) {
    if(entry.isDirectory()) {
      Serial.print("  [DIR]  ");
    } else {
      Serial.print("  ");
      Serial.print(entry.size());
      Serial.print("\t");
    }
    Serial.println(entry.name());
    entry = root.openNextFile();
  }
}

String a9g_send_at(const char* cmd, uint32_t timeout = A9G_CMD_TIMEOUT) {
  a9gSerial.println(cmd);
  
  if(Serial) {
    Serial.print("A9G >> ");
    Serial.println(cmd);
  }
  
  String resp;
  resp.reserve(256);
  unsigned long start = millis();
  
  while((millis() - start) < timeout) {
    while(a9gSerial.available()) {
      char c = a9gSerial.read();
      resp += c;
      
      if(resp.endsWith("OK\r\n") || 
         resp.endsWith("ERROR\r\n") ||
         resp.endsWith("> ")) {
        goto done;
      }
    }
    delay(5);
  }
  
done:
  if(resp.length() > 0 && Serial) {
    Serial.print("A9G << ");
    Serial.println(resp);
  }
  
  return resp;
}

void a9g_power_toggle() {
  log_msg("A9G: Power cycle");
  digitalWrite(A9G_PWR, HIGH);
  delay(1200);
  digitalWrite(A9G_PWR, LOW);
  delay(A9G_BOOT_TIME);
}

bool init_a9g() {
  log_msg("A9G: Initializing...");
  
  pinMode(A9G_PWR, OUTPUT);
  digitalWrite(A9G_PWR, LOW);
  
  a9gSerial.begin(A9G_BAUD, SERIAL_8N1, A9G_RX, A9G_TX);
  delay(500);
  
  String resp = a9g_send_at("AT", 2000);
  if(resp.indexOf("OK") < 0) {
    log_msg("A9G: Not responding, power cycle");
    a9g_power_toggle();
    resp = a9g_send_at("AT", 3000);
  }
  
  if(resp.indexOf("OK") < 0) {
    log_msg("A9G: Init failed!");
    sys.a9g_online = false;
    return false;
  }
  
  a9g_send_at("ATE0");
  a9g_send_at("AT+CMGF=1");
  
  resp = a9g_send_at("AT+CREG?");
  if(resp.indexOf(",1") > 0 || resp.indexOf(",5") > 0) {
    sys.gsm_reg = true;
    log_msg("A9G: Network registered");
  } else {
    sys.gsm_reg = false;
    log_msg("A9G: Not registered (will retry later)");
  }
  
  sys.a9g_online = true;
  return true;
}

int a9g_get_rssi() {
  String resp = a9g_send_at("AT+CSQ", 3000);
  int idx = resp.indexOf("+CSQ: ");
  if(idx >= 0) {
    int comma = resp.indexOf(',', idx);
    String rssi_str = resp.substring(idx + 6, comma);
    return rssi_str.toInt();
  }
  return -1;
}

bool a9g_send_sms(const char* num, const char* msg) {
  if(!sys.a9g_online || !sys.gsm_reg) return false;
  
  String cmd = "AT+CMGS=\"";
  cmd += num;
  cmd += "\"";
  
  a9gSerial.println(cmd);
  delay(300);
  
  a9gSerial.print(msg);
  delay(100);
  a9gSerial.write(0x1A);
  
  delay(5000);
  return true;
}

void a9g_enable_gps() {
  a9g_send_at("AT+GPS=1");
  log_msg("GPS: Enabled");
}

void a9g_disable_gps() {
  a9g_send_at("AT+GPS=0");
  log_msg("GPS: Disabled");
}

String a9g_get_gps() {
  return a9g_send_at("AT+GPSRD=1");
}

void a9g_process_bg() {
  while(a9gSerial.available()) {
    String line;
    char c;
    while(a9gSerial.available() && (c = a9gSerial.read()) != '\n') {
      line += c;
    }
    
    if(line.length() > 0) {
      if(line.indexOf("+CMT:") >= 0) {
        log_msg("SMS received!");
      }
      
      if(Serial) Serial.println(line);
    }
  }
}

void rs485_mode(bool tx_enable) {
  digitalWrite(RS485_DE, tx_enable ? HIGH : LOW);
  digitalWrite(RS485_RE, tx_enable ? HIGH : LOW);
  
  if(tx_enable) {
    delayMicroseconds(50);
  }
}

bool init_rs485() {
  log_msg("RS485: Init");
  
  pinMode(RS485_DE, OUTPUT);
  pinMode(RS485_RE, OUTPUT);
  rs485_mode(false);
  
  rs485Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);
  
  sys.rs485_init = true;
  return true;
}

void rs485_send(const char* data) {
  rs485_mode(true);
  rs485Serial.print(data);
  rs485Serial.flush();
  delayMicroseconds(100);
  rs485_mode(false);
}

void rs485_send_bytes(uint8_t* buf, size_t len) {
  rs485_mode(true);
  rs485Serial.write(buf, len);
  rs485Serial.flush();
  delayMicroseconds(100);
  rs485_mode(false);
}

String rs485_recv(uint32_t timeout = RS485_RESPONSE_TIME) {
  String data;
  unsigned long start = millis();
  
  while((millis() - start) < timeout) {
    if(rs485Serial.available()) {
      data += (char)rs485Serial.read();
    }
  }
  
  return data;
}

void rs485_process_bg() {
  if(rs485Serial.available()) {
    String msg;
    while(rs485Serial.available()) {
      msg += (char)rs485Serial.read();
      delay(2);
    }
    
    if(msg.length() > 0) {
      log_msg("RS485 RX: " + msg);
    }
  }
}

float read_battery() {
  long sum = 0;
  for(int i = 0; i < 5; i++) {
    sum += analogRead(VBAT_ADC_PIN);
    delay(5);
  }
  
  float adc_avg = sum / 5.0;
  float v = (adc_avg / ADC_RES) * ADC_VREF * VDIV_RATIO;
  
  return v;
}

void enter_light_sleep(uint32_t sec) {
  log_msg("Entering light sleep for " + String(sec) + "s");
  esp_sleep_enable_timer_wakeup(sec * 1000000ULL);
  esp_light_sleep_start();
  log_msg("Woke from sleep");
}

void print_help() {
  Serial.println("\n--- ATLAS Board Commands ---");
  Serial.println("help         - this menu");
  Serial.println("status       - system info");
  Serial.println("reboot       - restart");
  Serial.println("\nSD Card:");
  Serial.println("sd.ls        - list files");
  Serial.println("sd.cat <f>   - read file");
  Serial.println("sd.rm <f>    - delete file");
  Serial.println("\nA9G:");
  Serial.println("gsm.at <cmd> - send AT command");
  Serial.println("gsm.rssi     - signal strength");
  Serial.println("gsm.sms <num> <msg>");
  Serial.println("gps.on / gps.off");
  Serial.println("gps.loc      - get location");
  Serial.println("\nRS485:");
  Serial.println("485.tx <msg>");
  Serial.println("\nGPIO:");
  Serial.println("pin.set <n> <0|1>");
  Serial.println("pin.get <n>");
  Serial.println("\nPower:");
  Serial.println("pwr.sleep <sec>");
  Serial.println("pwr.vbat\n");
}

void print_status() {
  Serial.println("\n=== System Status ===");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println("s");
  
  Serial.print("Free RAM: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  
  Serial.print("SD Card: ");
  Serial.println(sys.sd_ok ? "OK" : "FAIL");
  
  Serial.print("A9G: ");
  Serial.println(sys.a9g_online ? "Online" : "Offline");
  
  Serial.print("GSM: ");
  Serial.println(sys.gsm_reg ? "Registered" : "Not registered");
  
  Serial.print("RS485: ");
  Serial.println(sys.rs485_init ? "Ready" : "Not init");
  
  Serial.print("Battery: ");
  Serial.print(sys.vbat, 2);
  Serial.println("V");
  
  if(sys.a9g_online) {
    Serial.print("RSSI: ");
    Serial.println(sys.rssi);
  }
  
  Serial.println();
}

void handle_command(String cmd) {
  cmd.trim();
  
  if(cmd.length() == 0) return;
  
  String cmd_lower = cmd;
  cmd_lower.toLowerCase();
  
  if(cmd_lower == "help" || cmd_lower == "?") {
    print_help();
  }
  else if(cmd_lower == "status") {
    print_status();
  }
  else if(cmd_lower == "reboot") {
    Serial.println("Rebooting in 2s...");
    delay(2000);
    ESP.restart();
  }
  else if(cmd_lower == "sd.ls") {
    sd_list_dir("/");
  }
  else if(cmd_lower.startsWith("sd.cat ")) {
    String fname = cmd.substring(7);
    String content = sd_read(fname.c_str());
    Serial.println(content);
  }
  else if(cmd_lower.startsWith("sd.rm ")) {
    String fname = cmd.substring(6);
    if(SD.remove(fname.c_str())) {
      Serial.println("Deleted: " + fname);
    } else {
      Serial.println("Delete failed");
    }
  }
  else if(cmd_lower.startsWith("gsm.at ")) {
    String at = cmd.substring(7);
    a9g_send_at(at.c_str());
  }
  else if(cmd_lower == "gsm.rssi") {
    int r = a9g_get_rssi();
    Serial.print("RSSI: ");
    Serial.println(r);
  }
  else if(cmd_lower.startsWith("gsm.sms ")) {
    int sp = cmd.indexOf(' ', 8);
    if(sp > 0) {
      String num = cmd.substring(8, sp);
      String msg = cmd.substring(sp + 1);
      a9g_send_sms(num.c_str(), msg.c_str());
      Serial.println("SMS queued");
    }
  }
  else if(cmd_lower == "gps.on") {
    a9g_enable_gps();
  }
  else if(cmd_lower == "gps.off") {
    a9g_disable_gps();
  }
  else if(cmd_lower == "gps.loc") {
    String loc = a9g_get_gps();
    Serial.println(loc);
  }
  else if(cmd_lower.startsWith("485.tx ")) {
    String msg = cmd.substring(7);
    rs485_send(msg.c_str());
    Serial.println("TX: " + msg);
  }
  else if(cmd_lower.startsWith("pin.set ")) {
    int sp = cmd.indexOf(' ', 8);
    if(sp > 0) {
      int pin = cmd.substring(8, sp).toInt();
      int val = cmd.substring(sp + 1).toInt();
      pinMode(pin, OUTPUT);
      digitalWrite(pin, val);
      Serial.println("Pin " + String(pin) + " = " + String(val));
    }
  }
  else if(cmd_lower.startsWith("pin.get ")) {
    int pin = cmd.substring(8).toInt();
    pinMode(pin, INPUT);
    int val = digitalRead(pin);
    Serial.println("Pin " + String(pin) + " = " + String(val));
  }
  else if(cmd_lower.startsWith("pwr.sleep ")) {
    int sec = cmd.substring(10).toInt();
    enter_light_sleep(sec);
  }
  else if(cmd_lower == "pwr.vbat") {
    float v = read_battery();
    Serial.print("Battery: ");
    Serial.print(v, 3);
    Serial.println("V");
  }
  else {
    Serial.println("Unknown command (try 'help')");
  }
}

void process_serial_input() {
  while(Serial.available()) {
    char c = Serial.read();
    
    if(c == '\n' || c == '\r') {
      if(cmd_idx > 0) {
        cmd_buffer[cmd_idx] = 0;
        handle_command(String(cmd_buffer));
        cmd_idx = 0;
      }
    }
    else if(cmd_idx < CMD_BUF_SIZE - 1) {
      cmd_buffer[cmd_idx++] = c;
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  
  Serial.println("\n\n=================================");
  Serial.println("ATLAS Dev Board v2.1");
  Serial.println("Booting...");
  Serial.println("=================================\n");
  
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  
  digitalWrite(STATUS_LED, HIGH);
  delay(300);
  digitalWrite(STATUS_LED, LOW);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  log_msg("I2C ready");
  
  init_sdcard();
  delay(200);
  
  init_a9g();
  delay(200);
  
  init_rs485();
  delay(200);
  
  sys.vbat = read_battery();
  sys.boot_time = millis();
  
  Serial.println("\n=================================");
  Serial.println("System Ready");
  Serial.println("Type 'help' for commands");
  Serial.println("=================================\n");
  
  blink_fast(2);
  
  if(sys.sd_ok) {
    String log = "\n--- Boot @ ";
    log += millis();
    log += "ms ---\n";
    log += "Heap: ";
    log += ESP.getFreeHeap();
    log += "\nVbat: ";
    log += sys.vbat;
    log += "V\n";
    sd_append("/boot.log", log.c_str());
  }
}

void loop() {
  if(millis() - last_blink > 1000) {
    last_blink = millis();
    led_state = !led_state;
    digitalWrite(STATUS_LED, led_state);
    
    sys.vbat = read_battery();
    
    if(sys.a9g_online) {
      sys.rssi = a9g_get_rssi();
    }
  }
  
  process_serial_input();
  a9g_process_bg();
  rs485_process_bg();
  
  static uint32_t btn_press_start = 0;
  static bool btn_was_pressed = false;
  
  if(digitalRead(BOOT_BTN) == LOW) {
    if(!btn_was_pressed) {
      btn_press_start = millis();
      btn_was_pressed = true;
    }
    else if((millis() - btn_press_start) > 5000) {
      Serial.println("\n*** Long press detected ***");
      blink_fast(5);
      btn_was_pressed = false;
    }
  }
  else {
    if(btn_was_pressed && (millis() - btn_press_start) < 5000) {
      Serial.println("Button pressed");
      blink_fast(1);
    }
    btn_was_pressed = false;
  }
  
  delay(10);
}
