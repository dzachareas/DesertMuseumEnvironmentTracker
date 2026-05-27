#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

// DATA STRUCTURE
// Must EXACTLY match sender

typedef struct struct_message {

  char sensor[16];

  float temperature;
  float humidity;
  float pressure;

  int rssi;

} struct_message;

struct_message incomingData;

// CALLBACK WHEN DATA RECEIVED

void OnDataRecv(
    const esp_now_recv_info_t *recv_info,
    const uint8_t *data,
    int len
) {

  if (len != sizeof(struct_message)) {
    return; // reject corrupted packets
  }

  struct_message msg;
  memcpy(&msg, data, sizeof(msg));

  int rssi = recv_info->rx_ctrl->rssi;

  StaticJsonDocument<256> doc;

  doc["sensor"] = msg.sensor;
  doc["temperature"] = msg.temperature;
  doc["humidity"] = msg.humidity;
  doc["pressure"] = msg.pressure;
  doc["rssi"] = rssi;

  char buffer[128];

  size_t n = serializeJson(doc, buffer);

  buffer[n] = '\n';

  Serial.write(buffer, n);
  Serial.write('\n');
}

void setup() {

  Serial.begin(115200);

  delay(1000);

  //Serial.println();
  //Serial.println("ESP32 Gateway Receiver");
  //Serial.println();

  // WIFI STA MODE
  // Required for ESP-NOW

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  
  // INIT ESP-NOW

  if (esp_now_init() != ESP_OK) {

    //Serial.println("ESP-NOW Init Failed");

    return;
  }

  // REGISTER RECEIVE CALLBACK

  esp_now_register_recv_cb(OnDataRecv);

  //Serial.println();
  //Serial.println("ESP-NOW Gateway Ready");
}

void loop() {

}
