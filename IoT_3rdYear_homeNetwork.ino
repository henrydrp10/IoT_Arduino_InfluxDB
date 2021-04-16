#include <Wire.h>
#include "WiFi.h"
#include <WiFiUdp.h>
#include <SPI.h>
#include <InfluxDBClient.h>
#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "FS.h"
#include "SD.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <sstream>
using namespace std;

#define INFLUXDB_HOST "192.168.1.60"
#define INFLUXDB_PORT 8086
#define INFLUXDB_TOKEN "FEtP4M2cosTnZ2SCLmFgr8ELeW10R8qLWwl7B6kXOJuOUbYp6NUK3NCmVtglsBeg3M5pI6_cNwWzEnO-vnairQ=="
#define INFLUXDB_ORG "iot"
#define INFLUXDB_BUCKET "iot"

#define DEFAULT_READS_PER_METRIC 16

const char* SSID = "MOVISTAR_E9AD";
const char* password = "2KsewRQcLuDsVQUr5Pvj";

WiFiClient client;
WiFiUDP Udp;
InfluxDBClient influxClient;

Adafruit_BME280 bme;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

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
  delay(4000);

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
  u8g2.begin();
  bool status = bme.begin(0x77);

  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
  }

  File logFile = SD.open("/log.txt", FILE_APPEND);
  logFile.print("Starting data logging at: ");
  logFile.close();

  configTime(3600, 1, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
  struct tm tmstruct ;
  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct, 5000);

  std::ostringstream timeNow;
  timeNow << tmstruct.tm_mday << "/" << tmstruct.tm_mon + 1 << "/" << tmstruct.tm_year + 1900 << ", " << tmstruct.tm_hour << ":" << tmstruct.tm_min << ":" << tmstruct.tm_sec;
  log_time(timeNow.str());

  influxClient = InfluxDBClient(INFLUXDB_HOST, INFLUXDB_PORT, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, "testsensor");
  loopInterval = influxClient.getPushInterval() / DEFAULT_READS_PER_METRIC;

  float storageMB = SD.usedBytes() / (1024 * 1024);

  std::ostringstream mbUsed;
  mbUsed << "Storage used: " << storageMB << "MB";

  std::ostringstream monStatus;
  monStatus << "Monitoring status: " << influxClient.getMonitoringStatus();

  u8g2_prepare();
  u8g2.clearBuffer();
  u8g2.drawStr( 0, 16, monStatus.str().c_str());
  u8g2.drawStr( 0, 48, mbUsed.str().c_str());
  u8g2.sendBuffer();

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

  vector<KeyValue> values;
  std::ostringstream tempString;

  if (loopCount >= (readsPerMetric - 1)) {

    configTime(3600, 1, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
    struct tm tmstruct;
    tmstruct.tm_year = 0;
    getLocalTime(&tmstruct, 5000);

    std::ostringstream timeNow;
    timeNow << tmstruct.tm_mday << "/" << tmstruct.tm_mon + 1 << "/" << tmstruct.tm_year + 1900 << ", " << tmstruct.tm_hour << ":" << tmstruct.tm_min << ":" << tmstruct.tm_sec;
    log_time(timeNow.str());

    bool tempOk = influxClient.createMetric("temperature", tempVect, values, tempString, lowerTempThreshold, upperTempThreshold);
    log_data("temperature", tempVect);

    bool humOk = influxClient.createMetric("humidity", humVect, values, tempString, lowerHumThreshold, upperHumThreshold);
    log_data("humidity", humVect);

    bool barOk = influxClient.createMetric("barometric_pressure", barVect, values, tempString, lowerBarThreshold, upperBarThreshold);
    log_data("barometric_pressure", barVect);

    bool difOk = influxClient.createMetric("differential_pressure", difVect, values, tempString, lowerDifThreshold, upperDifThreshold);
    log_data("differential_pressure", difVect);

    influxClient.checkMonitoringLevels(tempOk && humOk && barOk && difOk, readsPerMetric);
    log_status();

    // OLED DISPLAY MONITORING STATUS AND SD STORAGE USED

    float storageMB = SD.usedBytes() / (1024 * 1024);

    std::ostringstream mbUsed;
    mbUsed << "Storage Used: " << storageMB << "MB";

    std::ostringstream monStatus;
    monStatus << "Monitoring status: " << influxClient.getMonitoringStatus();

    u8g2.clearBuffer();
    u8g2.drawStr( 0, 16, monStatus.str().c_str());
    u8g2.drawStr( 0, 48, mbUsed.str().c_str());
    u8g2.sendBuffer();

    // RESET LOOP COUNT AND CLEAR VALUE VECTORS FOR NEXT SET

    loopCount = 0;
    loopInterval = influxClient.getPushInterval() / readsPerMetric;

    tempVect.clear();
    humVect.clear();
    barVect.clear();
    difVect.clear();

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

  Serial.print("Bar Pressure = ");
  Serial.print(bar_p);
  Serial.println(" hPa");

  Serial.print("Diff Pressure = ");
  Serial.print(dif_p);
  Serial.println(" bar");

}

// PREPARE OLED SCREEN FOR WRITING -------------------------------------------------------------------

void u8g2_prepare(void) {

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);

}

// LOG METHODS TO SD ---------------------------------------------------------------------------------

void log_time(string timeNow)
{
  File logFile = SD.open("/log.txt", FILE_APPEND);
  logFile.println(timeNow.c_str());
  logFile.println();
  logFile.close();
}

void log_data(string metric, vector<float> values)
{
  std::ostringstream str;
  str << metric << " - Values: ";

  for (int i = 0; i < values.size() - 1; i++)
  {
    str << values.at(i) << ", ";
  }
  str << values.at(values.size() - 1);

  std::ostringstream str2;
  str2 << "Status byte: " << toBinary(influxClient.getStatusByte());

  File logFile = SD.open("/log.txt", FILE_APPEND);
  logFile.println(str.str().c_str());
  logFile.println(str2.str().c_str());
  logFile.close();
}

void log_status()
{
  std::ostringstream str;
  str << "Monitoring status: " << influxClient.getMonitoringStatus();

  File logFile = SD.open("/log.txt", FILE_APPEND);
  logFile.println(str.str().c_str());
  logFile.println();
  logFile.close();
}

// INT TO BINARY -------------------------------------------------------------------------------------

string toBinary(int n)
{
    string r;
    while(n != 0)
    {
      r = (n % 2 == 0 ? "0":"1") + r;
      n /= 2;
    }
    return r;
}
