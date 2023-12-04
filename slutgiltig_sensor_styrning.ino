#include <Wire.h>
#include <Adafruit_AMG88xx.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_CCS811.h>


#define RXD 16
#define TXD 17
#define air_sensor 26

Adafruit_AMG88xx amg;
Adafruit_CCS811 ccs;

// WiFi-uppgifter
const char* ssid = "iPSK-UMU";
const char* pwd = "HejHopp!!";

// MQTT-inställningar
const char mqttServer [] = "tfe.iotwan.se";
int mqttPort = 1883;
const char* mqttUser = "intro22";
const char* mqttPassword = "outro";
unsigned long previousMillis = 0; // Variabel för att spåra senaste uppdateringen
const long interval = 100; // Intervallet för uppdateringar (1000 ms = 1 sekunder)

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Temperaturintervall för AMG8833-sensorn
int minT = 15;  
int maxT = 50;
float pixels[AMG88xx_PIXEL_ARRAY_SIZE];

// MQTT Topics
const char* first_half = "FireBot/first_half";
const char* second_half = "FireBot/second_half";
const char* xTopic = "FireBot/X";
const char* yTopic = "FireBot/Y";

String XValue = "";
String YValue = "";

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  Serial.begin(115200);
  Wire.begin(23, 22);
  WiFi.begin(ssid, pwd);
  esp_timer_init();

  if (!amg.begin()) {
    Serial.println("Could not find a valid AMG88xx sensor");
    while (1);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("CCS811 test");
  if(!ccs.begin()){
    Serial.println("Failed to start sensor! Please check your wiring.");
    while(1);
  }

  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client", mqttUser, mqttPassword )) {
      Serial.println("Connected");
      mqttClient.subscribe(xTopic);
      mqttClient.subscribe(yTopic);
    } else {
      Serial.print("failed with state ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

void readAndPublishSensorData() {
  amg.readPixels(pixels);

  String tempData = "";
  
  for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    tempData += String(pixels[i]) + ",";
  }

  int data_length = tempData.length();

  String firstHalf = tempData.substring(0, data_length / 2);
  char firstHalfChar[firstHalf.length() + 1];
  firstHalf.toCharArray(firstHalfChar, sizeof(firstHalfChar));
  
  String secondHalf = tempData.substring(data_length / 2);
  char secondHalfChar[secondHalf.length() + 1];
  secondHalf.toCharArray(secondHalfChar, sizeof(secondHalfChar));

  // Publicera första och andra halvan av datat till MQTT
  mqttClient.publish(first_half, firstHalfChar);
  mqttClient.publish(second_half, secondHalfChar);

  Serial.print("First half published: " + firstHalf);
  Serial.print("Second half published: " + secondHalf);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Null-terminate the payload
  String message = String((char*)payload);
  Serial.println("data_received" + message);
  if (String(topic) == xTopic) {
    XValue = message;
    Serial2.print(XValue);    
  } else if (String(topic) == yTopic) {
    YValue = message;
    Serial2.print(YValue);    
  }
}

void loop() 
{
  mqttClient.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // Spara tiden för senaste uppdateringen
    previousMillis = currentMillis;

    // Läs in och publicera sensordata
    readAndPublishSensorData();
  }

  // Skriv ut X- och Y-värdena till seriel monitor 
  Serial.print("X: ");
  Serial.print(XValue);
  Serial.print(", Y: ");
  Serial.print(YValue);
}
