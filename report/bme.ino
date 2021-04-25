#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h> 

Adafruit_BME280 bme;

void setup 
{
	Serial.begin(9600);

	status = bme.begin(0x76);  
	if (!status) {
    		Serial.println("Could not find a valid BME280 sensor, check wiring!");
    		while (1);
	}

	float temp = bme.readTemperature();
	Serial.println(temp);
}

void loop
{

}

