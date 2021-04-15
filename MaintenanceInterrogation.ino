#include "Arduino.h"
#include <EEPROM.h>
#include <vector>
#include <string>
#include <sstream>
using namespace std;

#define EEPROM_STATUS_BYTE_ARRAY_INDEX_OFFSET 429
#define EEPROM_STATUS_BYTE_ARRAY_OFFSET 430
#define EEPROM_SIZE 510

void setup() {

    Serial.begin(9600);
    EEPROM.begin(EEPROM_SIZE);

    delay(5000);
    Serial.println("Starting Maintenance Interrogation Program:\n");
    delay(1000);

    vector<uint16_t> statusByteVector;
    uint8_t idxPointer = readUint8FromDisk(EEPROM_STATUS_BYTE_ARRAY_INDEX_OFFSET);

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
        Serial.println(toBinary(statusByte).c_str());
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
