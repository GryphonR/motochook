/** ================================== */
/** Includes                           */
/** ================================== */

#include <math.h>
#include "Calibration.h"


/** ================================== */
/** BUILD OPTIONS                      */
/** ================================== */
const int DEBUG_MODE = 0; //if debug mode is on, no data will be sent via bluetooth. This is to make any debug messages easier to see.

/** ================================== */
/** CONSTANTS                          */
/** ================================== */
/** ___________________________________________________________________________________________________ ANALOG INPUT PINS */
const int COOL_TEMP_IN_PIN    = A0;    // Analog input pin for Coolant Temperature
const int THROTTLE_IN_PIN     = A3;    // Analog input pin for the throttle
const int LAMBDA_IN_PIN       = A7;    // Analog input pin for the lower battery voltage (battery between ground and 12V)
const int OIL_PRESSURE_IN_PIN = A2;    // Analog input pin for current draw
const int OIL_TEMP_IN_PIN     = A5;    // Analog input pin for temp sensor 1


/** ___________________________________________________________________________________________________ DIGITAL INTERRUPT PINS */
const int MOTOR_RPM_PIN       = 2;      // Digital interrupt for motor pulses


/** ________________________________________________________________________________________ CONSTANTS */
/* DATA TRANSMIT INTERVALS */
const unsigned long SHORT_DATA_TRANSMIT_INTERVAL     = 100;         // transmit interval in ms
const unsigned long MOTOR_CHECK_INTERVAL     = 10;         // transmit interval in ms


/** ___________________________________________________________________________________________________ DATA IDENTIFIERS */
/**
 *  If these are altered the data will no longer be read correctly by the phone.
 */
const char COOL_TEMP_ID       = 'v';
const char THROTTLE_INPUT_ID  = 't';
const char LAMBDA_ID          = 'w';
const char OIL_PRESSURE_ID    = 'i';
const char MOTOR_ID           = 'm';
const char OIL_TEMP_ID        = 'o';

/** ================================== */
/** VARIABLES                          */
/** ================================== */
/** ___________________________________________________________________________________________________ TIMING VARIABLES */
unsigned long lastShortDataSendTime       = 0;
unsigned long lastMotorCheckTime       = 0;
int loopCounter                 = 0;


/** ___________________________________________________________________________________________________ INTERRUPT VERIABLES */
/** Any variables that are being used in an Interrupt Service Routine need to be declared as volatile. This ensures
 *  that each time the variable is accessed it is the master copy in RAM rather than a cached version within the CPU.
 *  This way the main loop and the ISR variables are always in sync
 */
volatile unsigned int motorTime = 0;

//Sensor Filter Arrays
unsigned int rpmFilterArray[16];
uint8_t rpmFilterCount = 0;

unsigned int opFilterArray[16];
uint8_t opFilterCount = 0;

unsigned int ctFilterArray[32];
uint8_t ctFilterCount = 0;

unsigned int otFilterArray[32];
uint8_t otFilterCount = 0;

/** ___________________________________________________________________________________________________ Sensor Readings */

float coolantTemp           = 0;
float throttle              = 0;
float lambda                = 0;
float oilPressure           = 0;
float motorRPM              = 0;
float oilTemp               = 0;


/** ___________________________________________________________________________________________________ Smoothing Variables */

unsigned int calcConstant = 4; //microseconds per count

/** ================================== */
/** SETUP                                */
/** ================================== */
void setup()
{

        //Set up pin modes for all inputs and outputs
        pinMode(COOL_TEMP_IN_PIN,     INPUT);
        pinMode(OIL_PRESSURE_IN_PIN,        INPUT);
        pinMode(THROTTLE_IN_PIN,      INPUT);
        pinMode(OIL_PRESSURE_IN_PIN,          INPUT);
        pinMode(OIL_TEMP_IN_PIN,        INPUT);

        pinMode(8, INPUT);


        //Hardware counter for pulse Timestamp
        /*
           CS12 CS11 CS10
           0    0    0    no clock
           0    0    1    /1 - 16MHz
           0    1    0    /8 - 2MHz
           0    1    1    /64 - 256KHz
           1    0    0    /256
           1    0    1    /1024
           1    1    0    External clock, falling edge
           1    1    1    External clock, rising edge
         */
        // Set up hardware counter for RPM:
        // Setting count speed to 250KHz, and enabling input capture mode. This allows us to capture the number of counts at 250KHz between each RPM pulse.

        TCCR1A = 0;                     // this register set to 0!
        TCCR1B =_BV(CS11);
        TCCR1B |= _BV(CS10);            // 256khz
        TCCR1B |= _BV(ICES1);           // enable input capture

        TIMSK1 |= ((1<<ICIE1) | (1<<TOIE1));                         // enable input capture interrupt for timer 1 // enable overflow interrupt to detect missing input pulses

        Serial.begin(115200); // Bluetooth and USB communications

        lastShortDataSendTime = millis(); //Give the timing a start value.
        lastMotorCheckTime = millis();


} //End of Setup

void loop()
{
        // This addes the latest motor pulse time to an array
        if(millis() - lastMotorCheckTime >= MOTOR_CHECK_INTERVAL ) {
                rpmFilterArray[rpmFilterCount] = motorTime;
                rpmFilterCount = rpmFilterCount < 15 ? rpmFilterCount+1 : 0;
        }


        if (millis() - lastShortDataSendTime >= SHORT_DATA_TRANSMIT_INTERVAL)         //i.e. if 100ms have passed since this code last ran
        {
                lastShortDataSendTime = millis();         //this is reset at the start so that the calculation time does not add to the loop time
                loopCounter = loopCounter + 1;         // This value will loop 1-4, the 1s update variables will update on certain loops to spread the processing time.

                //It is recommended to leave the ADC a short recovery period between readings (~1ms). To ensure this we can transmit the data between readings
                motorRPM = readMotorRPM();
                sendData(MOTOR_ID, motorRPM);

                coolantTemp = readCoolantTemp();
                // coolantTemp = 0;
                sendData(COOL_TEMP_ID, coolantTemp);

                throttle = readThrottle();         // if this is being used as the input to a motor controller it is recommended to check it at a higher frequency than 4Hz
                sendData(THROTTLE_INPUT_ID, throttle);

                lambda = readLambda();
                // lambda = 15.82;
                sendData(LAMBDA_ID, lambda);

                // oilPressure = 120;
                oilPressure = readOilPressure();
                sendData(OIL_PRESSURE_ID, oilPressure);

                oilTemp = readOilTemp();
                sendData(OIL_TEMP_ID, oilTemp);

                //  for motoChook, all these bits do is blink LEDs to give a visual heartbeat
                if (loopCounter == 1)
                {
                        digitalWrite(13, HIGH);         //these are just flashing the LEDs as visual confimarion of the loop
                        digitalWrite(9, HIGH);
                }

                if (loopCounter == 2)
                {
                }

                if (loopCounter == 3)
                {         //nothing actaully needed to do at .75 seconds
                        digitalWrite(13, LOW);
                        digitalWrite(9, LOW);
                }

                if (loopCounter == 4)
                {
                        loopCounter = 0;         //4 * 0.25 makes one second, so counter resets
                }

        }


}

/**
 * Sensor Reading Functions
 *
 * Each function returns the value of the sensor it is reading
 */


float readCoolantTemp()
{
        float tempCoolantTemp = analogRead(COOL_TEMP_IN_PIN); //this will give a 10 bit value of the voltage with 1024 representing the ADC reference voltage of 5V

        if(tempCoolantTemp <= 290)
        {
                tempCoolantTemp = -38.29*log(tempCoolantTemp)+267.25; //Trendline formula from Excel
        }else{
                tempCoolantTemp = -0.1105*tempCoolantTemp + 81.596; //Trendline formula from Excel
        }

        ctFilterArray[ctFilterCount] = tempCoolantTemp;
        ctFilterCount = ctFilterCount < 31 ? ctFilterCount+1 : 0;

        unsigned int tempCount = 0;
        for(int i = 0; i < 32; i++) {
                tempCount += ctFilterArray[i];
        }

        tempCount = tempCount >> 5; //quick div by 32

        return (tempCount);
}

float readLambda()
{
        float tempLambda = analogRead(LAMBDA_IN_PIN); //this will give a 10 bit value of the voltage with 1024 representing the ADC reference voltage of 5V

        tempLambda = map((int)tempLambda,0,1024,1000,1800); //map to x100 for accuracy

        tempLambda = tempLambda/100; // div by 100 for float value

        return (tempLambda);
}

float readOilPressure()
{
        // Reading 0-5v from ADC. Comes through as 0-1024.
        float tempOP = analogRead(OIL_PRESSURE_IN_PIN);

        // tempOP = 5/1024 * actual voltage reading

        tempOP = map((int)tempOP, 102,921,0,1500)/10;

        opFilterArray[opFilterCount] = tempOP;
        opFilterCount = opFilterCount < 15 ? opFilterCount+1 : 0;

        unsigned int tempCount = 0;
        for(int i = 0; i < 16; i++) {
                tempCount += opFilterArray[i];
        }

        tempCount = tempCount >> 4; //quick div by 16

        return (tempCount);

}


float readThrottle() //This function is for a variable Throttle input
{
        float tempThrottle = analogRead(THROTTLE_IN_PIN);

        if(tempThrottle < CAL_THROTTLE_COUNT_MIN) {
                tempThrottle = CAL_THROTTLE_COUNT_MIN;
        }else if(tempThrottle > CAL_THROTTLE_COUNT_MAX) {
                tempThrottle = CAL_THROTTLE_COUNT_MAX;
        }

        tempThrottle = map((int)tempThrottle, CAL_THROTTLE_COUNT_MIN, CAL_THROTTLE_COUNT_MAX, 0, 1000)/10;

        return (tempThrottle);
}

float readOilTemp()
{
        float temp = analogRead(OIL_TEMP_IN_PIN); //use the thermistor function to turn the ADC reading into a temperature

        if(temp < 260) {
                temp = -43.47*log(temp)+341.42;
        }else{
                temp = -0.1233*temp+129.76;
        }

        otFilterArray[otFilterCount] = temp;
        otFilterCount = otFilterCount < 31 ? otFilterCount+1 : 0;

        unsigned int tempCount = 0;
        for(uint8_t i = 0; i < 32; i++) {
                tempCount += otFilterArray[i];
        }

        tempCount = tempCount >> 5; //quick div by 32

        return (tempCount);

}


float readMotorRPM()
{
        unsigned int tempRpm = 0;

        for(uint8_t i= 0; i<16; i++) {
                tempRpm += rpmFilterArray[i];
        }

        tempRpm = tempRpm >> 4; //Quick divide by 16 (2^4 = 16)

        tempRpm = 6000000/(calcConstant*tempRpm);

        return (tempRpm);
}


void sendData(char identifier, float value)
{
        if (!DEBUG_MODE) // Only runs if debug mode is LOW (0)
        {
                byte dataByte1;
                byte dataByte2;
                if (value == 0)
                {
                        // It is impossible to send null bytes over Serial connection
                        // so instead we define zero as 0xFF or 11111111 i.e. 255
                        dataByte1 = 0xFF;
                        dataByte2 = 0xFF;
                }
                else if (value <= 127)
                {
                        // Values under 128 are sent as a float
                        // i.e. value = dataByte1 + dataByte2 / 100
                        int integer;
                        int decimal;
                        float tempDecimal;
                        integer = (int) value;
                        tempDecimal = (value - (float) integer) * 100;
                        decimal = (int) tempDecimal;
                        dataByte1 = (byte) integer;
                        dataByte2 = (byte) decimal;
                        if (decimal == 0)
                                dataByte2 = 0xFF;
                        if (integer == 0)
                                dataByte1 = 0xFF;
                }
                else
                {
                        // Values above 127 are sent as integer
                        // i.e. value = dataByte1 * 100 + dataByte2
                        int tens;
                        int hundreds;
                        hundreds = (int)(value / 100);
                        tens = value - hundreds * 100;
                        dataByte1 = (byte)hundreds;
                        dataByte1 += 128;
                        dataByte2 = (byte) tens;
                        if (tens == 0)
                                dataByte2 = 0xFF;
                        if (hundreds == 0)
                                dataByte1 = 0xFF;
                }
                // Send the data in the format { [id] [1] [2] }
                Serial.write(123);
                Serial.write(identifier);
                Serial.write(dataByte1);
                Serial.write(dataByte2);
                Serial.write(125);

        }

}

/** override for integer values*/
void sendData(char identifier, int value)
{
        if (!DEBUG_MODE)
        {
                byte dataByte1;
                byte dataByte2;
                if (value == 0)
                {
                        dataByte1 = 0xFF;
                        dataByte2 = 0xFF;
                }
                else if (value <= 127)
                {
                        dataByte1 = (byte)value;
                        dataByte2 = 0xFF; //we know there's no decimal component as an int was passed in
                }
                else
                {
                        int tens;
                        int hundreds;
                        hundreds = (int)(value / 100);
                        tens = value - hundreds * 100;
                        dataByte1 = (byte)hundreds;
                        dataByte1 += 128; //sets MSB High to indicate Integer value
                        dataByte2 = (byte) tens;
                        if (tens == 0)
                                dataByte2 = 0xFF;
                        if (hundreds == 0)
                                dataByte1 = 0xFF;
                }
                Serial.write(123);
                Serial.write(identifier);
                Serial.write(dataByte1);
                Serial.write(dataByte2);
                Serial.write(125);
        }

}

/** ================================== */
/** INTERRUPT SERVICE ROUTINES         */
/** ================================== */


ISR(TIMER1_CAPT_vect){
        TCNT1 = 0;                      // reset the counter
        motorTime = ICR1;              // save the input capture value
}
