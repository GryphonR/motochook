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

//DEBUG_MODE.  If 1, no data is sent to screen, allowing debug serial to be added.
#define DEBUG_MODE 0

// Filter toggles. To disable a filter, comment out the relevent line
#define OP_FILTER_ENABLE
#define OT_FILTER_ENABLE
#define CT_FILTER_ENABLE

//Time over which the rolling average is taken. 1 = 100ms, 5 = 0.5 seconds, 10 = 1 second
#define CAL_OP_FILTER_TIME 5
#define CAL_OT_FILTER_TIME 30
#define CAL_CT_FILTER_TIME 30

// Throttle Calibration mode - with this enabled it will display on screen instead of LAMBDA
// and will display 'counts' instead of percentage. Use this to determine Min and Max counts for
// 0 and 100%, and enter them below. Uncomment to  Enable/Comment to disable.

// #define THROTTLE_CAL_MODE

#define CAL_THROTTLE_COUNT_MIN 52
#define CAL_THROTTLE_COUNT_MAX 957

#define CAL_MOTOR_PULSES_PER_REVOLUTION 3
