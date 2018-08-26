/**
 * Display:
 *
 * Following hardware connections are assumed
 *
 *   OLED                   Arduino 101
 *   ---------------------+------------------------------------------------------------------
 *   #1 Vss/GND             GND
 *   #2 Vdd                 3V3 (up to 271 mA, use external power supply to feed Arduino 101)
 *   #4 D/!C                D9
 *   #7 SCLK                D13 (hardware SPI SCLK)
 *   #8 SDIN                D11 (hardware SPI MOSI)
 *   #16 !RESET             !RESET
 *   #17 !CS                D10
 *   #5,#6,#10-14,#19,#20   GND
 *   #3,#9,#15,#18          not connected
 *
 * Based on Adafruit SSD1306 driver (https://github.com/adafruit/Adafruit_SSD1306)
 */

#include <SPI.h>
#include <SD.h>
#include <ESP8266_SSD1322.h>
#include <Bounce2.h>

//ESP8266 Pins
#define OLED_CS     15  // D8, CS - Chip select
#define OLED_DC     2   // D4 - DC digital signal
#define OLED_RESET  16  // D0 -RESET digital signal

//hardware SPI - only way to go. Can get 110 FPS
ESP8266_SSD1322 display(OLED_DC, OLED_RESET, OLED_CS);

#if (SSD1322_LCDHEIGHT != 64)
#error ("Height incorrect, please fix ESP8266_SSD1322.h!");
#endif

#define BUTTON_PIN 4 //GPIO 4 D2
Bounce debouncer = Bounce();
int lastButtonState = 1;
//SD Card Variables
#define SD_CS 0 // D3
int cardWorking = 0;
int loggingEnabled = 0;

//Serial Variables
char inBuff[5] = {};
struct Data {
        float coolantTemp = 0;
        float tps = 0;
        float lambda = 0;
        float oilPressure = 0;
        float oilTemp = 0;
        int motorRPM = 0;
        long lastUpdated = 0;
} data;

long loopTimer = 0;
int halfLoopTime = 50;
int loopCount = 0;
long updateTimer = 0;
int updateCount = 0;
int sdFound = 0;
File dataFile;

void setup()   {
        pinMode(BUTTON_PIN,INPUT_PULLUP);
        debouncer.attach(BUTTON_PIN);
        debouncer.interval(5); // interval in ms

        pinMode(OLED_CS, OUTPUT);
        pinMode(SD_CS, OUTPUT);
        pinMode(14, OUTPUT);
        pinMode(13, OUTPUT);
        pinMode(16, OUTPUT);
        pinMode(12, INPUT);
        Serial.begin(115200);
        Serial.println("Serial Initialised");
        sdSetup();
        displaySetup();
        loopTimer = millis();
        updateTimer = millis();

}


void loop() {

        receiveSerial(); //check for new data every loop

        debouncer.update();
        int tempState = debouncer.read();
        if(tempState != lastButtonState) {
                lastButtonState = tempState;
                Serial.println("Button Pressed");
                if(!tempState) {
                        Serial.println("Toggle Logging");
                        loggingEnabled = !loggingEnabled;
                }
        }


        if(millis() - loopTimer >= halfLoopTime) {
                loopTimer = millis();
                // triggers at double speed and runs display and data logging alternately to better divide CPU time
                if(loopCount == 0) { //Do display things
                        loopCount = 1;
                        updateCount += 1;
                        updateDisplay();
                        // generateData();
                }else{ // Do logging things
                        loopCount = 0;
                        if(loggingEnabled)
                                writeToSD();
                }
        }
}

void generateData(){
        if(data.motorRPM < 14000) {
                data.motorRPM += 100;
        }else{
                data.motorRPM = 0;
        }

        if(data.lambda < 18) {
                data.lambda += 0.12;
        }else{
                data.lambda = 10;
        }

        data.coolantTemp = 80;
        data.oilTemp = 120;
        data.oilPressure = 40;
}

void updateDisplay(){
        //Updates display with  latest data availa ble
        display.clearDisplay(); // inneficient - if this takes too much time we need to send differences, rather than clear and send whole display.

        //RPM
        char buff[10];
        // display.drawRect(2,7,128, 32, 0xFFFF);
        display.setTextColor(WHITE);
        int rpmTop = data.motorRPM/1000;
        int rpmBottom = data.motorRPM - rpmTop*1000;

        sprintf(buff, "%2i", rpmTop);
        display.drawRightString(buff,45,7,6);
        sprintf(buff, "%03i", rpmBottom);
        display.drawRightString(buff,95,7,4);
        display.drawRightString("RPM",95,27,2);


        // sprintf(buff, "%i", data.motorRPM);
        // display.drawRightString(buff,135,7,6);
        //LAMBDA
        sprintf(buff, "%2.2f",data.lambda);
        display.drawRightString(buff, 250, 7, 6);

        // Coolant temp
        if(data.coolantTemp > 95) {
                display.fillRect(2, 44,51, 15, WHITE);
                display.setTextColor(BLACK);
        }
        sprintf(buff, "%3i",(int)data.coolantTemp);
        display.drawCentreString("CT:", 2, 45, 2);
        display.drawRightString(buff, 50, 45, 2);


        // Oil Pressure
        display.setTextColor(WHITE);
        if(data.oilPressure <50 && data.motorRPM > 8000) {
                display.fillRect(70, 44,51, 15, WHITE);
                display.setTextColor(BLACK);
        }
        sprintf(buff, "%3i",(int)data.oilPressure);
        display.drawRightString("OP:", 90, 45, 2);
        display.drawRightString(buff, 121, 45, 2);


        // Oil Temp
        display.setTextColor(WHITE);
        if(data.oilTemp > 140) {
                display.fillRect(141, 44,51, 15, WHITE);
                display.setTextColor(BLACK);
        }

        sprintf(buff, "%3i",(int)data.oilTemp);
        display.drawRightString("OT:", 161, 45, 2);
        display.drawRightString(buff, 192, 45, 2);

        //Bars
        float rpmEnd = 0.0182 * data.motorRPM; // 256/maxRPM i.e 14000
        display.fillRect(0, 0, (int)rpmEnd, 5, WHITE);

        //1000 rpm
        int breaks = 0.0182*1000;

        for(int i = breaks; i < rpmEnd; i+=breaks)
        {
                display.drawLine(i, 0, i, 5, BLACK);
        }

        //Lambda x100 for resolution
        int lambda = ((float)data.lambda-10)*100;
        int midPoint = 300;
        int midPointPix = 0.4 * midPoint;
        int readingPix = 0.4 * lambda;

        if(readingPix < midPointPix) {
                display.fillRect(readingPix, 60,midPointPix-readingPix, 63, WHITE);
                for(int i = 0; i < midPointPix; i+= 20) {
                        if(i>readingPix)
                                display.drawLine(i, 60, i, 63, BLACK);
                }
        }else{
                display.fillRect(midPointPix, 60, readingPix-midPointPix, 63, WHITE);
                for(int i = 256; i > midPointPix; i-= 20) {
                        if(i<readingPix)
                                display.drawLine(i, 60, i, 63, BLACK);
                }
        }

        if(loggingEnabled) {
                if(sdFound) {
                        display.drawRightString("LOG", 250, 45, 2);
                }else{
                        display.drawRightString("ERR", 250, 45, 2);
                }
        }else{
                display.drawRightString("OFF", 250, 45, 2);
        }


        // Lines...
        // for(int i = 20; i < 256; i+=20)
        // {
        //         display.drawLine(i, 0, i, display.height()-1, WHITE);
        // }


        // long dispTime = millis();
        display.display();
        // data.motorRPM = millis() - dispTime;
}

void writeToSD(){
        if(sdFound) {
                // long sdTime = millis();
                dataFile = SD.open("log.csv", FILE_WRITE);
                if(dataFile) {
                        String stringBuff = String(millis()) +","+ String(data.motorRPM) +","+String(data.lambda)+","+String(data.tps)+","+String(data.coolantTemp)+","+String(data.oilTemp)+","+String(data.oilPressure);
                        dataFile.println(stringBuff);
                        // Serial.print("SD Write Time: ");
                        // Serial.println(millis() - sdTime);
                }else{
                        Serial.println("Failed to open log.csv");
                        sdFound = 0;
                }

                dataFile.close();
        }

}

void sdSetup(){
        // For SD Init to work on ESP8266 it is necesary to modify line 286 of
        // Sd2Card.cpp to settings = SPISettings(sckRateID, MSBFIRST, SPI_MODE0);

        Serial.println("Initializing SD card...");

        if(!SD.begin(SD_CS, SPI_FULL_SPEED)) {
                Serial.println("SD.Begin failed");
                // Serial.println(SD.errorCode());
                return;
        }
        Serial.println("card initialized.");
        if(SD.exists("log.csv")) {
                Serial.println("Logfile Found");
        }else{
                Serial.println("No Logfile found... Creating one");
                dataFile = SD.open("log.csv", FILE_WRITE);
                if(dataFile) {
                        Serial.println("Logfile Created");
                        dataFile.println("Timestamp,RPM,Lambda,TPS,Coolant Temp,Oil Temp, Oil Pressure");
                }
                dataFile.close();
        }
        sdFound = 1;
}

void receiveSerial(){
        if(Serial.available()) {
                buffPush(Serial.read());
                if(inBuff[4] == '}') { // Latest char is the end of a packet
                        if(inBuff[0] =='{') { // First char of packet is in expected location - we should have data between them
                                float value = dataDecode(inBuff[2],inBuff[3]);
                                assignValue(inBuff[1], value);
                                data.lastUpdated = millis();
                        }
                }
        }
}

void buffPush(char in){
        //hard coded for buffer length of 5
        for(int i = 0; i < 4; i++) { //leave last value untouched, hence -1
                //Shifts the array left
                inBuff[i] = inBuff[i+1];
        }
        //Add new value to end
        inBuff[4] = in;
}

float dataDecode(char b1, char b2){
        float val = 0;
        //First calculate the value in b1 and b2
        //Check for 0
        // Serial.print("b2, B2: ");
        // Serial.print(b1);
        // Serial.print(",");
        // Serial.println(b2);
        // char buff [50];
        // sprintf(buff, "1: %x , 2: %x", b1, b2);
        // Serial.println(buff);
        if( b1 == 0xFF && b2 == 0xFF) {
                val = 0;
        } else if (b1 > 127) { // check flag for integer
                if(b2 == 0xFF){
                val = (b1 - 128)*100;
              }else{
                val = (b1 - 128)*100 + b2;
              }
                // Serial.print("integer value decoded as: ");
                // Serial.println(val);
        } else{ //value is a float
                // THere is a permanent rounding down error in this of -0.01. +0.01 to compensate.
                if(b2 == 0xFF){
                  val = b1;
              }else{
                val = b1 + ((float)b2+1)/100;
              }
                // Serial.print("Float value decoded as: ");
                // Serial.println(val);
        }

        return val;
}

void assignValue(char id, float val){
        switch (id) {
        case 'v':
                data.coolantTemp = val;
                break;
        case 't':
                data.tps = val;
                break;
        case 'w':
                data.lambda = val;
                break;
        case 'i':
                data.oilPressure = val;
                break;
        case 'm':
                Serial.print("Recieved RPM: ");
                Serial.println(val);
                data.motorRPM = val*10;

                break;
        case 'o':
                data.oilTemp = val;
                break;
        default:
                break;
        }
}



void displaySetup(){
        // Initialize and perform reset
        display.begin(true);

        Serial.println("Initialising Display");


        // Clear the buffer.
        display.clearDisplay();

        display.setTextColor(WHITE);
        display.drawRightString("PARKES",20,20,4);

        display.display();
        delay(500);
        display.clearDisplay();
}
