#include "Arduino.h"
#include <EEPROM.h>
#include <vector>
#include <string>
#include <sstream>
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

    Serial.begin(9600);

    Serial.printf("%d\n", EEPROM.length());

    string influxHost = readStringFromDisk(EEPROM_INFLUX_HOST_OFFSET, EEPROM_INFLUX_PORT_OFFSET);
    int influxPort = readIntFromDisk(EEPROM_INFLUX_PORT_OFFSET);
    string influxOrg = readStringFromDisk(EEPROM_INFLUX_ORG_OFFSET, EEPROM_INFLUX_BUCKET_OFFSET);
    string influxBucket = readStringFromDisk(EEPROM_INFLUX_BUCKET_OFFSET, EEPROM_INFLUX_AUTH_TOKEN_OFFSET);
    string sensorGroup = readStringFromDisk(EEPROM_SENSOR_GROUP_OFFSET, EEPROM_PUSH_INTERVAL_OFFSET);
    uint8_t idxPointer = readUint8FromDisk(EEPROM_STATUS_BYTE_ARRAY_INDEX_OFFSET);

    delay(5000);
    Serial.println("Starting Maintenance Interrogation Program:\n");
    delay(1000);

    Serial.println("Influx details:");
    Serial.printf("INFLUX HOST: %s\n", influxHost.c_str());
    Serial.printf("INFLUX PORT: %d\n", influxPort);
    Serial.printf("INFLUX ORG: %s\n", influxOrg.c_str());
    Serial.printf("INFLUX BUCKET: %s\n", influxBucket.c_str());
    Serial.printf("SENSOR GROUP: %s\n", influxHost.c_str());
    Serial.println();

    vector<uint16_t> statusByteVector;

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

    Serial.println("Last statusByte readings:");
    for (uint16_t statusByte : statusByteVector)
    {
        Serial.println(statusByte);
    }


}

void loop() {

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
