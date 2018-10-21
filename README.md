paulvha SCD30 library for Raspberry Pi
===========================================================

Based on the parts are based on SparkFun SCD30 CO2 Sensor Library ( https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library)


## Background
As part of a larger project I am looking at analyzing and understanding the air quality. 
The aim of this project was to better understand the kind of gas-types that are in the air. 

I have ported the library to CPP on a Raspberry PI running Raspbian Jessie release. It has been 
adjusted and extended for stable working.

# version 1.0	/  October 2018
 * Initial version Changed, enhanced and extended for raspberry Pi
 * by Paul van Haastrecht (paulvha@hotmail.com)
 * 
 * Resources / dependencies:
 * BCM2835 library (http://www.airspayce.com/mikem/bcm2835/)
 * twowire library (https://github.com/paulvha/twowire)

# version 2.0 / october 2018
 * by Paul van Haastrecht (paulvha@hotmail.com)
 * - some bug changes and code enhancements
 * - added softreset
 * - updated debug display
 * - changed single measurement method
 * - added option to output temperature in Fahrenheit instead of celsius
 * - extended documentation

## Software installation

Make your self superuser : sudo bash

3.1 BCM2835 library
Install latest from BCM2835 from : http://www.airspayce.com/mikem/bcm2835/

1. wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.56.tar.gz
2. tar -zxf bcm2835-1.56.tar.gz		// 1.56 was version number at the time of writing
3. cd bcm2835-1.56
4. ./configure
5. sudo make check
6. sudo make install

In order for this software to run you should NOT enable i2C in raspi-config to load the kernel drivers. 
It works directly on the hardware, but you’ll have to run program as root.

3.2 twowire library
Obtain the latest version from : https://github.com/paulvha/twowire

1. download the zip-file (clone or download / download zip-file) in the wanted directory
2. unzip twowire-master.zip (*1)
3. cd twowire-master
4. make install

*1) if you do not have unzip : sudo apt-get install zip unzip

3.3 scd30 software
Obtain the latest version from : https://github.com/paulvha/scd30_on_raspberry

1. Download the zip-file (clone or download / download zip-file) in the wanted directory
2. unzip scd30_on_raspberry-master.zip (*1)
3. cd scd30_on_raspberry-master
4. create the executable : make  \
5. To run you have to be as sudo ./scd30 -h ….

The “make” command will create an SCD30 monitor only. The command “make BUILD=DYLOS” will create an executable that is able to monitor both the SCD30 as well as the DYLOS 1700.

(detailed description of the many options in  scd30.odt)


============= ORIGINAL INFORMATION FROM SPARKFUN ===========================

