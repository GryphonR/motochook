# motoChook
An adaptation of the eChook code to log signals from engine sensors on a motorbike, with second microcontroller to drive SD card logging and a display.

## Overview

An arduino nano samples the engine sensors at 10hz, also takes in an 12v pulsed signal for engine RPM. This is output over USART via the eChook packeting format.

An ESP8266 Node MCU (picked as it was on my desk, is cheap and is pretty fast) receives and decodes the USART data, writes it to an SD card in CSV format at 10Hz, and displays it on a 3.12" OLED SSD1322.

## Sensor Inputs to Arduino Nano:

A0 - Coolant Temp

A2 - Oil Pressure

A3 - TPS

A5 - Oil Temp

A7 - Lambda

D2 - Engine RPM

### Dependencies

[ESP8266_SSD1322](https://github.com/winneymj/ESP8266_SSD1322)

[Bounce 2](https://github.com/thomasfredericks/Bounce2)

NB: As of 08/18 the version of Sd2Card.cpp included in the ESP8266 Arduino libraries needs modifying for this to work. As default it has the SPI speed for the SD card hard coded to 250KHz.

Line 286 needs to be modified from
```
Sd2Card.cpp to settings = SPISettings(250000, MSBFIRST, SPI_MODE0);
```
to
```
Sd2Card.cpp to settings = SPISettings(sckRateID, MSBFIRST, SPI_MODE0);
```
