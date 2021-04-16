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

float lowT1 = 0;
float lowT2 = 0;
float lowT3 = 0;
float highT1 = 100;
float highT2 = 100;
float highT3 = 100;

float t1, t2, t3;
vector<float> t1Vect, t2Vect, t3Vect;
int testingCount = 0;

int loopInterval;
int loopCount = 0;
int readsPerMetric = 16;

void setup() {

  Serial.begin(9600);
  delay(4000);

  Serial.println("STARTING TESTING PROGRAM:");

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

  // Starting device and modules
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

  influxClient.addMetricName("test_metric_1");
  influxClient.addMetricName("test_metric_2");
  influxClient.addMetricName("test_metric_3");
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
  Serial.println("");

  if (testingCount < 16)          // First 16 measures all OK
  {
    t1 = rand() % 100;
    t2 = rand() % 100;
    t3 = rand() % 100;

    t1Vect.push_back(t1);
    t2Vect.push_back(t2);
    t3Vect.push_back(t3);
  }
  else if (testingCount < 32)     // Next 16 measures are not OK (WARNING)
  {
    t1 = rand() % 100;
    t2 = rand() % 100 - 120;
    t3 = rand() % 100 + 120;

    t1Vect.push_back(t1);
    t2Vect.push_back(t2);
    t3Vect.push_back(t3);
  }
  else if (testingCount < 60)     // Next 28 measures are not OK (ALERT + 1)
  {
    t1 = rand() % 100 + 120;
    t2 = rand() % 100;
    t3 = rand() % 100;

    t1Vect.push_back(t1);
    t2Vect.push_back(t2);
    t3Vect.push_back(t3);
  }
  else if (testingCount < 104)    // Next 44 measures are OK (all way down to OK_)
  {
    t1 = rand() % 100;
    t2 = rand() % 100;
    t3 = rand() % 100;

    t1Vect.push_back(t1);
    t2Vect.push_back(t2);
    t3Vect.push_back(t3);
  }
  else
  {
    Serial.println("\nDONE\n");
    exit(0);
  }

  printValues(t1, t2, t3);

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

    bool t1_OK = influxClient.createMetric("test_metric_1", t1Vect, values, tempString, lowT1, highT1);
    log_data("test_metric_1", t1Vect);

    bool t2_OK = influxClient.createMetric("test_metric_2", t2Vect, values, tempString, lowT2, highT2);
    log_data("test_metric_2", t2Vect);

    bool t3_OK = influxClient.createMetric("test_metric_3", t3Vect, values, tempString, lowT3, highT3);
    log_data("test_metric_3", t3Vect);

    influxClient.checkMonitoringLevels(t1_OK && t2_OK && t3_OK, readsPerMetric);
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

    t1Vect.clear();
    t2Vect.clear();
    t3Vect.clear();

  } else loopCount++;

  influxClient.tick();
  delay(loopInterval);

  testingCount++;
  Serial.println();
}


// PRINT VALUES IN SERIAL MONITOR -------------------------------------------------------------------

void printValues(float t1, float t2, float t3) {

  Serial.print("Test metric 1 = ");
  Serial.println(t1);

  Serial.print("Test metric 2 = ");
  Serial.println(t2);

  Serial.print("Test metric 3 = ");
  Serial.println(t3);

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
