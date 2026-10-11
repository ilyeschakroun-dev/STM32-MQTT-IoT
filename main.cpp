#include <SPI.h>
#include <Wire.h>
#include <WiFiST.h>
#include <PubSubClient.h>
#include <HTS221Sensor.h>
#include <LPS22HBSensor.h>

// ========== IDENTIFIANTS A MODIFIER ==========
char ssid[] = "Ooredoo 5G_B48C7B";
char pass[] = "6VWN25M768";

const char* mqttServer = "192.168.1.249"; // L'IP de ton PC
const int mqttPort = 1883;
// =============================================

const char* topicTemp = "/temperature";
const char* topicHum  = "/humidite";
const char* topicPress = "/pression";

// Configuration WiFi qui fonctionne
SPIClass SPI_3(PC12, PC11, PC10);
WiFiClass WiFi(&SPI_3, PE0, PE1, PE8, PB13);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Capteurs
TwoWire dev_i2c(PB11, PB10);
HTS221Sensor hts(&dev_i2c);
LPS22HBSensor pressure(&dev_i2c);
bool sensorsReady = false;

unsigned long lastPublish = 0;
unsigned long lastMqttRetry = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("==========================");
  Serial.println("STM32L475 - CAPTEURS MQTT");
  Serial.println("==========================");

  // 1. ALLUMAGE WIFI (La solution trouvée)
  Serial.println("[1] Initialisation SPI et WiFi...");
  SPI_3.begin(); 
  delay(1000);

  WiFi.begin(ssid, pass);
  delay(3000); // On lui laisse le temps de se connecter

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WIFI CONNECTE !");
    Serial.print("IP de la carte : ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WIFI NON CONNECTE (Verifie le nom/mdp et le 2.4 GHz)");
  }

  // 2. INITIALISATION CAPTEURS
  Serial.println("[2] Initialisation I2C et Capteurs...");
  dev_i2c.begin();
  if (hts.begin() == 0 && pressure.begin() == 0) {
    if (hts.Enable() == 0 && pressure.Enable() == 0) {
      sensorsReady = true;
      Serial.println("CAPTEURS OK !");
    }
  }
  if (!sensorsReady) {
    Serial.println("ERREUR CAPTEURS");
  }

  // 3. CONFIGURATION MQTT
  mqtt.setServer(mqttServer, mqttPort);
}

void loop() {
  // Reconnexion WiFi si perdu
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Perte WiFi, reconnexion...");
    WiFi.begin(ssid, pass);
    delay(5000);
    return;
  }

  // Reconnexion MQTT si perdu
  if (!mqtt.connected()) {
    if (millis() - lastMqttRetry >= 5000) {
      lastMqttRetry = millis();
      Serial.println("Tentative de connexion au PC (MQTT)...");
      
      // On tente de se connecter avec le nom client "stm32-iot"
      if (mqtt.connect("stm32-iot")) {
        Serial.println("MQTT CONNECTE AU PC !");
      } else {
        Serial.print("Echec MQTT (Code erreur : ");
        Serial.print(mqtt.state());
        Serial.println(") - Le PC ecoute-t-il ?");
      }
    }
    return;
  }

  mqtt.loop(); // Fait tourner le moteur MQTT

  if (!sensorsReady) return;

  // Publication toutes les 5 secondes
  if (millis() - lastPublish >= 5000) {
    lastPublish = millis();

    float temperature = 0, humidite = 0, pression = 0;
    hts.GetTemperature(&temperature);
    hts.GetHumidity(&humidite);
    pressure.GetPressure(&pression);

    // TEMPERATURE
    String strTemp = String(temperature, 2);
    mqtt.publish(topicTemp, strTemp.c_str());

    // HUMIDITE
    String strHum = String(humidite, 2);
    mqtt.publish(topicHum, strHum.c_str());

    // PRESSION
    String strPress = String(pression, 2);
    mqtt.publish(topicPress, strPress.c_str());

    Serial.println("-------------------------");
    Serial.print("Temp : "); Serial.print(temperature); Serial.println(" C");
    Serial.print("Hum  : "); Serial.print(humidite); Serial.println(" %");
    Serial.print("Pres : "); Serial.print(pression); Serial.println(" hPa");
    Serial.println("-> Envoyees sur Mosquitto !");
  }
}