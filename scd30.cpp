/*******************************************************************
 * 
 * The SCD30 measures CO2 with accuracy of +/- 30ppm.
 *
 * This library handles the initialization of the SCD30 and outputs
 * CO2 levels, relative humidty, and temperature.
 ******************************************************************
 * October 2018 : created for raspberry Pi
 * by Paul van Haastrecht (paulvha@hotmail.com)
 * 
 * version 1.0 initial Raspberry Pi
 * 
 * version 2.0 : October 2018  
 * - some bug changes and code enhancements
 * - added softreset
 * - updated debug display
 * - changed single measurement method
 * - added option to output temperature in Fahrenheit instead of celsius
 * 
 * Version 3.0 : October 2018
 * - added dewpoint and heatindex
 * 
 * Resources / dependencies:
 * BCM2835 library (http://www.airspayce.com/mikem/bcm2835/)
 * twowire library (https://github.com/paulvha/twowire)
 * 
 * The SCD30 monitor can be extended with a DYLOS 1700 monitor.
 * For this "DYLOS" needs to be set. The best way is to use the makefile
 * 
 * To create a build with only the SCD30 monitor type: 
 *      make
 *
 * To create a build with the SCD30 and DYLOS monitor type:
 *      make BUILD=DYLOS
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
 **********************************************************************/

# include "SCD30.h"

/* global constructor */ 
SCD30 MySensor;

char progname[20];

#ifdef DYLOS        // DYLOS monitor option

/* indicate these are C-programs and not to be linked */
extern "C" {
    void close_dylos();
    int read_dylos (char * buf, int len, int wait, int verbose);
    int open_dylos(char * device, int verbose);
}

typedef struct dylos
{
    char     port[MAXBUF];   // connected port (like /dev/ttyUSB0)
    bool     include;        // true = include
    uint16_t value_pm10;      // measured value PM10 DC1700
    uint16_t value_pm1;       // Measured value PM1  DC1700
} dylos;

#endif

typedef struct scd_par
{
    /* option SCD30 parameters */
    bool stop_cm;              // stop continuous measurement
    bool perform_single;       // perform a single measurement
    uint16_t interval;          // sample interval. 2 <> 1800 seconds
    int16_t frc;               // SCD30 forced recalibration  400 <>2000 ppm
    int16_t temp_offset;       // Temperature offset. 0 <> 25C
    int16_t altitude;          // altitude in meters -1520 <> 3040 meter
    int16_t pressure;          // pressure in Mbar 700 <> 1200 mbar
    bool    asc;                // true to perform Automatic Self calibration
    
    /* option program variables */
    uint16_t loop_count;        // number of measurement
    uint16_t loop_delay;        // loop delay in between measurements
    bool timestamp;             // include timestamp in output
    bool tempCel;               // show temperature in Celsius or Fahrenheit
    bool heatindex;             // add heatindex in output
    bool dewpoint;              // add dewpoint in output
    int verbose;                // verbose level
    
#ifdef DYLOS                    // DYLOS monitor option
    /* include Dylos info */
    struct dylos dylos;
#endif

} scd_par;
 
/*********************************************************************
*  @brief close hardware and program correctly
**********************************************************************/
void closeout()
{
   /* reset pins in Raspberry Pi */
   MySensor.close();
   
#ifdef DYLOS        // DYLOS monitor option
   /* close dylos */
   close_dylos();
#endif

   exit(EXIT_SUCCESS);
}

/*********************************************************************
* @brief catch signals to close out correctly 
* @param  sig_num : signal that was raised
* 
**********************************************************************/
void signal_handler(int sig_num)
{
    switch(sig_num)
    {
        case SIGINT:
        case SIGKILL:
        case SIGABRT:
        case SIGTERM:
        default:
#ifdef DYLOS                        // DYLOS monitor option
            printf("\nStopping SCD30 & Dylos monitor\n");
#else
            printf("\nStopping SCD30 monitor\n");
#endif
            closeout();
            break;
    }
}

/*****************************************
 * @brief setup signals 
 *****************************************/
void set_signals()
{
    struct sigaction act;
    
    memset(&act, 0x0,sizeof(act));
    act.sa_handler = &signal_handler;
    sigemptyset(&act.sa_mask);
    
    sigaction(SIGTERM,&act, NULL);
    sigaction(SIGINT,&act, NULL);
    sigaction(SIGABRT,&act, NULL);
    sigaction(SIGSEGV,&act, NULL);
    sigaction(SIGKILL,&act, NULL);
}

/*********************************************
 * @brief generate timestamp
 * 
 * @param buf : returned the timestamp
 *********************************************/  
void get_time_stamp(char * buf)
{
    time_t ltime;
    struct tm *tm ;
    
    ltime = time(NULL);
    tm = localtime(&ltime);
    
    static const char wday_name[][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    
    static const char mon_name[][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    sprintf(buf, "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
    wday_name[tm->tm_wday],  mon_name[tm->tm_mon],
    tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
    1900 + tm->tm_year);
}

/************************************************
 * @brief  initialise the variables 
 * @param scd : pointer to SCD30 parameters
 ************************************************/
void init_variables(struct scd_par *scd)
{
    /* option SCD30 parameters */
    scd->asc = true;                // set Automatic Self Calibration (ASC)
    scd->stop_cm = false;           // NOT stop continuous measurement
    scd->perform_single = false;    // NOT perform a single measurement
    scd->interval = 2;               // sample interval. 2 <> 1800 seconds
    scd->frc = -1;                  // SCD30 forced recalibration  400 <>2000 ppm
    scd->temp_offset = -1;          // Temperature offset. 0 <> 25C
    scd->altitude = -1;             // altitude in meters -1520 <> 3040 meter
    scd->pressure = -1;             // pressure in Mbar 700 <> 1200 mbar
    
    /* option program variables */
    scd->loop_count = 10;           // number of measurement
    scd->loop_delay = 5;            // loop delay in between measurements
    scd->timestamp = false;        // NOT include timestamp in output
    scd->dewpoint = false;         // NOT include dewpoint in output
    scd->heatindex = false;        // NOT include heatindex in output
    scd->tempCel = true;            // display temperarure in Celsius
    scd->verbose = 0;               // No verbose level

#ifdef DYLOS                        // DYLOS monitor option
    /* Dylos values */
    scd->dylos.include = false;
    scd->dylos.value_pm1 = 0;
    scd->dylos.value_pm10 = 0;
#endif
}

/**********************************************************
 * @brief initialise the Raspberry PI and SCD30 / Dylos hardware 
 * @param scd : pointer to SCD30 parameters
 *********************************************************/
void init_hw(struct scd_par *scd)
{

/* Dylos monitor MUST be started as ROOT (/dev/tty* permission) */
#ifndef DYLOS
    /* hard_I2C requires  root permission */    
    if (MySensor.settings.I2C_interface == hard_I2C)
#endif
    {
        if (geteuid() != 0)
        {
            p_printf(RED,(char *)"You must be super user\n");
            exit(EXIT_FAILURE);
        }
    }
    
    /* progress & debug messages tell driver */
    MySensor.setDebug(scd->verbose);
    
    /* start hardware and SCD30 */
    if (MySensor.begin(scd->asc, scd->interval) == false)
    {
        p_printf(RED,(char *) "Error during init I2C\n");
        exit(-1);
    }
    
    /* frc, altitude, pressure */
    if (scd->altitude != -1)
    {
        if (scd->verbose) printf("setting altitude to %d\n", scd->altitude);
        if(MySensor.setAltitudeCompensation(scd->altitude) == false) closeout();
    }
    
    /* pressure will overrule altitude */
    if (scd->pressure != -1)
    {
        if (scd->verbose) printf("setting pressure to %d\n", scd->pressure);
        
        if(MySensor.setAmbientPressure(scd->pressure) == false)
        {
            p_printf (RED, (char *) "Error during setting pressure\n");
            closeout();
        }
    }
    
    /* will overrule ASC */
    if (scd->frc != -1)
    {
        if (scd->verbose) printf("setting forced recalibration to %d\n", scd->frc);
        
        if(MySensor.setForceRecalibration(scd->frc) == false) 
        {
            p_printf (RED, (char *) "Error during setting FRC\n");
            closeout();
        }
    }
    
    /* only impacts the temperature and humidity reading. NOT the CO2 */
    if (scd->temp_offset != -1)
    {        
        if (scd->verbose) printf("setting temperature offset to %d\n", scd->temp_offset);
        
        if(MySensor.setTemperatureOffset(scd->temp_offset) == false) 
        {
            p_printf (RED, (char *) "Error during setting Temperature offset\n");
            closeout();
        }
    }
    
#ifdef DYLOS    // DYLOS monitor option
    /* init Dylos DC1700 port */ 
    if (scd->dylos.include)
    {
        if(scd->verbose) p_printf (YELLOW, (char *) "initialize Dylos\n");
        
        if (open_dylos(scd->dylos.port, scd->verbose) != 0)   closeout();
    }
#endif
}

#ifdef DYLOS        // DYLOS monitor option

/*****************************************************************
 * @brief Try to read from Dylos DC1700 monitor
 * 
 * @param scd : pointer to SCD30 parameters and Dylos values
 ****************************************************************/
bool do_dylos(struct scd_par *scd)
{
    char    buf[MAXBUF], t_buf[MAXBUF];
    int     ret, i, offset =0 ;
    
    /* if no Dylos device specified */
    if ( ! scd->dylos.include) return(false);
    
    if(scd->verbose > 0 ) printf("\nReading Dylos data ");
    
    /* reset values */
    scd->dylos.value_pm1 = scd->dylos.value_pm10 = 0;
    
    /* try to read from Dylos and wait max 2 seconds */
    ret = read_dylos(buf, MAXBUF, 2, scd->verbose);

    /* if data received : parse it */
    for(i = 0; i < ret; i++)
    {
        /* if last byte on line */
        if (buf[i] == 0xa) 
        {
            /* terminate & get PM10 */
            t_buf[offset] = 0x0;
            scd->dylos.value_pm10 = (uint16_t)strtod(t_buf, NULL);
            
            // break
            i = ret;        
        }
        
        /* skip carriage return and any carbage below 'space' */
        else if (buf[i] != 0xd && buf[i] > 0x1f) 
        {
            t_buf[offset] = buf[i];
        
            /* get PM1 */
            if (t_buf[offset] == ',')
            {
                t_buf[offset] = 0x0;
                scd->dylos.value_pm1 = (uint16_t)strtod(t_buf, NULL);
                offset=0;
            }
            else
                offset++;
        }
    }
    
    return(true);
}

#endif


/*****************************************************************
 * @brief output the results
 * 
 * @param scd : pointer to SCD30 parameters
 ****************************************************************/
void do_output(struct scd_par *scd)
{
    char buf[30],t;
    float index, dew, temp, hum;
    uint16_t co2;
    
    if (scd->timestamp) 
    {
        get_time_stamp(buf);
        printf("%s: ",buf);
    }
    
    co2 = (uint16_t) MySensor.getCO2();
    hum = MySensor.getHumidity();
    
    if (scd->tempCel)   // Celsius
    {
        temp = MySensor.getTemperature();
        index = MySensor.computeHeatIndex(temp, hum, false);
        dew = MySensor.calc_dewpoint(temp, hum, false);
        t= 'C';
   }
    else     // Fahrenheit
    {
        temp = MySensor.getTemperatureF();
        index = MySensor.computeHeatIndex(temp, hum, true);
        dew = MySensor.calc_dewpoint(temp, hum, true);
        t= 'F';
    }
    
     printf("CO2: %4d PPM\tHumdity: %3.2f %%RH  Temperature: %3.2f *%c  ",co2, hum,temp,t);
     
     if (scd->heatindex) printf("heatindex: %3.2f *%c ", index, t);
     if (scd->dewpoint)  printf("dew-point: %3.2f *%c ", dew, t);
     
#ifdef DYLOS

    if (do_dylos(scd))
       printf("  DYLOS: PM1 %4d PPM  PM10 %4d PPM",scd->dylos.value_pm1, scd->dylos.value_pm10 );

#endif
    printf("\n");
       
    /* display debug information on highest verbose level */
    if(scd->verbose == 2) MySensor.DispClockStretch();
}

/*****************************************************************
 * @brief Here the main of the program 
 * @param scd : pointer to SCD30 parameters
 ****************************************************************/
void main_loop(struct scd_par *scd)
{
    char    buf[10];
    int     loop_set, reset_retry = RESET_RETRY;
    bool    first=true;
   
    /* get the serial number (check that communication works) */
    if(MySensor.getSerialNumber(buf) == false)
    {
       p_printf (RED, (char *) "Error during getting serial number\n");
       closeout();
    }

    p_printf(YELLOW, (char *) "Serialnumber  %s\n", buf);
    
    /* single measurement requested */
    if (scd->perform_single == true)
    {
        p_printf(GREEN,(char *) "Starting single SCD30 measurement:\n");
        
        if (MySensor.StartSingleMeasurement() == false)
        {
            p_printf (RED, (char *) "Can not perform single measurement\n");
            closeout();
        }
        
        do_output(scd);
            
        return;
    }
    
    p_printf(GREEN,(char *)  "Starting SCD30 measurement:\n");
            
    /*  check for endless loop */
    if (scd->loop_count > 0 ) loop_set = scd->loop_count;
    else loop_set = 1;
    
    /* loop requested */
    while (loop_set > 0)
    {
        if(MySensor.dataAvailable() == true)
        {
            reset_retry = RESET_RETRY;
            do_output(scd);
        }
        else
        {
            if (reset_retry-- == 0)
            {
                p_printf (RED, (char *) "Retry count exceeded. perform softreset\n");
                MySensor.SoftReset();
                reset_retry = RESET_RETRY;
                first = true;
            }
            else
            {
                /* Prevent message when previous mode of the SCD30 was 
                 * STOP continuous measurement. It needs 4 seconds 
                 * at least for the first results in that case */
                 
                if (first)  first = false;
                else printf("no data available\n");
            }
        }
        
        /* delay for seconds */
        sleep(scd->loop_delay);
        
        /* check for endless loop */
        if (scd->loop_count > 0) loop_set--;
    }
}       

/*********************************************************************
* @brief usage information  
* @param scd : pointer to SCD30 parameters
**********************************************************************/

void usage(struct scd_par *scd)
{
    printf(    "%s [options]  (version %d) \n\n"
    
    "SCD30 settings: \n"
    "-a         set Automatic Self Calibration (ASC)    (default)\n"
    "-n         set NO ASC\n"
    "-i #       measurement interval period SCD30       (default %d)\n"
    "-f #       set forced recalibration value          (No default)\n"
    "-m #       set current altitude in meters          (No default)\n"
    "-o #       set temperature offset in *C            (No default)\n"
    "-p #       set ambient pressure mbar               (No default)\n"

    "-k         stop continuous measurement             (No default)\n"
    "-c         set for continuous measurement          (default)\n"
    "-S         perform single measurement              (No default)\n"
    
    "\nprogram settings\n"
    "-B         Do not display output in color\n"
    "-l #       number of measurements (0 = endless)    (default %d)\n"
    "-w #       waittime (seconds) between measurements (default %d)\n"
    "-v #       verbose/ debug level (0 - 2)            (default %d)\n"
    "-t         add timestamp to output                 (default no stamp)\n"
    "-u         add dew-point to output\n"
    "-x         add heat-index to output\n"
    "-F         show temperature in Fahrenheit\n"
    
#ifdef DYLOS 
    "\nDylos DC1700: \n"
    "-D port    Enable Dylos input from port            (No default)\n"
#endif    
    "\nI2C settings: \n"
    "-H         use hardware I2C                        (default:soft_I2C)\n"
    "-q #       set I2C speed                           (default is %dkhz)\n"
    "-s #       set SDA GPIO for soft_I2C               (default GPIO %d)\n"
    "-d #       set SCL GPIO for soft_I2C               (default GPIO %d)\n"
    "-P         set internal pullup resistor on SDA/SCL (default not set)\n"
    
   ,progname, version, scd->interval, scd->loop_count, scd->loop_delay, scd->verbose,
   SCD30_SPEED, DEF_SDA, DEF_SCL);
}

/*********************************************************************
 * Parse parameter input 
 * @param scd : pointer to SCD30 parameters
 
 *********************************************************************/ 

void parse_cmdline(int opt, char *option, struct scd_par *scd)
{
    switch (opt) {
        
    case 'a':   // set Automatic Self Calibration (ASC)
        scd->asc = true;
        break;

    case 'n':   // set NO Automatic Self Calibration (ASC)
        scd->asc = false;
        break;
        
    case 'm':   // altitude in meters
        scd->altitude = (int16_t) strtod(option, NULL);
      
        // 700 mbar ~ 3040M altitude, 1200mbar ~ -1520
        if (scd->altitude < -1520 || scd->altitude > 3040)
        {
            p_printf (RED, (char *) "Incorrect altitude. Must be between -1520 and 3040 meter\n");
            exit(EXIT_FAILURE);
        }
      
        if (scd->pressure != -1)
        {
            p_printf (RED, (char *) "Either set altitude or pressure\n");
            exit(EXIT_FAILURE);
        }
        break;
      
     case 'p':   // pressure in Mbar
        scd->pressure = (int16_t) strtod(option, NULL);
        
        // setting to zero will de-activate
        if (scd->pressure != 0)
        {
            // must be between 700 and 1200 mbar
            if (scd->pressure < 700 || scd->pressure > 1200)
            {
                p_printf (RED, (char *) "Incorrect pressure. Must be between 700 and 1200 mbar\n");
                exit(EXIT_FAILURE);
            }
        }
                    
        if (scd->altitude != -1)
        {
            p_printf (RED, (char *) "Either set altitude or pressure\n");
            exit(EXIT_FAILURE);
        }
        break;   

    case 'i':   // SCD30 interval 
        scd->interval = (uint16_t) strtod(option, NULL);
        
        // must be between 2 and 1800 seconds
        if (scd->interval < 2 || scd->interval > 1800)
        {
            p_printf (RED, (char *) "Incorrect interval %d. Must be between 2 and 1800 seconds\n", scd->interval);
            exit(EXIT_FAILURE);
        }
        break;
     
     case 'o':   // temperature offset
        scd->temp_offset = (uint16_t) strtod(option, NULL);
        
        // must be between 0 and 25 degrees
        if (scd->temp_offset < 0 || scd->temp_offset > 25)
        {
            p_printf (RED, (char *) "Incorrect temperature offset %d. Must be between 0 and 25C degrees\n",scd->temp_offset );
            exit(EXIT_FAILURE);
        }
        break;
               
    case 'f':   // SCD30 forced recalibration value
        scd->frc = (uint16_t) strtod(option, NULL);
        scd->asc = false;
        
        // must be between 400 and 2000 ppm
        if (scd->frc < 400 || scd->frc > 2000)
        {
            p_printf (RED, (char *) "Incorrect recalibration value (FRC) %d. Must be between 400 and 2000 ppm\n", scd->frc);
            exit(EXIT_FAILURE);
        }
        
        break;

    case 'S':   // perform single measurement (NO break !)
        scd->perform_single = true;
     
    case 'k':   // stop continuous measurement
        scd->interval = 0;       // set for NO interval to stop in MySensor.begin()
        break;   

 
    case 'B':   // set for no color output
        NoColor = true;
        break; 
              
    case 'l':   // loop count
        scd->loop_count = (uint16_t) strtod(option, NULL);
        break;
          
    case 'w':   // loop delay in between measurements
        scd->loop_delay = (uint16_t) strtod(option, NULL);
        break;
    
    case 't':  // Add timestamp to output
        scd->timestamp = true;
        break;

    case 'F':  // show temperature in Fahrenheit
        scd->tempCel = false;
        break;
                
    case 'v':   // set verbose / debug level
        scd->verbose = (int) strtod(option, NULL);

        // must be between 0 and 2
        if (scd->verbose < 0 || scd->verbose > 2)
        {
            p_printf (RED, (char *) "Incorrect verbose/debug. Must be  0,1, 2 \n");
            exit(EXIT_FAILURE);
        }
        break;

    case 'u':   // add heatindex to output
        scd->heatindex = true;
        break;
        
    
    case 'x':   // add dewpoint to output
        scd->dewpoint = true;
        break; 
                         
    case 'H':   // i2C interface 
        MySensor.settings.I2C_interface = hard_I2C; 
        break;
 
    case 'P':   // enable internal BCM2835 pullup resistor 
        MySensor.settings.pullup = true; 
        break;       
    
    case 'q':   // i2C Speed
        MySensor.settings.baudrate = (uint32_t) strtod(option, NULL);
         
        if (MySensor.settings.baudrate  < 1 || MySensor.settings.baudrate  > 400)
        {
            p_printf(RED,(char *) "Invalid i2C speed option %dKhz\n",MySensor.settings.baudrate );
            exit(EXIT_FAILURE);
        }
        break; 
      
    case 'd':   // change default SCL line for soft_I2C
        MySensor.settings.scl = (int)strtod(option, NULL);
      
        if (MySensor.settings.scl < 2 || MySensor.settings.scl == 4 || 
        MySensor.settings.scl > 27 || MySensor.settings.sda == MySensor.settings.scl)
        {
            p_printf(RED,(char *) "invalid GPIO for SCL :  %d\n",MySensor.settings.scl);
            exit(EXIT_FAILURE);
        }   
        break; 

    case 's':   // change default SDA line for soft_I2C
        MySensor.settings.sda = (int)strtod(option, NULL);
      
        if (MySensor.settings.sda < 2 || MySensor.settings.sda == 4 || 
        MySensor.settings.sda > 27 || MySensor.settings.sda == MySensor.settings.scl)
        {
            p_printf(RED,(char *) "Invalid GPIO for SDA :  %d\n", MySensor.settings.sda);
            exit(EXIT_FAILURE);
        }   
        break;       

    case 'D':   // include Dylos read
#ifdef DYLOS
        strncpy(scd->dylos.port, option, MAXBUF);
        scd->dylos.include = true;
#else
        p_printf(RED, (char *) "Dylos is not supported in this build\n");
#endif
        break;
    case 'h':   // help  (No break)
    
    default: /* '?' */
        usage(scd);
        exit(EXIT_FAILURE);
    }
}

/***********************
 *  program starts here
 **********************/
 
int main(int argc, char *argv[])
{
    int opt;
    struct scd_par scd; // parameters
    
    /* set signals */
    set_signals(); 
 
    /* save name for (potential) usage display */
    strncpy(progname,argv[0],20);
    
    /* set the initial values */
    init_variables(&scd);

    /* parse commandline */
    while ((opt = getopt(argc, argv, "ani:f:m:o:p:kcSBl:v:w:tHs:d:q:PD:hFxu")) != -1)
    {
        parse_cmdline(opt, optarg, &scd);
    }

    /* initialise hardware */
    init_hw(&scd);
  
    /* main loop to read SCD30 results */
    main_loop(&scd);
    
    closeout();
}
