#include "Arduino.h"
#include <WiFi.h>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

#define DEFAULT_BUFFER_SIZE 100
#define DEFAULT_PUSH_INTERVAL 120000
#define DEFAULT_HTTP_RETRIES 3
#define DEFAULT_HTTP_TIMEOUT 3000
#define DEFAULT_HTTP_RETRY_DELAY 1000
#define DEFAULT_WIFI_SERVER_PORT 8080
#define SENSOR_STATUS_FETCH_ENDPOINT "GET /status"
#define SENSOR_STATUS_MODIFY_ENDPOINT "PATCH /status"
#define HTTP_REQUEST_MAX_SIZE 10240

#define HTTP_HEADER_RESPONSE_200 "HTTP/1.1 200 OK"
#define HTTP_HEADER_RESPONSE_204 "HTTP/1.1 204 No Content"
#define HTTP_HEADER_RESPONSE_400 "HTTP/1.1 400 Bad Request"
#define HTTP_HEADER_RESPONSE_404 "HTTP/1.1 404 Not Found"
#define HTTP_HEADER_CONTENT_TYPE_JSON "Content-Type: application/json"
#define HTTP_HEADER_CONNECTION_CLOSE "Connection: close"

#define DEFAULT_ALERT_BOUND 5
#define DEFAULT_OK_BOUND 0
#define DEFAULT_STATUS_BYTE 0b0000000000100001

// statusByte from MSB to LSB: lastMetricOK, >, <, ALERT, WARNING, OK 

#define EEPROM_INFLUX_HOST_OFFSET 1
#define EEPROM_INFLUX_PORT_OFFSET 17
#define EEPROM_INFLUX_ORG_OFFSET 21
#define EEPROM_INFLUX_BUCKET_OFFSET 122
#define EEPROM_INFLUX_AUTH_TOKEN_OFFSET 223
#define EEPROM_SENSOR_GROUP_OFFSET 324
#define EEPROM_PUSH_INTERVAL_OFFSET 425
#define EEPROM_STATUS_BYTE_OFFSET 429
#define EEPROM_SIZE 431

enum protocol { UDP, HTTP };
enum monitoringStatus { OK, WARNING, ALERT };

class KeyValue {
    public:
        KeyValue(uint8_t key, string value);
        string toString();
    private:
        uint8_t key;
        string value;
};

class Datapoint {
    public:
        Datapoint(uint8_t metric, vector<KeyValue> tags, vector<KeyValue> values);
        string toString();
        void addTag(KeyValue tag);
        void addValue(KeyValue value);
    private:
        uint8_t metric;
        vector<KeyValue> tags;
        vector<KeyValue> values;
};

class InfluxDBClient {
    public:
        InfluxDBClient();
        InfluxDBClient(string influxHost, int influxPort, string influxOrg, string influxBucket, string authToken, string sensorGroup);
        InfluxDBClient(string influxHost, int influxPort, string influxOrg, string influxBucket, string authToken, enum protocol protocol,
                       int pushInterval, int bufferSize, int httpRetries, int httpTimeout, int httpRetryDelay, string sensorGroup);
        
        void addDatapoint(Datapoint dp);
        bool addMetricName(string metric);
        bool createMetric(string metric, vector<float> &valVect, vector<KeyValue> &tags, vector<KeyValue> &values, std::ostringstream &tempString, float lowerThreshold, float upperThreshold);
        void checkMonitoringLevels(bool metricsOk);

        void flushMetrics();
        void tick();

        uint8_t getStatusByte();
        void setStatusByte(uint16_t newStatusByte);
        int getPushInterval();
        void setPushInterval(int newPushInterval);

    private:
        void initMap();
        bool pushMetrics(vector<Datapoint> metrics);
        bool pushMetricsHTTP(vector<Datapoint> metrics);
        bool waitForResponse();
        void handleHTTPRequest(WiFiClient client);
        void sendSensorStatus(WiFiClient client);
        void modifySensorStatus(WiFiClient client, string request);
        bool handleResponse();
        int getResponseCode();
        bool is2xxResponseCode(int code);
        string makeHTTPRequestBody(vector<Datapoint> metrics);
        bool checkHTTPConnection();
        bool getHTTPConnection();
        void loadSensorConfig();
        void saveSensorConfig();

        int bufferSize = DEFAULT_BUFFER_SIZE;
        int pushInterval = DEFAULT_PUSH_INTERVAL;
        int httpRetries = DEFAULT_HTTP_RETRIES;
        int httpTimeout = DEFAULT_HTTP_TIMEOUT;
        int httpRetryDelay = DEFAULT_HTTP_RETRY_DELAY;
        enum protocol protocol = HTTP;
        string influxHost;
        int influxPort;
        string influxOrg;
        string influxBucket;
        string authToken;
        WiFiClient client;
        vector<Datapoint> metricsBuffer = {};
        int httpRequestsMade;
        int maxHTTPRequestsPerConnection = 100;
        int previousFlushTime;
        bool waitingForResponse = false;
        int responseSendTime = 0;
        int retriesCount = 0;
        IPAddress sensorAddress;
        string sensorGroup;

        enum monitoringStatus monitoringStatus = OK;
        uint16_t statusByte = DEFAULT_STATUS_BYTE;
        int warningToAlert = DEFAULT_OK_BOUND;

        unordered_map<string, uint8_t> metricsMap;
        unordered_map<uint8_t, string> valuesMap;
};