#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// CREATE SECOND I2C BUS for TWO SENSORS

TwoWire I2CBME2 = TwoWire(1);

// BME280 OBJECTS

Adafruit_BME280 bme1;
Adafruit_BME280 bme2;

// SENSOR IDS
// These are names of the boards.  Make them what you want to be shown on the web dashboard and google sheets
char SENSOR1_ID[] = "board1";
char SENSOR2_ID[] = "board2";

// Need to pull this out, the calibration is done on the pi side now
// Sensor 1 calibration

float TEMP1_OFFSET = 0.0;
float HUM1_OFFSET  = 0.0;
float PRES1_OFFSET = 0.0;

// Sensor 2 caliibration


float TEMP2_OFFSET = 0.0;
float HUM2_OFFSET  = 0.0;
float PRES2_OFFSET = 0.0;

// GATEWAY MAC

// You'll need to pull the MAC of the gateway ESP32 and enter it below.  I left a MAC,
// so you can see how to translate it

// 8c:94:df:b9:39:41
uint8_t receiverMAC[] = {
  0x8C, 0x94, 0xDF, 0xB9, 0x39, 0x41
};

// DATA STRUCTURE

typedef struct struct_message {

  char sensor[16];

  float temperature;
  float humidity;
  float pressure;

  int rssi;

} struct_message;

struct_message sensorData;

// SEND CALLBACK

void OnDataSent(
    const wifi_tx_info_t *info,
    esp_now_send_status_t status
) {

  Serial.print("Send Status: ");

  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("SUCCESS");
  } else {
    Serial.println("FAILED");
  }
}

// SEND SENSOR FUNCTION

void sendSensor(
    Adafruit_BME280 &bme,
    const char *sensorID,
    float tempOffset,
    float humOffset,
    float presOffset
) {

  float tempC = bme.readTemperature();

  float tempF = (tempC * 9.0 / 5.0) + 32.0;

  float humidity = bme.readHumidity();

  float pressure =
      bme.readPressure() / 100.0F;
  
    tempF += tempOffset;
  humidity += humOffset;
  pressure += presOffset;
  
  strcpy(sensorData.sensor, sensorID);

  sensorData.temperature = tempF;
  sensorData.humidity    = humidity;
  sensorData.pressure    = pressure;

  sensorData.rssi = 0;

  // Debugging
  Serial.println("================================");

  Serial.print("Sensor: ");
  Serial.println(sensorData.sensor);

  Serial.print("Temperature: ");
  Serial.print(sensorData.temperature);
  Serial.println(" F");

  Serial.print("Humidity: ");
  Serial.print(sensorData.humidity);
  Serial.println(" %");

  Serial.print("Pressure: ");
  Serial.print(sensorData.pressure);
  Serial.println(" hPa");

  esp_err_t result = esp_now_send(
      receiverMAC,
      (uint8_t *) &sensorData,
      sizeof(sensorData)
  );

  if (result == ESP_OK) {

    Serial.println("Data Sent");

  } else {

    Serial.println("Send Error");
  }

  delay(250);
}

void setup() {

  Serial.begin(115200);

  delay(1000);

  // WIFI

  WiFi.mode(WIFI_STA);

  WiFi.disconnect();

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  Serial.println();
  Serial.println("ESP32 Dual I2C BME280");
  Serial.println();

  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // INIT I2C BUS #1
  // Pins for borad 1
  Wire.begin(21, 22);

  // INIT I2C BUS #2
  // Pins for board 2
  I2CBME2.begin(25, 26);

  // INIT BME280 #1

  bool status1 = bme1.begin(0x76, &Wire);

  if (!status1) {

    Serial.println("BME280 #1 NOT FOUND");

    while (1);
  }

  // INIT BME280 #2
  Serial.println("Scanning second I2C bus...");

  for (byte addr = 1; addr < 127; addr++) {

    I2CBME2.beginTransmission(addr);

    if (I2CBME2.endTransmission() == 0) {

      Serial.print("Found device at 0x");
      Serial.println(addr, HEX);
    }
  }
  bool status2 = bme2.begin(0x76, &I2CBME2);

  if (!status2) {

    Serial.println("BME280 #2 NOT FOUND");

    while (1);
  }

  Serial.println("Both BME280 sensors initialized");

  // ESP-NOW INIT

  if (esp_now_init() != ESP_OK) {

    Serial.println("ESP-NOW Init Failed");

    return;
  }

  esp_now_register_send_cb(OnDataSent);

  // REGISTER PEER

  esp_now_peer_info_t peerInfo = {};

  memcpy(peerInfo.peer_addr, receiverMAC, 6);

  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {

    Serial.println("Failed to add peer");

    return;
  }

  Serial.println("ESP-NOW Ready");
}

void loop() {

  sendSensor(
    bme1,
    SENSOR1_ID,
    TEMP1_OFFSET,
    HUM1_OFFSET,
    PRES1_OFFSET
  );

  delay(100);

  sendSensor(
    bme2,
    SENSOR2_ID,
    TEMP2_OFFSET,
    HUM2_OFFSET,
    PRES2_OFFSET
  );

  delay(5000);
}
