#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// --- Wi-Fi & MQTT Settings ---
const char* ssid        = "YOUR_WIFI_SSID";
const char* password    = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_HIVEMQ_CLUSTER_URL";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "YOUR_USERNAME";
const char* mqtt_pass   = "YOUR_PASSWORD";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// --- Pin Definitions ---
const int irSensorPin  = 13;
const int inductivePin = 32;  // FIX: was GPIO 12 (boot-strapping pin → caused boot loop)
const int voltagePin   = 34;  // ADC1 pin

const int motorENA = 14;
const int motorIN1 = 27;
const int motorIN2 = 26;

const int servoAPin = 25;
const int servoBPin = 33;

const int ledR = 4;
const int ledG = 16;
const int ledB = 17;

// --- LEDC channels for PWM (ESP32 native) ---
#define CH_LED_R   0
#define CH_LED_G   1
#define CH_LED_B   2
#define CH_MOTOR   3
#define PWM_FREQ   5000
#define PWM_RES    8    // 8-bit resolution: 0-255

// --- Objects ---
Servo servoA;
Servo servoB;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- State ---
bool isBeltRunning = false;

// ─────────────────────────────────────────────
//  WiFi + NTP
// ─────────────────────────────────────────────

void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    // Sync real UTC time so dashboard timestamps are correct
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Syncing NTP");
    struct tm ti;
    tries = 0;
    while (!getLocalTime(&ti) && tries < 20) { delay(500); Serial.print("."); tries++; }
    Serial.println(getLocalTime(&ti) ? "\nNTP synced." : "\nNTP failed (millis fallback).");
  } else {
    Serial.println("\nWiFi failed.");
  }
}

// Returns Unix epoch in milliseconds; falls back to millis() if NTP not ready
unsigned long long timestampMs() {
  struct tm ti;
  if (getLocalTime(&ti)) {
    time_t now; time(&now);
    return (unsigned long long)now * 1000ULL;
  }
  return (unsigned long long)millis();
}

// ─────────────────────────────────────────────
//  MQTT
// ─────────────────────────────────────────────

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == "ewaste/control/belt") {
    if (msg == "ON")  beltControl(true);
    if (msg == "OFF") beltControl(false);
  }
}

// FIX: non-blocking — returns after one attempt instead of looping forever
void reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    delay(3000);
    return;
  }
  Serial.print("MQTT reconnecting... ");
  if (client.connect("ESP32SorterClient", mqtt_user, mqtt_pass)) {
    Serial.println("connected.");
    client.subscribe("ewaste/control/belt");
  } else {
    Serial.println("failed (rc=" + String(client.state()) + "). Retry in 5s.");
    delay(5000);
  }
}

// ─────────────────────────────────────────────
//  Hardware helpers
// ─────────────────────────────────────────────

void setRGB(int r, int g, int b) {
  ledcWrite(CH_LED_R, r);
  ledcWrite(CH_LED_G, g);
  ledcWrite(CH_LED_B, b);
}

void beltControl(bool state) {
  isBeltRunning = state;
  if (state) {
    digitalWrite(motorIN1, HIGH);
    digitalWrite(motorIN2, LOW);
    ledcWrite(CH_MOTOR, 200);
  } else {
    digitalWrite(motorIN1, LOW);
    digitalWrite(motorIN2, LOW);
    ledcWrite(CH_MOTOR, 0);
  }
}

// FIX: average 10 ADC samples to reduce ESP32 ADC noise
float readBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < 10; i++) { sum += analogRead(voltagePin); delay(5); }
  float raw = sum / 10.0;
  return (raw / 4095.0) * 3.3 * 11.0; // Vout * divider ratio (10k/100k = 11x)
}

void testServos() {
  lcd.clear(); lcd.print("Testing Servos..");
  Serial.println("Servo self-test...");
  servoA.write(90); delay(800); servoA.write(0); delay(800);
  servoB.write(90); delay(800); servoB.write(0); delay(800);
  Serial.println("Servo self-test done.");
}

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Digital I/O
  pinMode(irSensorPin,  INPUT);
  pinMode(inductivePin, INPUT_PULLUP); // GPIO 32 has internal pullup, safe at boot
  pinMode(motorIN1,     OUTPUT);
  pinMode(motorIN2,     OUTPUT);

  // LEDC (PWM) — replaces analogWrite for reliable ESP32 behavior
  ledcSetup(CH_LED_R, PWM_FREQ, PWM_RES); ledcAttachPin(ledR,     CH_LED_R);
  ledcSetup(CH_LED_G, PWM_FREQ, PWM_RES); ledcAttachPin(ledG,     CH_LED_G);
  ledcSetup(CH_LED_B, PWM_FREQ, PWM_RES); ledcAttachPin(ledB,     CH_LED_B);
  ledcSetup(CH_MOTOR,  PWM_FREQ, PWM_RES); ledcAttachPin(motorENA, CH_MOTOR);

  // Servos
  servoA.attach(servoAPin); servoA.write(0);
  servoB.attach(servoBPin); servoB.write(0);

  // LCD
  lcd.init(); lcd.backlight();

  testServos();

  // Network
  espClient.setInsecure();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  client.setBufferSize(256);

  lcd.clear(); lcd.print("System Ready");
  Serial.println("Setup complete.");
}

// ─────────────────────────────────────────────
//  Main loop
// ─────────────────────────────────────────────

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (isBeltRunning && digitalRead(irSensorPin) == LOW) {
    beltControl(false);
    delay(500); // let pogo pins settle on battery terminals

    // 1. Read sensors
    float batteryVoltage = readBatteryVoltage();
    bool  isMagnetic     = (digitalRead(inductivePin) == LOW);

    // 2. Classify
    String batteryType = "Unknown";

    if (!isMagnetic && batteryVoltage >= 3.0 && batteryVoltage <= 4.2) {
      batteryType = "Li-ion";
      setRGB(0, 0, 255);
      servoA.write(90);

    } else if (isMagnetic && batteryVoltage >= 1.0 && batteryVoltage <= 1.5) {
      batteryType = "NiMH";    // FIX: was "NiCd_NiMH" — must match dashboard key exactly
      setRGB(255, 255, 0);
      servoB.write(90);

    } else if (!isMagnetic && batteryVoltage > 0.8 && batteryVoltage <= 1.6) {
      batteryType = "Alkaline";
      setRGB(0, 255, 0);
      // No servo — passes through to default bin

    } else {
      setRGB(255, 0, 0); // Unknown
    }

    // 3. Serial debug
    Serial.println("--- BATTERY DETECTED ---");
    Serial.print("Voltage : "); Serial.print(batteryVoltage, 2); Serial.println(" V");
    Serial.print("Magnetic: "); Serial.println(isMagnetic ? "YES" : "NO");
    Serial.print("Type    : "); Serial.println(batteryType);
    Serial.println("------------------------");

    // 4. LCD
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(batteryType);
    lcd.setCursor(0, 1);
    lcd.print(batteryVoltage, 2); lcd.print("V M:"); lcd.print(isMagnetic ? "Y" : "N");

    // 5. Publish MQTT
    // FIX: key is "timestamp" (was "ts") + real Unix ms from NTP (was millis()/1000)
    unsigned long long ts = timestampMs();
    char payload[160];
    snprintf(payload, sizeof(payload),
      "{\"type\":\"%s\",\"voltage\":%.2f,\"magnetic\":%s,\"timestamp\":%llu}",
      batteryType.c_str(), batteryVoltage, isMagnetic ? "true" : "false", ts);
    client.publish("ewaste/sort/events", payload);

    // 6. Reset and resume
    delay(2000);
    servoA.write(0); servoB.write(0);
    setRGB(0, 0, 0);
    lcd.clear(); lcd.print("Scanning...");
    beltControl(true);
  }
}
