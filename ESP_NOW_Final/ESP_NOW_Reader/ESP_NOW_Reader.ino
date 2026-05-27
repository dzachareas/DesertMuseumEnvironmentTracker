#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;

// SENSOR IDENTIFICATION
// This is where you name your sensor, this is what will be reported to the pi and the google sheet
char SENSOR_ID[] = "test1";

// GATEWAY MAC ADDRESS
// Replace with gateway ESP32 MAC
//8c:94:df:b9:39:41
uint8_t receiverMAC[] = {0x8C, 0x94, 0xDF, 0xB9, 0x39, 0x41};

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

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");

  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("SUCCESS");
  } else {
    Serial.println("FAILED");
  }
}

void setup() {

  Serial.begin(115200);

  delay(1000);

  // WIFI STA MODE

  WiFi.mode(WIFI_STA);

  WiFi.disconnect();

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  Serial.println();
  Serial.println("ESP32 ESP-NOW SENSOR NODE");
  Serial.println();

  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // BME280 INIT

  bool status;

  status = bme.begin(0x76);

  if (!status) {

    Serial.println("Could not find BME280 sensor!");

    while (1);
  }

  // ESP-NOW INIT

  if (esp_now_init() != ESP_OK) {

    Serial.println("ESP-NOW Init Failed");

    return;
  }

  esp_now_register_send_cb(OnDataSent);

  // REGISTER PEER

  esp_now_peer_info_t peerInfo;

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

  // READ SENSOR

  float tempC = bme.readTemperature();

  float tempF = (tempC * 9.0 / 5.0) + 32.0;

  float humidity = bme.readHumidity();

  float pressure =
      bme.readPressure() / 100.0F;

  // BUILD DATA PACKET

  strcpy(sensorData.sensor, SENSOR_ID);

  sensorData.temperature = tempF;
  sensorData.humidity    = humidity;
  sensorData.pressure    = pressure;

  //sensorData.rssi = WiFi.RSSI();
  sensorData.rssi = 0;

  // DEBUG OUTPUT

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

  Serial.print("RSSI: ");
  Serial.println(sensorData.rssi);

  // SEND DATA

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

  delay(5000);
}
