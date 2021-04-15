#include "Arduino.h"
#include <EEPROM.h>
#include <vector>
#include <string>
#include <sstream>
using namespace std;

#define EEPROM_SIZE 510

void setup() {

    Serial.begin(9600);
    EEPROM.begin(EEPROM_SIZE);

    delay(5000);
    Serial.println("Resetting EEPROM\n");
    delay(1000);

    for (int i = 0; i < EEPROM_SIZE; i++)
    {
      uint8_t byte;
      EEPROM.put(i, 255);
    }

    EEPROM.commit();
    EEPROM.end();

    Serial.println("done");
}

void loop() {
  
}
