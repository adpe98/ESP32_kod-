//ThermalCAM and Server
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_AMG88xx.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// Definiera pinnummer för joystickens axlar och knapp
const int pinY = 39; // A2
const int pinX = 34; // A3
const int pinButton = 25; // A1
int oldY, oldX;

unsigned long previousMillis = 0;
const long interval = 100; // 10 gånger per sekund

// WiFi-uppgifter
const char* ssid = "iPSK-UMU";
const char* pwd = "HejHopp!!";
WiFiClient wifiClient;

// MQTT broker setup
const char broker [] = "tfe.iotwan.se";
int port = 1883;
const char* mqtt_usr = "intro22";
const char* mqtt_pwd = "outro";
PubSubClient client(wifiClient);

// Ersätt dessa pins med de faktiska pins vi använder
#define TFT_CS     4
#define TFT_RST    26
#define TFT_DC     25

TFT_eSPI tft = TFT_eSPI();

#define RXD 16
#define TXD 17
//Comment this out to remove the text overlay
#define SHOW_TEMP_TEXT
//low range of the sensor (this will be blue on the screen)
int minT = 15;
#define MINTEMP minT
//high range of the sensor (this will be red on the screen)
int maxT = 50;
#define MAXTEMP maxT

float pixels[AMG88xx_PIXEL_ARRAY_SIZE];

String camColors[] = {"E50015", "E40006", "E30800", "E31700", "E22600", "E13500", "E14400", "E05300", "E06200", "DF7100", "DE7F00", "DE8E00", "DD9C00", "DDAA00",
                      "DCB900", "DBC700", "DBD500", "D1DA00", "C2DA00", "B3D900", "A4D800", "95D800", "86D700", "78D700", "69D600", "5AD500", "4CD500", "3DD400", "2FD400", "21D300", "13D200", "05D200",
                      "00D109", "00D116", "00D024", "00CF32", "00CF40", "00CE4D", "00CE5B", "00CD68", "00CC76", "00CC83", "00CB90", "00CB9E", "00CAAB", "00C9B8", "00C9C4", "00BFC8", "00B1C8", "00A3C7",
                      "0095C6", "0088C6", "007AC5", "006DC5", "005FC4", "0052C3", "0044C3", "0037C2", "002AC2", "001DC1", "0010C0", "0003C0", "0900BF", "1500BF"
                     };

const char* first_half = "FireBot/first_half";
const char* second_half = "FireBot/second_half";
String sensor_data1 = "";
String sensor_data2 = "";
String X;
String Y;

String driver;

void setup() {

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  // WiFi
  Serial.print("Connecting to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pwd);
  // Vänta tills ansluten till WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  // MQTT broker
  client.setServer(broker, port);
  while (!client.connected()) {
    Serial.print("Connecting to MQTT ... ");
    if (client.connect("rierstam", mqtt_usr, mqtt_pwd)) {
      Serial.println("connected");
      client.setCallback(mqttCallback);
      client.subscribe(first_half);
      client.subscribe(second_half);
    } else {
      Serial.print("failed with state: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  tft.init();  // Initiera skärmen
  tft.setRotation(1);  // Sätt skärmrotation om nödvändigt
}

void updatePixelArray(String data, int startIdx) {
  int endIdx = startIdx + AMG88xx_PIXEL_ARRAY_SIZE / 2;
  int dataIndex = 0;
  for (int i = startIdx; i < endIdx && dataIndex < data.length(); i++) {
    int nextComma = data.indexOf(',', dataIndex);
    if (nextComma == -1) nextComma = data.length();
    String tempStr = data.substring(dataIndex, nextComma);
    pixels[i] = tempStr.toFloat();
    dataIndex = nextComma + 1;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("hej");
  payload[length] = '\0'; // Null-terminate the payload
  String message = String((char*)payload);

  if (String(topic) == first_half) {
    updatePixelArray(message, 0);
    Serial.println("First half data: " + message);
  } 
  else if (String(topic) == second_half) {
    updatePixelArray(message, AMG88xx_PIXEL_ARRAY_SIZE / 2);
    Serial.println("Second half data: " + message);
  }
  Serial.println("hejda");
}


void loop() {
  // Hantera WiFi och MQTT
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
  client.loop();

  // Uppdatera skärmen med nya värden
  for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    int colorTemp = constrain((int)pixels[i], MINTEMP, MAXTEMP);
    uint8_t colorIndex = map(colorTemp, MINTEMP, MAXTEMP, 0, sizeof(camColors) / sizeof(String) - 1);

    int row = i / 8;
    int col = i % 8;
    int x = col * (160 / 8);
    int y = row * (128 / 8);
    int w = 160 / 8;
    int h = 128 / 8;

    long color = strtol(camColors[colorIndex].c_str(), NULL, 16);
    tft.fillRect(x, y, w, h, color);
  }

  // Hantera joystick och andra sensorer
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Läs in värden från joystick och förbered en sträng för att skicka
    int valueY = analogRead(pinY);
    int valueX = analogRead(pinX);
    if ((valueY > 1800) && (valueY < 2000)) {
      valueY = 1950;
    }
    if ((valueX > 1800) && (valueX < 2000)) {
      valueX = 1950;
    }
    
    bool buttonPressed = digitalRead(pinButton) == LOW;
    char msg1[50];
    char msg2[50];
    sprintf(msg1, "Y%04d", valueY);
    sprintf(msg2, "X%04d", valueX);

    if (oldX != valueX) {
      client.publish("FireBot/X", msg2);
    }
    if (oldY != valueY) {
      client.publish("FireBot/Y", msg1);
    }
    oldY = valueY;
    oldX = valueX;
  }
}
