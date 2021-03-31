#include "Arduino.h"
#include <EEPROM.h>
#include <vector>
using namespace std;

#define EEPROM_INFLUX_HOST_OFFSET 1
#define EEPROM_INFLUX_PORT_OFFSET 17
#define EEPROM_INFLUX_ORG_OFFSET 21
#define EEPROM_INFLUX_BUCKET_OFFSET 122
#define EEPROM_INFLUX_AUTH_TOKEN_OFFSET 223
#define EEPROM_SENSOR_GROUP_OFFSET 324
#define EEPROM_PUSH_INTERVAL_OFFSET 425
#define EEPROM_STATUS_BYTE_ARRAY_INDEX_OFFSET 429
#define EEPROM_STATUS_BYTE_ARRAY_OFFSET 430
#define EEPROM_SIZE 510

void setup() {

    Serial.begin(115200);

    string influxHost;
    int influxPort;
    string influxOrg;
    string influxBucket;
    string sensorGroup;

    uint8_t idxPointer;
    vector<uint16_t> statusByteVector;

    EEPROM.get(EEPROM_INFLUX_HOST_OFFSET, influxHost);
    EEPROM.get(EEPROM_INFLUX_PORT_OFFSET, influxPort);
    EEPROM.get(EEPROM_INFLUX_ORG_OFFSET, influxOrg);
    EEPROM.get(EEPROM_INFLUX_BUCKET_OFFSET, influxBucket);
    EEPROM.get(EEPROM_SENSOR_GROUP_OFFSET, sensorGroup);
    EEPROM.get(EEPROM_STATUS_BYTE_ARRAY_INDEX_OFFSET, idxPointer);

    delay(5000);
    Serial.println("Starting Maintenance Interrogation Program:");
    delay(1000);
    
    Serial.println("Influx details:");
    Serial.printf("INFLUX HOST: %s\n", influxHost);
    Serial.printf("INFLUX PORT: %d\n", influxPort);
    Serial.printf("INFLUX ORG: %s\n", influxOrg);
    Serial.printf("INFLUX BUCKET: %s\n", influxBucket);
    Serial.printf("SENSOR GROUP: %s\n", influxHost);
    Serial.println();

    for (int i = EEPROM_STATUS_BYTE_ARRAY_OFFSET + idxPointer; i < EEPROM_SIZE; i += 2)
    {
        uint16_t statusByte;
        EEPROM.get(i, statusByte);
        statusByteVector.push_back(statusByte);
    }

    for (int i = EEPROM_STATUS_BYTE_ARRAY_OFFSET; i < EEPROM_STATUS_BYTE_ARRAY_OFFSET + idxPointer; i += 2)
    {
        uint16_t statusByte;
        EEPROM.get(i, statusByte);
        statusByteVector.push_back(statusByte);
    }
    
    Serial.println("Last statusByte readings:")
    Serial.println();

    for (uint16_t statusByte : statusByteVector)
    {
        Serial.println(statusByte);
    }
    

}

void loop() {

}