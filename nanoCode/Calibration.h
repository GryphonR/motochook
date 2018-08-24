/*
 * This file contains all the values used to calibrate your eChook board.
 * By seperating these it makes it possible to update to newer code without
 * having to copy your calibrations across - just make sure you keep this file!
 *
 * The values provided by default are for the eChook's team dev board, but each
 * board is different. These defaults will give readings in the right ballpark,
 * but for accuracy, see the documentation to calibrate your own board.
 *
 * Every varialbe in this file begins with CAL_ so that it is identifiable in
 * the main code as being defined in this file.
 *
 */


#define OP_FILTER_ENABLE
#define OT_FILTER_ENABLE
#define CT_FILTER_ENABLE


const int CAL_MOTOR_PULSES_PER_REVOLUTION = 3;     //Outer circumference of tyre, in Meters. i.e. the distance travelled in one revolution

const int CAL_THROTTLE_COUNT_MIN = 52;
const int CAL_THROTTLE_COUNT_MAX = 957;

//Board Specific Calibrations
const float CAL_REFERENCE_VOLTAGE   = 5;     // Voltage seen on the arduino 5V rail
