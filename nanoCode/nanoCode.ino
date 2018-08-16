/***** ======================================================================================================================================== *****/
/*****           ====================================================================================================================           *****/
/*****                     ================================================================================================                     *****/
/*****                                                                                                                                          *****/
/*****                                                       eChook Telemetry Board Code                                                        *****/
/*****                                                              ARDUINO NANO                                                                *****/
/*****                                                                                                                                          *****/
/*****                                                    Modified for engine signal logging                                                    *****/
/*****                                                                                                                                          *****/
/*****                                                               IAN COOPER                                                                 *****/
/*****                                                             ROWAN GRIFFIN                                                                *****/
/*****                                                                BEN NAGY                                                                  *****/
/*****                                                              MATT RUDLING                                                                *****/
/*****                                                                                                                                          *****/
/*****                     ================================================================================================                     *****/
/*****           ====================================================================================================================           *****/
/***** ======================================================================================================================================== *****/



/** ================================== */
/** Includes                           */
/** ================================== */

#include <math.h>
#include <Bounce2.h>
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

/** ___________________________________________________________________________________________________ DIGITAL AND PWM OUTPUT PINS */
const int LED_1_OUT_PIN       = 6;     // PWM Output for LED 1
const int LED_2_OUT_PIN       = 9;     // PWM Output for LED 2


/** ________________________________________________________________________________________ BLUETOOTH CONSTANTS */
/* Communication SETUP PARAMETERS */
const long BT_BAUDRATE       = 115200;              // Baud Rate to run at. Must match Arduino's baud rate.

//Bluetooth module uses hardware serial from Arduino, so Arduino Tx -> HC05 Rx, Ard Rx -> HC Tx. EN and Status are disconnected.


/** ________________________________________________________________________________________ CONSTANTS */
/* DATA TRANSMIT INTERVALS */
const unsigned long     SHORT_DATA_TRANSMIT_INTERVAL     = 100;     // transmit interval in ms
const unsigned long     LONG_DATA_TRANSMIT_INTERVAL     = 100;     // transmit interval in ms


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
unsigned long   lastShortDataSendTime       = 0;
unsigned long   lastLongDataSendTime        = 0;
unsigned long   lastMotorSpeedPollTime[3]   = {0,0,0};    // poll interval for wheel speed
int       	  	loopCounter              		= 0;


/** ___________________________________________________________________________________________________ INTERRUPT VERIABLES */
/** Any variables that are being used in an Interrupt Service Routine need to be declared as volatile. This ensures
 *  that each time the variable is accessed it is the master copy in RAM rather than a cached version within the CPU.
 *  This way the main loop and the ISR variables are always in sync
 */
volatile unsigned int motorPoll[3] = {0,0,0};
int motorPollCount = 0;


int rpmFilterArray[10];
int rpmFilterCount = 0;

/** ___________________________________________________________________________________________________ Sensor Readings */

float coolantTemp           = 0;
float throttle              = 0;
float lambda                = 0;
float oilPressure       	  = 0;
float motorRPM        		  = 0;
float oilTemp       		    = 0;


/** ___________________________________________________________________________________________________ Smoothing Variables */


/** ================================== */
/** SETUP                                */
/** ================================== */
void setup()
{

  //Set up pin modes for all inputs and outputs
  pinMode(LED_1_OUT_PIN,        OUTPUT);
  pinMode(LED_2_OUT_PIN,        OUTPUT);

  pinMode(COOL_TEMP_IN_PIN,  	  INPUT);
  pinMode(OIL_PRESSURE_IN_PIN,      	INPUT);
  pinMode(THROTTLE_IN_PIN,    	INPUT);
  pinMode(OIL_PRESSURE_IN_PIN,        	INPUT);
  pinMode(OIL_TEMP_IN_PIN,       	INPUT);

  /**
   * Set up Interrupts:
   * When the specified digital change is seen on a the interrupt pin it will pause the main loop and
   * run the code in the Interrupt Service Routine (ISR) before resuming the main code.
   * The interrupt number is not the pin number on the arduino Nano. For explanation see here:
   * https://www.arduino.cc/en/Reference/AttachInterrupt
  */

  attachInterrupt(0, motorSpeedISR, RISING);

  /**
   * Initialise Serial Communication
   * If communication over bluetooth is not working or the results are garbled it is likely the
   * baud rate set here (number in brakcets after Serial.begin) and the baud rate of the bluetooth
   * module aren't set the same.
   *
   * A good tutorial for altering the HC-05 Bluetooth Module parameters is here:
   * http://www.instructables.com/id/Modify-The-HC-05-Bluetooth-Module-Defaults-Using-A/
   *
   * The HC-05 modules commonly come preset with baud rates of 9600 or 32000
   *
   * Alternatively the following bit of code will attempt to automatically configure a
   * HC-05 module if it is plugged in in AT (setup) mode then the arduino is reset. (Power Arduino,
   * unplug HC-05 module, press button on HC-05 module, plug back in holding button [light should blink slowly],
   * release button, then reset Arduino)
  */


  Serial.begin(BT_BAUDRATE);    // Bluetooth and USB communications

  lastShortDataSendTime = millis(); //Give the timing a start value.
  lastLongDataSendTime = millis(); //Give the timing a start value.

} //End of Setup

void loop()
{
        // We want to check different variables at different rates. For most variables 0.25 seconds will be good for logging and analysis.
        // Certain variables either can't have or do not need this resolution.
        // Wheel and Motor speed are accumulated over time, so the longer time left between samples, the higher the resolution of the value.
        // As such, these are only updated ever 1 second. Temperature is a reading that will not change fast, and consumes more processing
        // time to calculate than most, so this is also checked every 1s.


  if (millis() - lastLongDataSendTime >= LONG_DATA_TRANSMIT_INTERVAL) //i.e. if 250ms have passed since this code last ran
  {
    lastLongDataSendTime = millis(); //this is reset at the start so that the calculation time does not add to the loop time
    motorRPM = readMotorRPM();
    // rpmFilterArray[rpmFilterCount] = motorRPM;
    // rpmFilterCount ++;
    // if(rpmFilterCount == 10){
    //   rpmFilterCount = 0;
    // }
    //
    // int sum = 0;
    //
    // for(int i = 0; i<10; i++){
    //   sum += rpmFilterCount;
    // }
    //
    // motorRPM = sum/10;

    sendData(MOTOR_ID, motorRPM);

  }

  if (millis() - lastShortDataSendTime >= SHORT_DATA_TRANSMIT_INTERVAL) //i.e. if 250ms have passed since this code last ran
  {
    lastShortDataSendTime = millis(); //this is reset at the start so that the calculation time does not add to the loop time
    loopCounter = loopCounter + 1; // This value will loop 1-4, the 1s update variables will update on certain loops to spread the processing time.

                //It is recommended to leave the ADC a short recovery period between readings (~1ms). To ensure this we can transmit the data between readings

                coolantTemp = readCoolantTemp();
                // coolantTemp = 0;
                sendData(COOL_TEMP_ID, coolantTemp);

                throttle = readThrottle(); // if this is being used as the input to a motor controller it is recommended to check it at a higher frequency than 4Hz
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
                        digitalWrite(13, HIGH); //these are just flashing the LEDs as visual confimarion of the loop
                        digitalWrite(9, HIGH);
                }

                if (loopCounter == 2)
                {
                }

                if (loopCounter == 3)
                { //nothing actaully needed to do at .75 seconds
                        digitalWrite(13, LOW);
                        digitalWrite(9, LOW);
                }

                if (loopCounter == 4)
                {
                        loopCounter = 0; //4 * 0.25 makes one second, so counter resets
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

        return (tempCoolantTemp);
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
        float tempOP = analogRead(OIL_PRESSURE_IN_PIN);

        tempOP = map((int)tempOP, 102,921,0,1500)/10;

        return (tempOP);
}


int readThrottle() //This function is for a variable Throttle input
{
        int tempThrottle = analogRead(THROTTLE_IN_PIN);

        tempThrottle = map(tempThrottle, 102, 905, 0, 100);

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

        return (temp); //return Temperature.
}


float readMotorRPM()
{
  // First action is to take copies of the motor poll count and time so that the variables don't change during the calculations.
  int tempMotorPoll = motorPoll[motorPollCount];
  motorPoll[motorPollCount] = 0;

  unsigned long tempMotorPollTime = millis();
  unsigned long tempLastMotorPollTime = lastMotorSpeedPollTime[motorPollCount];
  lastMotorSpeedPollTime[motorPollCount] = tempMotorPollTime;


  motorPollCount = motorPollCount < 2? motorPollCount+1 : 0; //If count is less than 2, increment, else 0


        // Now calculate the number of revolutions of the motor shaft

  int pulsesPerRevolution = 3;

  float motorRevolutions = tempMotorPoll; //assuming one poll per revolution. Otherwise a divider is needed
  float timeDiffms = tempMotorPollTime - tempLastMotorPollTime;

  // How many time periods in a minute:
  // 60000 ms per  minute
  float tppm = 60000/timeDiffms;

  //pulses per minute
  float ppm = motorRevolutions * tppm;

  float motorShaftRPM = ppm/pulsesPerRevolution;

        return (motorShaftRPM);
}


/**
 * Calculation Functions
 *
 * Where calculations are needed multiple times, they are broken out into their own functions here
 */


/** ================================== */
/** BLUETOOTH DATA PACKETING FUNCTIONS */
/** ================================== */
/** The two functions in this section handle packeting the data and sending it over USART to the bluetooth module. The two functions are
 *  identically named so are called the in the same way, however the first is run if the value passed to it is a float and the second is
 *  run if the value passed into it is an integer (an override function). For all intents and purposes you can ignore this and simply call
 *  'sendData(identifier, value);' using one of the defined identifiers and either a float or integer value to send informaion over BT.
 *
 * identifier:  see definitions at start of code
 * value:       the value to send (typically some caluclated value from a sensor)
 */

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
                        {
                                dataByte2 = 0xFF;
                        }

                        if (integer == 0)
                        {
                                dataByte1 = 0xFF;
                        }

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
                        //dataByte1 = dataByte1 || 0x10000000; //flag for integer send value
                        dataByte1 += 128;
                        dataByte2 = (byte) tens;

                        if (tens == 0)
                        {
                                dataByte2 = 0xFF;
                        }

                        if (hundreds == 0)
                        {
                                dataByte1 = 0xFF;
                        }
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
                        {
                                dataByte2 = 0xFF;
                        }

                        if (hundreds == 0)
                        {
                                dataByte1 = 0xFF;
                        }
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

/** This function is triggered every time the magnet on the motor shaft passes the Hall effect
 *  sensor. Care must be taken to ensure the sensor is position the correct way and so is the
 *  magnet as it will only respond in a specific orientation and magnet pole.
 *
 *  It is best practice to keep ISRs as small as possible.
 */
void motorSpeedISR()
{
  motorPoll[0]++;
  motorPoll[1]++;
  motorPoll[2]++;
}
