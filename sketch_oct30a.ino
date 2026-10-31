#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <PZEM004Tv30.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Wi-Fi Credentials (PLACEHOLDERS FOR GITHUB)
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Firebase Credentials (PLACEHOLDERS FOR GITHUB)
#define DATABASE_SECRET "YOUR_FIREBASE_DATABASE_SECRET"
#define DATABASE_URL "https://iot-energylink-default-rtdb.asia-southeast1.firebasedatabase.app"

// PZEM-004T HardwareSerial Pins
#define PZEM_RX_PIN 16  // Connects to PZEM TX
#define PZEM_TX_PIN 17  // Connects to PZEM RX

PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// Initialize 16x2 LCD at common I2C address 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
const unsigned long timerDelay = 2000;

void setup() {
  Serial.begin(115200);

  // --- LCD Hardware Stability Fixes ---
  delay(2000);             // Give the yellow LCD time to boot completely
  Wire.begin(21, 22);      // Explicitly set I2C pins
  Wire.setClock(50000);    // Slow down I2C clock to keep the screen synced
  // ------------------------------------

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("EnergyLink IoT");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Show connected status on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  delay(1000);
  lcd.clear();

  // Configure Firebase
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();

    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    float frequency = pzem.frequency();
    float pf = pzem.pf();

    // Prevent NaN errors if the sensor communication drops
    if (isnan(voltage))   voltage = 0.0;
    if (isnan(current))   current = 0.0;
    if (isnan(power))     power = 0.0;
    if (isnan(energy))    energy = 0.0;
    if (isnan(frequency)) frequency = 50.0;
    if (isnan(pf))        pf = 1.0;

    // --- Hardware Ghost Data Filter ---
    // If the AC mains is disconnected, force active metrics to true zero
    if (voltage < 50.0) {
        voltage = 0.0;
        current = 0.0;
        power = 0.0;
        frequency = 0.0;
        pf = 0.0;
    }
    // ----------------------------------

    // --- Update LCD Display (4 Parameters) ---
    // Top Row: Voltage and Power
    String row1 = "V:" + String(voltage, 1) + "V P:" + String((int)power) + "W";
    
    // Bottom Row: Current and Energy
    String row2 = "I:" + String(current, 2) + "A E:" + String(energy, 2) + "k";

    // Add invisible spaces to the end of the text to erase old digits
    while(row1.length() < 16) row1 += " ";
    while(row2.length() < 16) row2 += " ";

    // Print to the screen
    lcd.setCursor(0, 0);
    lcd.print(row1);
    
    lcd.setCursor(0, 1);
    lcd.print(row2);
    // -----------------------------------------

    // Package data for Firebase
    FirebaseJson json;
    json.set("voltage", voltage);
    json.set("current", current);
    json.set("power", power);
    json.set("energy", energy);
    json.set("frequency", frequency);
    json.set("pf", pf);

    Serial.printf("Pushing -> V: %.1fV, I: %.2fA, P: %.1fW\n", voltage, current, power);

    // Push to the Realtime Database
    if (Firebase.RTDB.setJSON(&fbdo, "/Energy", &json)) {
      Serial.println("Firebase update SUCCESS");
    } else {
      Serial.print("Firebase update FAILED: ");
      Serial.println(fbdo.errorReason());
    }
  }
}