#include <Wire.h>
#include "WiFi.h"
#include <WiFiUdp.h>
#include <SPI.h>
#include <InfluxDBClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <sstream>
using namespace std;

#define INFLUXDB_HOST "192.168.0.3"
#define INFLUXDB_PORT 8086
#define INFLUXDB_TOKEN "FEtP4M2cosTnZ2SCLmFgr8ELeW10R8qLWwl7B6kXOJuOUbYp6NUK3NCmVtglsBeg3M5pI6_cNwWzEnO-vnairQ=="
#define INFLUXDB_ORG "iot"
#define INFLUXDB_BUCKET "eduroamIoT"

#define INFLUXDB_HOST1 "192.168.1.40"
#define INFLUXDB_PORT1 8086
#define INFLUXDB_TOKEN1 "73ugAny7dK14n4q5jns9YmT_TM8g9GrSoXVWTF8WSeIA2rvCdC4OaB2giQKkUeb4bBgIXgeR1O8T6T64Pol2Ow=="
#define INFLUXDB_ORG1 "uom"
#define INFLUXDB_BUCKET1 "eduroamIoT"

#define DEFAULT_READS_PER_METRIC 16

// const char* SSID = "NOWTVHSCFW";
// const char* password = "R2jvpFLN89Iu";
const char* SSID = "MOVISTAR_E9AD";
const char* password = "2KsewRQcLuDsVQUr5Pvj";

WiFiClient client;
WiFiUDP Udp;
InfluxDBClient influxClient;
Adafruit_BME280 bme;

float temp, hum, bar_p, dif_p;
vector<float> tempVect, humVect, barVect, difVect;

float lowerTempThreshold = 5;
float upperTempThreshold = 50;
float lowerHumThreshold = 25;
float upperHumThreshold = 75;
float lowerBarThreshold = 500;
float upperBarThreshold = 1500;
float lowerDifThreshold = -500;
float upperDifThreshold = 500;

int loopCount = 0;
int loopInterval;
int readsPerMetric = DEFAULT_READS_PER_METRIC;

void setup() {

  Serial.begin(9600);

  delay(3000);

  // WiFi connection
  
  Serial.println("");
  Serial.println("Connecting to: ");
  Serial.println(SSID);

  WiFi.begin(SSID, password);

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start the sensor

  bool status;
  status = bme.begin(0x77);

  influxClient = InfluxDBClient(INFLUXDB_HOST1, INFLUXDB_PORT1, INFLUXDB_ORG1, INFLUXDB_BUCKET1, INFLUXDB_TOKEN1, "testsensor");
  loopInterval = influxClient.getPushInterval() / readsPerMetric;

  Serial.print("The statusByte -> ");
  Serial.println(influxClient.getStatusByte());
  Serial.print("The monitoringStatus -> ");
  Serial.println(influxClient.getMonitoringStatus());
  Serial.print("The statusByteIdxPointer -> ");
  Serial.println(influxClient.getStatusByteIdxPointer());
    

  influxClient.addMetricName("temperature");
  influxClient.addMetricName("humidity");
  influxClient.addMetricName("barometric_pressure");
  influxClient.addMetricName("differential_pressure");
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
  Serial.println("");

  temp = bme.readTemperature();
  hum = bme.readHumidity();
  bar_p = bme.readPressure() / 100.0F;
  dif_p = analogRead(34) * 10 / 4096;

  printValues(temp, hum, bar_p, dif_p);

  tempVect.push_back(temp);
  humVect.push_back(hum);
  barVect.push_back(bar_p);
  difVect.push_back(dif_p);

  vector<KeyValue> tags, values;
  std::ostringstream tempString;

  if (loopCount >= (readsPerMetric - 1)) {
    
    bool tempOk = influxClient.createMetric("temperature", tempVect, tags, values, tempString, lowerTempThreshold, upperTempThreshold);
    bool humOk = influxClient.createMetric("humidity", humVect, tags, values, tempString, lowerHumThreshold, upperHumThreshold);
    bool barOk = influxClient.createMetric("barometric_pressure", barVect, tags, values, tempString, lowerBarThreshold, upperBarThreshold);
    bool difOk = influxClient.createMetric("differential_pressure", difVect, tags, values, tempString, lowerDifThreshold, upperDifThreshold);

    influxClient.checkMonitoringLevels(tempOk && humOk && barOk && difOk, readsPerMetric);

    Serial.print("Status byte: ");
    Serial.println(influxClient.getStatusByte());
    Serial.print("Monitoring Status 3: ");
    Serial.println(influxClient.getMonitoringStatus());
    
    loopCount = 0;
    loopInterval = influxClient.getPushInterval() / readsPerMetric;

    Serial.print("The statusByte -> ");
    Serial.println(influxClient.getStatusByte());
    Serial.print("The monitoringStatus -> ");
    Serial.println(influxClient.getMonitoringStatus());
    Serial.print("The statusByteIdxPointer -> ");
    Serial.println(influxClient.getStatusByteIdxPointer());
    Serial.println("Next step: tick()");
    
  } else loopCount++;
  
  influxClient.tick();
  delay(loopInterval);

  Serial.println();
}


// PRINT VALUES IN SERIAL MONITOR -------------------------------------------------------------------

void printValues(float temp, float hum, float bar_p, float dif_p) {

  Serial.print("Temperature = ");
  Serial.print(temp);
  Serial.println(" *C");

  Serial.print("Humidity = ");
  Serial.print(hum);
  Serial.println(" %");

  Serial.print("Pressure = ");
  Serial.print(bar_p);
  Serial.println(" hPa");

  Serial.print("Pressure2 = ");
  Serial.print(dif_p);
  Serial.println(" bar");
  
}
