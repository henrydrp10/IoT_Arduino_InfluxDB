// Libraries to include
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h> 

// Declare and initialise the sensor
Adafruit_BME280 bme;

status = bme.begin(0x76);  
if (!status) {
    Serial.println("Check wiring!");
    while (1);
}

// Read the values and put them into float variables
float temp = bme.readTemperature();
float hum = bme.readHumidity();
float bar_p = bme.readPressure() / 100.0F;


