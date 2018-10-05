/****************************************************************
 * 
 * Based on original library from Sparkfun : 
 * 
 * This is a library written for the SCD30
 * SparkFun sells these at its website: www.sparkfun.com
 * Do you like this library? Help support SparkFun. Buy a board!
 * https://www.sparkfun.com/products/14751
 *
 * The SCD30 measures CO2 with accuracy of +/- 30ppm.
 *
 * This library handles the initialization of the SCD30 and outputs
 * CO2 levels, relative humidty, and temperature.
 ******************************************************************
 * October 2018 : Changed, enhanced and extended for raspberry Pi
 * by Paul van Haastrecht (paulvha@hotmail.com)
 * 
 * version 1.0 initial Raspberry Pi
 * 
 * Resources / dependencies:
 * BCM2835 library (http://www.airspayce.com/mikem/bcm2835/)
 * twowire library (https://github.com/paulvha/twowire)
 * 
 * *****************************************************************
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation, either version 3 of the 
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 ********************************************************************/

#ifndef __SCD30_H__
#define __SCD30_H__

# include <twowire.h>
# include <getopt.h>
# include <signal.h>
# include <stdint.h>
# include <stdarg.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <time.h>


/* set version number */
# define version 1

/* The default I2C address for the SCD30 is 0x61 */
#define SCD30_ADDRESS 0x61

/* default speed 100 Khz*/
# define SCD30_SPEED 100

/* default GPIO for SOFT_I2C */
#define DEF_SDA 2
#define DEF_SCL 3

# define MAXBUF 100

/* Available commands */
#define COMMAND_CONTINUOUS_MEASUREMENT      0x0010
#define COMMAND_SET_MEASUREMENT_INTERVAL    0x4600
#define COMMAND_GET_DATA_READY              0x0202
#define COMMAND_READ_MEASUREMENT            0x0300
#define COMMAND_AUTOMATIC_SELF_CALIBRATION  0x5306
#define COMMAND_SET_FORCED_RECALIBRATION_FACTOR 0x5204
#define COMMAND_SET_TEMPERATURE_OFFSET      0x5403
#define COMMAND_SET_ALTITUDE_COMPENSATION   0x5102
#define CMD_READ_SERIALNBR                  0xD033
#define CMD_START_SINGLE_MEAS               0x0006
#define CMD_STOP_MEAS                       0x0104

struct scd30_p
{
  public:

    /*! driver information */
    bool         hw_initialized;     // initialized or not
    bool         I2C_interface;      // hard_I2C or soft_I2C
    uint8_t     I2C_Address;        // slave address
    uint16_t    baudrate;           // speed
    uint8_t     sda;                // SDA GPIO (soft_I2C only)
    uint8_t     scl;                // SCL GPIO (soft_I2C only)
    bool         pullup;             // enable internal BCM2835 resistor
};

class SCD30
{
  public:
            
        /*! structure with values */
        scd30_p settings;
        
        /*! constructor */
        SCD30(void);
        
        /*! initialize the library 
         *
         * ASC = set / not set Auto Self Calibration
         * interval = continuous measurement every X seconds
         */
        bool begin(bool asc, uint16_t interval);
        
        /*! begin continuous measurements */
        bool beginMeasuring(uint16_t pressureOffset);
        bool beginMeasuring(void);
        
        /*! stop measurements */
        bool StopMeasurement(void);

        /*! perform single measurement */
        bool StartSingleMeasurement(void);

        /*! get serial number */
        bool getSerialNumber(char *val);

        /*! get measurement values */
        bool dataAvailable();
        bool readMeasurement();
        uint16_t getCO2(void);
        float getHumidity(void);
        float getTemperature(void);
        float getTemperatureF(void);

        /*! set options */
        bool setMeasurementInterval(uint16_t interval);
        bool setAmbientPressure(uint16_t pressure_mbar);
        bool setAltitudeCompensation(uint16_t altitude);
        bool setAutoSelfCalibration(bool enable);
        bool setForceRecalibration(uint16_t val);
        
        /*! only to compensate for the temperature reading */
        bool setTemperatureOffset(float tempOffset);

        /*! close driver and release memory */
        void close(void);
        
        /*! enable debug messages */
        void setDebug(int val);
        
        /*! display the clock stretch statistics */
        void DispClockStretch();

  private:
        /*! display debug messages */
        void debug_cmd(uint16_t command);
        
        /*! low level communication routines */
        bool sendCommand(uint16_t command, uint16_t arguments);
        bool sendCommand(uint16_t command);
        bool sendCommand(uint16_t command, uint16_t arguments, uint8_t len);

        uint16_t readRegister(uint16_t registerAddress);
        bool readbytes(char *buff, uint8_t len);

        bool checkCrc(uint8_t *data, uint8_t len, uint8_t crc_rec); 
        uint8_t computeCRC8(uint8_t data[], uint8_t len);
};

/*! to display in color  */
void p_printf (int level, char *format, ...);

// color display enable
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define WHITE   5

#define REDSTR "\e[1;31m%s\e[00m"
#define GRNSTR "\e[1;92m%s\e[00m"
#define YLWSTR "\e[1;93m%s\e[00m"
#define BLUSTR "\e[1;34m%s\e[00m"

// disable color output
extern int NoColor;

#endif  // End of definition check
