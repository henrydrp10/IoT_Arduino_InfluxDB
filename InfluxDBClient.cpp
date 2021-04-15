#define _GLIBCXX_USE_C99 1

#include <InfluxDBClient.h>
#include "Arduino.h"
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <WiFi.h>
#include <SPI.h>
#include <EEPROM.h>
using namespace std;

WiFiServer wifiServer(DEFAULT_WIFI_SERVER_PORT);

void writeIntToDisk(int address, int value);
void writeUint8ToDisk(int address, uint8_t value);
void writeUint16ToDisk(int address, uint16_t value);
uint16_t readUint16FromDisk(int address);

KeyValue::KeyValue(uint8_t key, string value)
{
    this->key = key;
    this->value = value;
}

uint8_t KeyValue::getKey()
{
    return this->key;
}

string KeyValue::getValue()
{
    return this->value;
}

Datapoint::Datapoint(uint8_t metric, vector<KeyValue> values)
{
    this->metric = metric;
    this->values = values;
}

uint8_t Datapoint::getMetric()
{
    return this->metric;
}

vector<KeyValue> Datapoint::getValues()
{
    return this->values;
}

void Datapoint::addValue(KeyValue value)
{
    this->values.push_back(value);
}

string InfluxDBClient::keyValueToString(KeyValue kv)
{
    string str = this->valuesMap.at(kv.getKey()) + "=" + kv.getValue();
    return str.c_str();
}

string InfluxDBClient::datapointToString(Datapoint dp)
{
    vector<KeyValue> dpTags;
    dpTags.push_back(KeyValue(this->metricsMap.at("sensor_group"), this->sensorGroup));
    dpTags.push_back(KeyValue(this->metricsMap.at("sensor_address"), this->sensorAddress.toString().c_str()));

    uint8_t metric = dp.getMetric();
    string str = this->valuesMap.at(metric);

    for (int i = 0; i < dpTags.size(); i++)
        str += "," + this->keyValueToString(dpTags.at(i));

    str += " ";

    for (int i = 0; i < dp.getValues().size(); i++)
    {
        if (i > 0)
            str += ",";

        str += this->keyValueToString(dp.getValues().at(i));
    }

    return str.c_str();
}

InfluxDBClient::InfluxDBClient() {}

InfluxDBClient::InfluxDBClient(string influxHost, int influxPort, string influxOrg, string influxBucket, string authToken, string sensorGroup)
{
    this->influxHost = influxHost;
    this->influxPort = influxPort;
    this->influxOrg = influxOrg;
    this->influxBucket = influxBucket;
    this->previousFlushTime = millis();
    this->authToken = authToken;
    this->sensorAddress = WiFi.localIP();
    this->sensorGroup = sensorGroup;
    wifiServer.begin();
    EEPROM.begin(EEPROM_SIZE);
    this->initMap();
    this->loadSensorConfig();
}

InfluxDBClient::InfluxDBClient(string influxHost, int influxPort, string influxOrg, string influxBucket, string authToken, enum protocol protocol,
                               int pushInterval, int bufferSize, int httpRetries, int httpTimeout, int httpRetryDelay, string sensorGroup)
{
    this->influxHost = influxHost;
    this->influxPort = influxPort;
    this->influxOrg = influxOrg;
    this->influxBucket = influxBucket;
    this->authToken = authToken;
    this->protocol = protocol;
    this->pushInterval = pushInterval;
    this->bufferSize = bufferSize;
    this->httpRetries = httpRetries;
    this->httpTimeout = httpTimeout;
    this->httpRetryDelay = httpRetryDelay;
    this->previousFlushTime = millis();
    this->sensorAddress = WiFi.localIP();
    this->sensorGroup = sensorGroup;
    wifiServer.begin();
    EEPROM.begin(EEPROM_SIZE);
    this->initMap();
    this->loadSensorConfig();
}

void InfluxDBClient::initMap()
{
    this->metricsMap.insert({"influx_host", 0});
    this->metricsMap.insert({"influx_port", 1});
    this->metricsMap.insert({"influx_org", 2});
    this->metricsMap.insert({"influx_bucket", 3});
    this->metricsMap.insert({"sensor_group", 4});
    this->metricsMap.insert({"sensor_address", 5});
    this->metricsMap.insert({"sensor_config", 6});
    this->metricsMap.insert({"push_interval", 7});
    this->metricsMap.insert({"mean_value", 8});
    this->metricsMap.insert({"min_value", 9});
    this->metricsMap.insert({"max_value", 10});

    this->valuesMap.insert({0, "influx_host"});
    this->valuesMap.insert({1, "influx_port"});
    this->valuesMap.insert({2, "influx_org"});
    this->valuesMap.insert({3, "influx_bucket"});
    this->valuesMap.insert({4, "sensor_group"});
    this->valuesMap.insert({5, "sensor_address"});
    this->valuesMap.insert({6, "sensor_config"});
    this->valuesMap.insert({7, "push_interval"});
    this->valuesMap.insert({8, "mean_value"});
    this->valuesMap.insert({9, "min_value"});
    this->valuesMap.insert({10, "max_value"});
}

void InfluxDBClient::addMetricName(string metric)
{
    this->metricsMap.insert({metric, this->metricsMap.size()});
    this->valuesMap.insert({this->valuesMap.size(), metric});
}

void InfluxDBClient::addDatapoint(Datapoint dp)
{
    if (metricsBuffer.size() >= this->bufferSize)
    {
        metricsBuffer.erase(metricsBuffer.begin());
    }
    metricsBuffer.push_back(dp);
}


bool InfluxDBClient::createMetric(string metric, vector<float> &valVect, vector<KeyValue> &values, std::ostringstream &tempString, float lowerThreshold, float upperThreshold)
{
    if (this->metricsMap.count(metric) == 0)
    {
        this->addMetricName(metric);
    }

    float minValue = valVect.at(0);
    float maxValue = valVect.at(0);
    float sum = 0;

    for (float value : valVect)
    {
        if (value < minValue)
        {
            minValue = value;
        }
        else if (value > maxValue)
        {
            maxValue = value;
        }

        sum += value;

    }

    float mean = sum / valVect.size();

    tempString << minValue;
    values.push_back(KeyValue(this->metricsMap.at("min_value"), tempString.str().c_str()));
    tempString.str("");

    tempString << mean;
    values.push_back(KeyValue(this->metricsMap.at("mean_value"), tempString.str().c_str()));
    tempString.str("");

    tempString << maxValue;
    values.push_back(KeyValue(this->metricsMap.at("max_value"), tempString.str().c_str()));
    tempString.str("");

    this->addDatapoint(Datapoint(this->metricsMap.at(metric), values));
    
    values.clear();

    // Clear previous statusByte

    for (size_t i = 0; i < 16; i++)
    {
        this->statusByte &= ~(1u << i);
    }

    // Metric value

    uint8_t metricBin = this->metricsMap.at(metric);
    this->statusByte = ((uint16_t)metricBin << 8) | this->statusByte;

    // Bit 3 -> if value is lower than the lowerThreshold set up
    // Bit 4 -> if value is higher than the higherThreshold set up
    // Bit 5 -> if last metric values are OK

    if (minValue < lowerThreshold)
    {
        this->statusByte |= 1u << 3;        // <
        setStatusByte(this->statusByte);
        return false;
    }
    if (maxValue > upperThreshold)
    {
        this->statusByte |= 1u << 4;        // >
        setStatusByte(this->statusByte);
        return false;
    }
    if ((minValue >= lowerThreshold) && (maxValue <= upperThreshold))
    {
        this->statusByte |= 1u << 5;        // lastMetricOK to 1
        setStatusByte(this->statusByte);
        return true;
    }

}

void InfluxDBClient::checkMonitoringLevels(bool metricsOk, int &readsPerMetric)
{
    // Clear previous statusByte
    for (size_t i = 0; i < 16; i++)
    {
        this->statusByte &= ~(1u << i);
    }

    switch (this->monitoringStatus)
    {
        case OK_:

            if (!metricsOk)
            {
                uint16_t newStatusByte = this->statusByte;
                newStatusByte |= 1u << 1;
                setStatusByte(newStatusByte); // OK -> WARNING

                this->monitoringStatus = WARNING;
                setPushInterval(pushInterval / 2);
                readsPerMetric /= 2;

                this->warningToAlert = 2;
            }
            else
            {
                uint16_t newStatusByte = this->statusByte;
                newStatusByte |= 1u << 0;
                setStatusByte(newStatusByte); // OK
            }
            break;

        case WARNING:

            if (metricsOk)
            {
                this->warningToAlert--;
                if (this->warningToAlert <= DEFAULT_OK_BOUND)
                {
                    uint16_t newStatusByte = this->statusByte;
                    newStatusByte |= 1u << 0;
                    setStatusByte(newStatusByte); // WARNING -> OK

                    this->monitoringStatus = OK_;
                    setPushInterval(pushInterval * 2);
                    readsPerMetric *= 2;
                }
                else
                {
                    uint16_t newStatusByte = this->statusByte;
                    newStatusByte |= 1u << 1;
                    setStatusByte(newStatusByte); // WARNING
                }
            }
            else
            {
                this->warningToAlert++;
                if (this->warningToAlert >= DEFAULT_ALERT_BOUND)
                {
                    uint16_t newStatusByte = this->statusByte;
                    newStatusByte |= 1u << 2;
                    setStatusByte(newStatusByte); // WARNING -> ALERT

                    this->monitoringStatus = ALERT;
                    setPushInterval(pushInterval / 2);
                    readsPerMetric /= 2;
                }
                else
                {
                    uint16_t newStatusByte = this->statusByte;
                    newStatusByte |= 1u << 1;
                    setStatusByte(newStatusByte); // WARNING
                }
            }
            break;

        case ALERT:

            if (metricsOk)
            {
                this->warningToAlert--;
                if (this->warningToAlert < DEFAULT_ALERT_BOUND)
                {
                    uint16_t newStatusByte = this->statusByte;
                    newStatusByte |= 1u << 1;
                    setStatusByte(newStatusByte);   // ALERT -> WARNING

                    this->monitoringStatus = WARNING;
                    setPushInterval(pushInterval * 2);
                    readsPerMetric *= 2;
                }
                else
                {
                    uint16_t newStatusByte = this->statusByte;
                    newStatusByte |= 1u << 2;
                    setStatusByte(newStatusByte);   // ALERT
                }
            }
            else
            {
                this->warningToAlert++;
                uint16_t newStatusByte = this->statusByte;
                newStatusByte |= 1u << 2;
                setStatusByte(newStatusByte);   // ALERT
            }

            break;
    }

    Serial.print("Reads per metric: ");
    Serial.println(readsPerMetric);
    Serial.print("Monitoring status: ");
    Serial.println(this->monitoringStatus);
    Serial.print("WarningToAlert: ");
    Serial.println(this->warningToAlert);

}

enum monitoringStatus InfluxDBClient::getMonitoringStatus()
{
    return this->monitoringStatus;
}

void InfluxDBClient::tick()
{
    Serial.println("Buffer:");
    for (int i = 0; i < this->metricsBuffer.size(); i++)
    {
        Datapoint dp = this->metricsBuffer.at(i);
        string str = datapointToString(dp);
        Serial.printf("%s\n", str.c_str());
    }

    WiFiClient client = wifiServer.available();
    if (client)
        this->handleHTTPRequest(client);

    if (this->waitingForResponse && this->client.available())
    {
        if (this->handleResponse())
        {
            this->metricsBuffer.clear();
            this->previousFlushTime = millis();
            this->retriesCount = 0;
        }
        else
        {
            this->retriesCount++;
            this->waitingForResponse = false;
            pushMetrics(this->metricsBuffer);
        }
    }
    else if (millis() - this->responseSendTime > this->httpTimeout)
    {
        this->waitingForResponse = false;
        if (this->retriesCount < this->httpRetries)
        {
            this->retriesCount++;
            this->waitingForResponse = false;
            pushMetrics(this->metricsBuffer);
        }
    }

    if (millis() - this->previousFlushTime > this->pushInterval)
    {
        pushMetrics(this->metricsBuffer);
    }
}

uint16_t InfluxDBClient::getStatusByte()
{
    return this->statusByte;
}

void InfluxDBClient::setStatusByte(uint16_t newStatusByte)
{
    this->statusByte = newStatusByte;
    writeUint16ToDisk(EEPROM_STATUS_BYTE_ARRAY_OFFSET + (int) this->statusByteIdxPointer, this->statusByte);

    this->statusByteIdxPointer += 2;
    if (this->statusByteIdxPointer >= (EEPROM_SIZE - EEPROM_STATUS_BYTE_ARRAY_OFFSET - 1))
        this->statusByteIdxPointer = 0;
    writeUint8ToDisk(EEPROM_STATUS_BYTE_ARRAY_INDEX_OFFSET, this->statusByteIdxPointer);

    EEPROM.commit();
}

uint8_t InfluxDBClient::getStatusByteIdxPointer()
{
    return this->statusByteIdxPointer;
}

int InfluxDBClient::getPushInterval()
{
    return this->pushInterval;
}

void InfluxDBClient::setPushInterval(int newPushInterval)
{
    this->pushInterval = newPushInterval;
    writeIntToDisk(EEPROM_PUSH_INTERVAL_OFFSET, this->pushInterval);

    // TODO: call this->publishSensorConfig() instead.

    ostringstream configStr;
    vector<KeyValue> configValues;

    configStr << newPushInterval;

    KeyValue kv = KeyValue(this->metricsMap.at("push_interval"), configStr.str().c_str());
    configValues.push_back(kv);

    Datapoint dp = Datapoint(this->metricsMap.at("sensor_config"), configValues);
    this->addDatapoint(dp);

    EEPROM.commit();
}

void send204(WiFiClient client)
{
    client.println(HTTP_HEADER_RESPONSE_204);
    client.println(HTTP_HEADER_CONNECTION_CLOSE);
    client.println();
}

void send400(WiFiClient client, string message)
{
    client.println(HTTP_HEADER_RESPONSE_400);
    client.println(HTTP_HEADER_CONNECTION_CLOSE);
    client.println();
    client.println(message.c_str());
}

void send404(WiFiClient client)
{
    client.println(HTTP_HEADER_RESPONSE_404);
    client.println(HTTP_HEADER_CONNECTION_CLOSE);
    client.println();
}

void InfluxDBClient::handleHTTPRequest(WiFiClient client)
{
    if (!client.available())
        return;

    string request;
    char c = client.read();
    int length = 0;
    while (client.available())
    {
        request.push_back(c);
        length++;
        if (length > HTTP_REQUEST_MAX_SIZE)
        {
            send400(client, "Request is too long");
            return;
        }
        c = client.read();
    }
    request.push_back(c);

    if (request.find(SENSOR_STATUS_FETCH_ENDPOINT) == 0)
    {
        this->sendSensorStatus(client);
    }
    else if (request.find(SENSOR_STATUS_MODIFY_ENDPOINT) == 0)
    {
        this->modifySensorStatus(client, request);
    }
    else
    {
        send404(client);
    }
}

void InfluxDBClient::sendSensorStatus(WiFiClient client)
{
    std::ostringstream status;
    status << "{\"pushInterval\":" << this->pushInterval << ",\"influxHost\":\"" << this->influxHost
           << "\",\"influxPort\":" << this->influxPort << ",\"influxOrg\":\"" << this->influxOrg
           << "\",\"influxBucket\":\"" << this->influxBucket << "\",\"sensorGroup\":\"" << this->sensorGroup << "\"}";

    client.println(HTTP_HEADER_RESPONSE_200);
    client.println(HTTP_HEADER_CONTENT_TYPE_JSON);
    client.println(HTTP_HEADER_CONNECTION_CLOSE);
    std::ostringstream contentLength;
    contentLength << "Content-Length: " << status.str().length();
    client.println(contentLength.str().c_str());
    client.println();
    client.println(status.str().c_str());
}

string extractValueFromRequest(string request, string key)
{
    int position = request.find(key + "=");
    if (position == std::string::npos)
    {
        return "";
    }

    int valueStart = request.find("=", position) + 1;
    int valueEnd = request.find("&", valueStart);
    if (valueEnd == std::string::npos)
    {
        valueEnd = request.length();
    }

    return request.substr(valueStart, valueEnd - valueStart);
}

void InfluxDBClient::modifySensorStatus(WiFiClient client, string request)
{
    string value = extractValueFromRequest(request, "pushInterval");
    if (value.length() > 0)
    {
        try
        {
            this->pushInterval = std::stoi(value);
        }
        catch (const std::exception &e)
        {
            send400(client, "Invalid value provided for pushInterval");
            return;
        }
    }

    value = extractValueFromRequest(request, "influxHost");
    if (value.length() > 0)
    {
        this->influxHost = value;
    }

    value = extractValueFromRequest(request, "influxPort");
    if (value.length() > 0)
    {
        try
        {
            this->influxPort = std::stoi(value);
        }
        catch (const std::exception &e)
        {
            send400(client, "Invalid value provided for influxPort");
            return;
        }
    }

    value = extractValueFromRequest(request, "influxOrg");
    if (value.length() > 0)
    {
        this->influxOrg = value;
    }

    value = extractValueFromRequest(request, "influxBucket");
    if (value.length() > 0)
    {
        this->influxBucket = value;
    }

    value = extractValueFromRequest(request, "authToken");
    if (value.length() > 0)
    {
        this->authToken = value;
    }

    value = extractValueFromRequest(request, "sensorGroup");
    if (value.length() > 0)
    {
        this->sensorGroup = value;
    }

    this->saveSensorConfig();

    send204(client);
}

bool InfluxDBClient::pushMetrics(vector<Datapoint> metrics)
{
    switch (this->protocol)
    {
    case HTTP:
        return pushMetricsHTTP(metrics);
    default:
        return true;
    }
}

bool InfluxDBClient::pushMetricsHTTP(vector<Datapoint> metrics)
{
    if (this->waitingForResponse)
    {
        return true;
    }

    if (!this->checkHTTPConnection())
    {
        return false;
    }

    std::ostringstream ss;
    ss << this->influxPort;

    string body = makeHTTPRequestBody(metrics);
    this->client.flush();
    string line = "POST /api/v2/write?org=" + this->influxOrg + "&bucket=" + this->influxBucket + " HTTP/1.1";
    this->client.println(line.c_str());
    line = "Host: " + this->influxHost + ":" + ss.str();
    this->client.println(line.c_str());
    this->client.println("Connection: keep-alive");
    this->client.println("Content-Type: text/plain");
    std::ostringstream ss1;
    ss1 << body.length();
    line = "Content-Length: " + ss1.str();
    this->client.println(line.c_str());
    line = "Authorization: Token " + this->authToken;
    this->client.println(line.c_str());
    this->client.println();
    this->client.println(body.c_str());
    this->httpRequestsMade++;

    this->waitingForResponse = true;
    this->responseSendTime = millis();
    return true;
}

bool InfluxDBClient::handleResponse()
{
    int responseCode = getResponseCode();
    this->client.flush();
    if (is2xxResponseCode(responseCode))
    {
        return true;
    }
    // TODO: add error logs
    return false;
}

bool InfluxDBClient::waitForResponse()
{
    int startTime = millis();
    while (!this->client.available())
    {
        if (millis() - startTime > httpTimeout)
        {
            return false;
        }
    }
    return true;
}

int InfluxDBClient::getResponseCode()
{
    string responseLine;
    while (this->client.available())
    {
        char c = this->client.read();
        if (c == '\n')
        {
            size_t isResponseCodeLine = responseLine.find("HTTP/1.1");
            if (responseLine.length() > 12 && isResponseCodeLine != string::npos)
            {
                int code = 0;
                code += responseLine[9] - 48;
                code *= 10;
                code += responseLine[10] - 48;
                code *= 10;
                code += responseLine[11] - 48;
                return code;
            }
            else
            {
                responseLine = "";
            }
            break;
        }
        else
        {
            responseLine += c;
        }
    }
    return 0;
}

bool InfluxDBClient::is2xxResponseCode(int code)
{
    return code / 200 == 1;
}

string InfluxDBClient::makeHTTPRequestBody(vector<Datapoint> metrics)
{
    string body = "";
    for (int i = 0; i < metrics.size(); i++)
    {
        body += this->datapointToString(metrics.at(i)) + "\n";
    }
    return body;
}

bool InfluxDBClient::checkHTTPConnection()
{
    if (this->httpRequestsMade >= this->maxHTTPRequestsPerConnection || !client.connected())
    {
        this->httpRequestsMade = 0;
        return getHTTPConnection();
    }
    return true;
}

bool InfluxDBClient::getHTTPConnection()
{
    this->client.stop();
    if (!this->client.connect(this->influxHost.c_str(), this->influxPort))
    {
        // TODO: add error log
        return false;
    }
    return true;
}

void writeIntToDisk(int address, int value)
{
    EEPROM.put(address, value);
}

void writeUint8ToDisk(int address, uint8_t value)
{
    EEPROM.put(address, value);
}

void writeUint16ToDisk(int address, uint16_t value)
{
    EEPROM.put(address, value);
}

void writeStringToDisk(int address, int addressUpperBound, string value)
{
    for(char const c: value)
    {
        EEPROM.write(address, c);
        address++;
        if (address >= addressUpperBound) {
            return;
        }
    }
    EEPROM.write(address, '\0');
}

int readIntFromDisk(int address)
{
    int value;
    EEPROM.get(address, value);
    return value;
}

uint8_t readUint8FromDisk(int address)
{
    uint8_t value;
    EEPROM.get(address, value);
    return value;
}

uint16_t readUint16FromDisk(int address)
{
    uint16_t value;
    EEPROM.get(address, value);
    return value;
}

string readStringFromDisk(int address, int addressUpperBound)
{
    std::ostringstream value;

    char c;
    EEPROM.get(address, c);
    while(c != '\0')
    {
        if (address >= addressUpperBound) {
            return "";
        }
        value << c;
        address++;
        EEPROM.get(address, c);
    }

    return value.str();
}

void loadStringIfExists(int address, int addressUpperBound, string *value) {
    string readValue = readStringFromDisk(address, addressUpperBound);
    if (readValue != "") {
        *value = readValue;
    }
}

void loadIntIfExists(int address, int *value)
{
    int readValue = readIntFromDisk(address);
    if (readValue != -1)
    {
        *value = readValue;
    }

}

void loadUint8IfExists(int address, uint8_t *value)
{
    uint8_t readValue = readUint8FromDisk(address);
    if (readValue != 255)
    {
        *value = readValue;
    }

}

// TODO: Check why it is not parsed correctly.
void InfluxDBClient::publishSensorConfig()
{
    vector<KeyValue> values;

    values.push_back(KeyValue(this->metricsMap.at("influx_host"), this->influxHost));
    values.push_back(KeyValue(this->metricsMap.at("influx_bucket"), this->influxBucket));
    values.push_back(KeyValue(this->metricsMap.at("influx_org"), this->influxOrg));

    std::ostringstream port;
    port << this->influxPort;
    values.push_back(KeyValue(this->metricsMap.at("influx_port"), port.str()));

    std::ostringstream pushInterval;
    pushInterval << this->pushInterval;
    values.push_back(KeyValue(this->metricsMap.at("push_interval"), pushInterval.str()));

    this->addDatapoint(Datapoint(this->metricsMap.at("sensor_config"), values));
}

void InfluxDBClient::loadSensorConfig()
{
    loadStringIfExists(EEPROM_INFLUX_HOST_OFFSET, EEPROM_INFLUX_PORT_OFFSET, &this->influxHost);
    loadStringIfExists(EEPROM_INFLUX_ORG_OFFSET, EEPROM_INFLUX_BUCKET_OFFSET, &this->influxOrg);
    loadStringIfExists(EEPROM_INFLUX_BUCKET_OFFSET, EEPROM_INFLUX_AUTH_TOKEN_OFFSET, &this->influxBucket);
    loadStringIfExists(EEPROM_INFLUX_AUTH_TOKEN_OFFSET, EEPROM_SENSOR_GROUP_OFFSET, &this->authToken);
    loadStringIfExists(EEPROM_SENSOR_GROUP_OFFSET, EEPROM_PUSH_INTERVAL_OFFSET, &this->sensorGroup);
    loadIntIfExists(EEPROM_PUSH_INTERVAL_OFFSET, &this->pushInterval);
    loadUint8IfExists(EEPROM_STATUS_BYTE_ARRAY_INDEX_OFFSET, &this->statusByteIdxPointer);

    int port = readIntFromDisk(EEPROM_INFLUX_PORT_OFFSET);
    if (port >= 0) {
        this->influxPort = port;
    }

     // TODO: call this->publishSensorConfig() here.
}

void InfluxDBClient::saveSensorConfig()
{
    writeStringToDisk(EEPROM_INFLUX_HOST_OFFSET, EEPROM_INFLUX_PORT_OFFSET, this->influxHost);
    writeIntToDisk(EEPROM_INFLUX_PORT_OFFSET, this->influxPort);
    writeStringToDisk(EEPROM_INFLUX_ORG_OFFSET, EEPROM_INFLUX_BUCKET_OFFSET, this->influxOrg);
    writeStringToDisk(EEPROM_INFLUX_BUCKET_OFFSET, EEPROM_INFLUX_AUTH_TOKEN_OFFSET, this->influxBucket);
    writeStringToDisk(EEPROM_INFLUX_AUTH_TOKEN_OFFSET, EEPROM_SENSOR_GROUP_OFFSET, this->authToken);
    writeStringToDisk(EEPROM_SENSOR_GROUP_OFFSET, EEPROM_PUSH_INTERVAL_OFFSET, this->sensorGroup);
    writeIntToDisk(EEPROM_PUSH_INTERVAL_OFFSET, this->pushInterval);
    EEPROM.write(EEPROM_STATUS_BYTE_ARRAY_INDEX_OFFSET, this->statusByteIdxPointer);

    EEPROM.commit();
    Serial.println("Saved config");

    // TODO: call this->publishSensorConfig() here.
}
