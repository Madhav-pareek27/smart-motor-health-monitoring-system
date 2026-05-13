#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <MPU6050.h>
#include <math.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// ------------------ DHT ------------------

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);


// ------------------ MOTOR ------------------

#define IN1 18
#define IN2 19
#define ENA 5


// ------------------ VOLTAGE ------------------

#define VOLTAGE_PIN 34


// ------------------ LCD ------------------

LiquidCrystal_I2C lcd(0x27, 16, 2);


// ------------------ MPU ------------------

MPU6050 mpu;


// ------------------ WIFI ------------------

const char* ssid = "Madss";
const char* password = "00000000";


// ------------------ FIREBASE ------------------

#define FIREBASE_HOST "smart-motor-health-system-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "j1hZaWXsZCkBIW1fhu2B9c5myOzUavbQKufzBImB"

FirebaseData fbData;
FirebaseConfig config;
FirebaseAuth auth;


// ------------------ THRESHOLDS ------------------

float tempThreshold = 50.0;
float vibThreshold = 2.0;

float minVoltage = 6.0;
float maxVoltage = 9.5;

float temperature = 0;
float voltage = 0;
float vibration = 0;


// ------------------ SETUP ------------------

void setup()
{
    Serial.begin(115200);
    delay(2000);

    // Motor Pins
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(ENA, OUTPUT);

    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(ENA, HIGH);

    // Start Sensors
    dht.begin();

    Wire.begin(21, 22);
    mpu.initialize();

    // LCD Start
    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Motor Monitor");

    delay(2000);
    lcd.clear();


    // ------------------ CONNECT WIFI ------------------

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    lcd.print("Connecting WiFi");
    Serial.print("Connecting to WiFi");

    int retry = 0;

    while (WiFi.status() != WL_CONNECTED && retry < 20)
    {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi Connected!");
        Serial.println(WiFi.localIP());

        lcd.clear();
        lcd.print("WiFi Connected");
    }
    else
    {
        Serial.println("\nWiFi Failed!");

        lcd.clear();
        lcd.print("WiFi Failed");
    }

    delay(2000);
    lcd.clear();


    // ------------------ CONNECT FIREBASE ------------------

    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    if (Firebase.ready())
    {
        Serial.println("Firebase Ready!");
        lcd.print("Firebase Ready");
    }
    else
    {
        Serial.println("Firebase Not Ready!");
        lcd.print("Firebase Error");
    }

    delay(2000);
    lcd.clear();
}


// ------------------ VOLTAGE READ ------------------

float readVoltage()
{
    int raw = analogRead(VOLTAGE_PIN);

    float v = (raw / 4095.0) * 3.3;

    return v * 5.0;
}


// ------------------ VIBRATION READ ------------------

float readVibration()
{
    int16_t ax, ay, az;

    mpu.getAcceleration(&ax, &ay, &az);

    float x = ax / 16384.0;
    float y = ay / 16384.0;
    float z = az / 16384.0;

    return sqrt(x * x + y * y + z * z);
}


// ------------------ LOOP ------------------

void loop()
{
    temperature = dht.readTemperature();

    if (isnan(temperature))
        temperature = 0;

    voltage = readVoltage();
    vibration = readVibration();

    String status = "OK";

    if (temperature > tempThreshold)
        status = "TEMP HIGH";

    else if (vibration > vibThreshold)
        status = "VIBRATION!";

    else if (voltage < minVoltage || voltage > maxVoltage)
        status = "VOLT ISSUE";


    // LCD Display

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 1);

    lcd.print(" V:");
    lcd.print(voltage, 1);

    lcd.setCursor(0, 1);
    lcd.print("V:");
    lcd.print(vibration, 2);

    lcd.print(" ");
    lcd.print(status);


    // Serial Log

    Serial.print("Temp: ");
    Serial.print(temperature);

    Serial.print(" Volt: ");
    Serial.print(voltage);

    Serial.print(" Vib: ");
    Serial.println(vibration);


    // Push to Firebase

    if (Firebase.ready() && WiFi.status() == WL_CONNECTED)
    {
        bool fault =
            (temperature > tempThreshold ||
             vibration > vibThreshold ||
             voltage < minVoltage ||
             voltage > maxVoltage);

        Firebase.setFloat(fbData, "/motor/data/temperature", temperature);
        Firebase.setFloat(fbData, "/motor/data/voltage", voltage);
        Firebase.setFloat(fbData, "/motor/data/vibration", vibration);
        Firebase.setBool(fbData, "/motor/data/fault", fault);

        Serial.println("Firebase ✔ pushed");
    }
    else
    {
        Serial.println("WiFi/Firebase not ready");
    }

    delay(2000);
}
