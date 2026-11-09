/*
  Smart Plant - Kode Tes Hardware (ESP32)
  Sensor : Capacitive Soil Moisture v2.0.0, DHT11, DS18B20
  Output : Relay 2 channel + pompa mini celup

  Pin:
    Soil moisture (AOUT) -> GPIO34
    DS18B20 (DATA)       -> GPIO33  (wajib pull-up 4.7k ke 3.3V)
    DHT11 (DATA)         -> GPIO4
    Relay IN1            -> GPIO13
    Relay IN2            -> GPIO12

  Library (Library Manager Arduino IDE):
    - DHT sensor library (Adafruit)  + Adafruit Unified Sensor
    - OneWire (Paul Stoffregen)
    - DallasTemperature (Miles Burton)

  Perintah via Serial Monitor (baud 115200, "Newline"):
    1 = toggle Relay 1
    2 = toggle Relay 2
    0 = matikan semua relay
    t = tes otomatis kedua relay (ON 2 detik masing-masing)
*/

#include <Arduino.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- Pin ----------
#define PIN_SOIL    34
#define PIN_DS18B20 33
#define PIN_DHT     4
#define PIN_RELAY1  26
#define PIN_RELAY2  27

// ---------- Konfigurasi relay ----------
// Kebanyakan modul relay 2 channel bersifat ACTIVE LOW (LOW = menyala).
// Kalau relay terbalik, ganti ke false.
#define RELAY_ACTIVE_LOW true

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

// ---------- Kalibrasi soil moisture ----------
// Isi setelah kalibrasi: baca nilai RAW saat sensor di udara kering (AIR)
// dan saat dicelup air sampai garis batas (WATER).
// Sensor kapasitif: makin basah -> nilai RAW makin KECIL.
int AIR_VALUE   = 3200;  // contoh, sesuaikan
int WATER_VALUE = 1400;  // contoh, sesuaikan

// ---------- Objek ----------
#define DHT_TYPE DHT11
DHT dht(PIN_DHT, DHT_TYPE);
OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);

bool relay1State = false;
bool relay2State = false;
unsigned long lastRead = 0;
const unsigned long READ_INTERVAL = 2000;  // DHT11 minimal ~1 detik

void setRelay1(bool on) {
  relay1State = on;
  digitalWrite(PIN_RELAY1, on ? RELAY_ON : RELAY_OFF);
  Serial.printf(">> Relay 1: %s\n", on ? "ON" : "OFF");
}

void setRelay2(bool on) {
  relay2State = on;
  digitalWrite(PIN_RELAY2, on ? RELAY_ON : RELAY_OFF);
  Serial.printf(">> Relay 2: %s\n", on ? "ON" : "OFF");
}

int readSoilRaw() {
  // rata-rata 10 sampel biar stabil
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PIN_SOIL);
    delay(5);
  }
  return sum / 10;
}

int soilPercent(int raw) {
  int pct = map(raw, AIR_VALUE, WATER_VALUE, 0, 100);
  return constrain(pct, 0, 100);
}

void testRelays() {
  Serial.println("\n=== TES RELAY ===");
  setRelay1(true);  delay(2000);
  setRelay1(false); delay(1000);
  setRelay2(true);  delay(2000);
  setRelay2(false);
  Serial.println("=== SELESAI ===\n");
}

void readSensors() {
  Serial.println("--------------------------------");

  // Soil moisture
  int raw = readSoilRaw();
  Serial.printf("Soil     : RAW=%d | Kelembapan=%d%%\n", raw, soilPercent(raw));

  // DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT11    : GAGAL BACA (cek kabel/pin D4)");
  } else {
    Serial.printf("DHT11    : Suhu=%.1f C | Kelembapan udara=%.1f%%\n", t, h);
  }

  // DS18B20
  ds18b20.requestTemperatures();
  float tc = ds18b20.getTempCByIndex(0);
  if (tc == DEVICE_DISCONNECTED_C) {
    Serial.println("DS18B20  : TIDAK TERDETEKSI (cek pull-up 4.7k & pin D33)");
  } else {
    Serial.printf("DS18B20  : Suhu=%.2f C\n", tc);
  }

  Serial.printf("Relay    : R1=%s | R2=%s\n",
                relay1State ? "ON" : "OFF", relay2State ? "ON" : "OFF");
}

// Diagnosa DS18B20: cek level pin saat idle dan ada tidaknya presence pulse
void diagDS18B20() {
  pinMode(PIN_DS18B20, INPUT_PULLUP);  // pull-up internal (~45k), sementara tanpa resistor
  delay(10);
  int idle = digitalRead(PIN_DS18B20);
  Serial.printf("DIAG DS18B20: level idle = %s\n",
                idle ? "HIGH (pull-up terpasang)"
                     : "LOW (DATA nyangkut ke GND / pull-up tidak ada)");

  bool present = oneWire.reset();
  Serial.printf("DIAG DS18B20: presence pulse = %s\n",
                present ? "ADA (sensor menjawab)" : "TIDAK ADA (sensor tidak menjawab)");

  if (present) {
    oneWire.reset_search();
    uint8_t addr[8];
    if (oneWire.search(addr)) {
      Serial.print("DIAG DS18B20: alamat = ");
      for (int i = 0; i < 8; i++) Serial.printf("%02X", addr[i]);
      Serial.println();
    }
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case '1': setRelay1(!relay1State); break;
      case '2': setRelay2(!relay2State); break;
      case '0': setRelay1(false); setRelay2(false); break;
      case 't': testRelays(); break;
      default: break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Set level OFF dulu sebelum pinMode agar relay tidak "klik" saat boot
  digitalWrite(PIN_RELAY1, RELAY_OFF);
  digitalWrite(PIN_RELAY2, RELAY_OFF);
  pinMode(PIN_RELAY1, OUTPUT);
  pinMode(PIN_RELAY2, OUTPUT);
  digitalWrite(PIN_RELAY1, RELAY_OFF);
  digitalWrite(PIN_RELAY2, RELAY_OFF);

  analogReadResolution(12);                       // 0 - 4095
  analogSetPinAttenuation(PIN_SOIL, ADC_11db);    // range ~0 - 3.3V

  dht.begin();
  pinMode(PIN_DS18B20, INPUT_PULLUP);  // pull-up internal untuk DS18B20
  ds18b20.begin();

  Serial.println("\n=== SMART PLANT - TES HARDWARE ===");
  diagDS18B20();
  Serial.printf("DS18B20 ditemukan: %d perangkat\n", ds18b20.getDeviceCount());
  Serial.println("Perintah: 1/2 = toggle relay, 0 = semua OFF, t = tes relay\n");
}

void loop() {
  handleSerial();

  if (millis() - lastRead >= READ_INTERVAL) {
    lastRead = millis();
    readSensors();
  }
}
