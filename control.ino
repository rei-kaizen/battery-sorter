#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// --- Wi-Fi & MQTT Settings ---
const char* ssid        = "YOUR_WIFI_SSID";
const char* password    = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_SERVER_URL";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "YOUR_MQTT_USERNAME";
const char* mqtt_pass   = "YOUR_MQTT_PASSWORD";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// --- Pin Definitions (NodeMCU ESP8266) ---
const int irSensorPin  = 13;  // D7 — IR obstacle sensor
const int inductivePin = 12;  // D6 — inductive proximity sensor (NPN)
const int voltagePin   = A0;  // A0 — battery OCV via voltage divider

const int motorENA = 14;  // D5 — PWM speed control
const int motorIN1 = 16;  // D0 — motor direction
const int motorIN2 = 0;   // D3 — motor direction

const int servoAPin = 15; // D8 — Alkaline divert gate
const int servoBPin = 2;  // D4 — NiMH divert gate

// LCD uses default I2C: SDA=D2(GPIO4), SCL=D1(GPIO5)

// --- Servo angle constants ---
const int SERVO_REST   = 0;   // neutral — gate not blocking belt
const int SERVO_DIVERT = 80;  // divert — gate redirects battery into bin

// --- Servo timing (milliseconds) ---
const int SERVO_A_DELAY = 5000;   // travel time from IR sensor to Alkaline gate
const int SERVO_B_DELAY = 10000;  // travel time from IR sensor to NiMH gate
const int SERVO_HOLD    = 3000;   // hold gate open to ensure battery falls into bin

// --- Objects ---
Servo servoA;
Servo servoB;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- State ---
bool isBeltRunning = false;

// ─────────────────────────────────────────────
//  Sensor reading
// ─────────────────────────────────────────────

// Averages 10 ADC samples to reduce noise
// Voltage divider: R1=100kΩ, R2=10kΩ → 11x multiplier
// ESP8266 ADC: 10-bit (0–1023), 3.3V reference
float readBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(voltagePin);
    delay(5);
  }
  float raw = sum / 10.0;
  return (raw / 1023.0) * 3.3 * 11.0;
}

// ─────────────────────────────────────────────
//  Battery classification
//  Based on open-circuit voltage and metal casing detection
//
//  NiMH     → metal casing (magnetic=true)  + 1.0–1.3V
//  Alkaline → plastic casing (magnetic=false) + 1.2–1.6V
//  Li-ion   → plastic casing (magnetic=false) + 3.0–4.2V
// ─────────────────────────────────────────────

String classifyBattery(float voltage, bool isMagnetic) {
  if (isMagnetic && voltage >= 1.0 && voltage <= 1.3) {
    return "NiMH";
  } else if (!isMagnetic && voltage >= 1.2 && voltage <= 1.6) {
    return "Alkaline";
  } else if (!isMagnetic && voltage >= 3.0 && voltage <= 4.2) {
    return "Li-ion";
  }
  return "Unknown";
}

// ─────────────────────────────────────────────
//  WiFi + NTP
// ─────────────────────────────────────────────

void setup_wifi() {
  Serial.print("Connecting to WiFi");
  lcd.clear(); lcd.print("Connecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  delay(1000);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 60) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Syncing NTP");
    tries = 0;
    while (time(nullptr) < 1000000000UL && tries < 20) {
      delay(500); Serial.print("."); tries++;
    }
    Serial.println(time(nullptr) > 1000000000UL ? "\nNTP synced." : "\nNTP failed.");
  } else {
    Serial.println("\nWiFi failed.");
  }
}

void getTimestampStr(char* buf, size_t len) {
  unsigned long ts = (unsigned long)time(nullptr);
  if (ts > 1000000000UL) {
    snprintf(buf, len, "%lu000", ts);
  } else {
    snprintf(buf, len, "0");
  }
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

void reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    delay(3000);
    return;
  }
  Serial.print("MQTT reconnecting... ");
  if (client.connect("ESP8266SorterClient", mqtt_user, mqtt_pass)) {
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

void beltControl(bool state) {
  isBeltRunning = state;
  if (state) {
    digitalWrite(motorIN1, HIGH);
    digitalWrite(motorIN2, LOW);
    analogWrite(motorENA, 200);
  } else {
    digitalWrite(motorIN1, LOW);
    digitalWrite(motorIN2, LOW);
    analogWrite(motorENA, 0);
  }
}

void testServos() {
  lcd.clear(); lcd.print("Testing Servos..");
  Serial.println("Servo self-test...");
  servoA.write(SERVO_DIVERT); delay(800);
  servoA.write(SERVO_REST);   delay(800);
  servoB.write(SERVO_DIVERT); delay(800);
  servoB.write(SERVO_REST);   delay(800);
  Serial.println("Servo self-test done.");
}

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────

void setup() {
  Serial.begin(74880);

  pinMode(irSensorPin,  INPUT);
  pinMode(inductivePin, INPUT_PULLUP);
  pinMode(motorIN1,     OUTPUT);
  pinMode(motorIN2,     OUTPUT);
  pinMode(motorENA,     OUTPUT);

  analogWrite(motorENA, 0);

  servoA.attach(servoAPin); servoA.write(SERVO_REST);
  servoB.attach(servoBPin); servoB.write(SERVO_REST);

  lcd.init(); lcd.backlight();
  testServos();

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
    delay(300); // brief settle before sensor reads

    // 1. Read both sensors
    float  batteryVoltage = readBatteryVoltage();
    bool   isMagnetic     = (digitalRead(inductivePin) == LOW);
    String batteryType    = classifyBattery(batteryVoltage, isMagnetic);

    // 2. Serial debug
    Serial.println("--- BATTERY DETECTED ---");
    Serial.print("Voltage : "); Serial.print(batteryVoltage, 2); Serial.println(" V");
    Serial.print("Magnetic: "); Serial.println(isMagnetic ? "YES" : "NO");
    Serial.print("Type    : "); Serial.println(batteryType);
    Serial.println("------------------------");

    // 3. LCD — show classification immediately
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(batteryType);
    lcd.setCursor(0, 1);
    lcd.print(batteryVoltage, 2);
    lcd.print("V M:");
    lcd.print(isMagnetic ? "Y" : "N");

    // 4. Publish MQTT
    char tsStr[14];
    getTimestampStr(tsStr, sizeof(tsStr));
    char payload[160];
    snprintf(payload, sizeof(payload),
      "{\"type\":\"%s\",\"voltage\":%.2f,\"magnetic\":%s,\"timestamp\":%s}",
      batteryType.c_str(), batteryVoltage,
      isMagnetic ? "true" : "false", tsStr);
    client.publish("ewaste/sort/events", payload);

    // 5. Servo timing — gate fires after belt travel delay
    if (batteryType == "Alkaline") {
      delay(SERVO_A_DELAY);
      servoA.write(SERVO_DIVERT);
      Serial.println("Servo A → 80° (Alkaline bin)");
      delay(SERVO_HOLD);
      servoA.write(SERVO_REST);
      Serial.println("Servo A → 0° (reset)");

    } else if (batteryType == "NiMH") {
      delay(SERVO_B_DELAY);
      servoB.write(SERVO_DIVERT);
      Serial.println("Servo B → 80° (NiMH bin)");
      delay(SERVO_HOLD);
      servoB.write(SERVO_REST);
      Serial.println("Servo B → 0° (reset)");

    } else {
      // Li-ion and Unknown pass through to end bin
      Serial.println("No servo — battery passes through to end bin");
      delay(500);
    }

    // 6. Resume belt
    lcd.clear(); lcd.print("Scanning...");
    beltControl(true);
  }
}