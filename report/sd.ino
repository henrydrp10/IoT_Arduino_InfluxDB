// Libraries to include
#include "FS.h"
#include "SD.h"

// Initialise the SD card
if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
}

// Append to the file log.txt
File logFile = SD.open("/log.txt", FILE_APPEND);
logFile.print("Hello World!");
logFile.close();