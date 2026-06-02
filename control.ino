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
const int irSensorPin  = 13;  // D7
const int inductivePin = 12;  // D6

const int motorENA = 14;  // D5 - PWM
const int motorIN1 = 16;  // D0
const int motorIN2 = 0;   // D3

const int servoAPin = 15; // D8 — Alkaline gate
const int servoBPin = 2;  // D4 — NiMH gate

// LCD uses default I2C: SDA=D2(GPIO4), SCL=D1(GPIO5)

// --- Servo angle constants ---
const int SERVO_REST   = 0;   // start position — gate fully open/neutral
const int SERVO_DIVERT = 80;  // divert position — gate redirects battery

// --- Servo timing (milliseconds) ---
const int SERVO_A_DELAY = 5000;   // wait 5s after IR trigger before Servo A moves
const int SERVO_B_DELAY = 10000;  // wait 10s after IR trigger before Servo B moves
const int SERVO_HOLD    = 3000;   // hold at divert position for 3s to capture battery

// --- Objects ---
Servo servoA;
Servo servoB;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- State ---
bool isBeltRunning  = false;
int  detectionCount = 0;  // cycles: 0=Alkaline, 1=NiMH, 2=Li-ion

// ─────────────────────────────────────────────
//  Battery classifier
//  Cycles: Alkaline → NiMH → Li-ion → repeat
// ─────────────────────────────────────────────

struct BatteryReading {
  String type;
  float  voltage;
  bool   isMagnetic;
};

// Simulated voltage for demo/testing: random 0.00V–1.50V
// To enable real sensing: replace with (analogRead(A0) / 1023.0) * 3.3 * 11.0
float simulatedVoltage() {
  return random(0, 1501) / 1000.0;
}

BatteryReading classifyNext() {
  int slot = detectionCount % 3;
  detectionCount++;
  if (slot == 0) return { "Alkaline", simulatedVoltage(), false };
  if (slot == 1) return { "NiMH",     simulatedVoltage(), true  };
  else           return { "Li-ion",   simulatedVoltage(), false };
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
    snprintf(buf, len, "0");  // NTP not synced; dashboard substitutes receive time
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
  randomSeed(analogRead(A0));

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

    // 1. Classify battery
    BatteryReading battery = classifyNext();

    // 2. Serial debug
    Serial.println("--- BATTERY DETECTED ---");
    Serial.print("Type    : "); Serial.println(battery.type);
    Serial.print("Voltage : "); Serial.print(battery.voltage, 2); Serial.println(" V (simulated)");
    Serial.print("Magnetic: "); Serial.println(battery.isMagnetic ? "YES" : "NO");
    Serial.println("------------------------");

    // 3. LCD — show type immediately on detection
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(battery.type);
    lcd.setCursor(0, 1);
    lcd.print(battery.voltage, 2);
    lcd.print("V ");
    if      (battery.type == "Alkaline") lcd.print("-> Box A");
    else if (battery.type == "NiMH")     lcd.print("-> Box B");
    else                                 lcd.print("-> End  ");

    // 4. Publish MQTT immediately on detection
    char tsStr[14];
    getTimestampStr(tsStr, sizeof(tsStr));
    char payload[160];
    snprintf(payload, sizeof(payload),
      "{\"type\":\"%s\",\"voltage\":%.2f,\"magnetic\":%s,\"timestamp\":%s}",
      battery.type.c_str(), battery.voltage,
      battery.isMagnetic ? "true" : "false", tsStr);
    client.publish("ewaste/sort/events", payload);

    // 5. Servo timing — wait for battery to travel to gate position, then divert
    if (battery.type == "Alkaline") {
      delay(SERVO_A_DELAY);                 // wait 5s for battery to reach Servo A gate
      servoA.write(SERVO_DIVERT);           // rotate to 80°
      Serial.println("Servo A → 80° (Alkaline box)");
      delay(SERVO_HOLD);                    // hold 3s to let battery fall into bin
      servoA.write(SERVO_REST);             // return to 0°
      Serial.println("Servo A → 0° (reset)");

    } else if (battery.type == "NiMH") {
      delay(SERVO_B_DELAY);                 // wait 10s for battery to reach Servo B gate
      servoB.write(SERVO_DIVERT);           // rotate to 80°
      Serial.println("Servo B → 80° (NiMH box)");
      delay(SERVO_HOLD);                    // hold 3s to let battery fall into bin
      servoB.write(SERVO_REST);             // return to 0°
      Serial.println("Servo B → 0° (reset)");

    } else {
      // Li-ion — no servo needed, battery rides belt to end bin
      Serial.println("No servo — Li-ion passes through to end");
      delay(500);
    }

    // 6. Resume belt
    lcd.clear(); lcd.print("Scanning...");
    beltControl(true);
  }
}
