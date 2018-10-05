/*********************************************************************
 * Supporting routines to access and read Dylos information and measured
 * values. 
 * 
 * Dylos is registered trademark Dylos Corporation
 * 2900 Adams St#C38, Riverside, CA92504 PH:877-351-2730
 * 
 * Copyright (c) 2017 Paul van Haastrecht <paulvha@hotmail.com>
 *
 * *******************************************************************
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software. If not, see <http://www.gnu.org/licenses/>. 
 * *******************************************************************
 * December 2017 / paulvha 
 * version 1.1  This is a scaled down version of the Dylosmonitor.
 * 
 *********************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

/* default device */
#define DYLOS_USB   "/dev/ttyUSB0"

/* file descriptor to device */
int fd=0;

/* pipes / inter process */
int  ctop[2], ptoc[2];
pid_t dylos_ch = 0;

/* indicate connected & initialised */
int connected = 0;

/* save & restore current port setting */
struct termios old_options;

/****************************************************
 *  close the Dylos connection correctly 
 ****************************************************/
void close_dylos()
{
    /* if child running */
    if (dylos_ch > 0)
    {
         // request child to stop
         write(ptoc[1],"s",1);
         exit(0);
    }
    
    /* this is executed by child (if it gets that far...) */
    if (fd)
    {
        // restore old settings
        if (tcsetattr(fd, TCSANOW, &old_options) < 0)
            printf("Unable to restore serial setting on device.\n");
    
        close(fd);
        
        connected = fd =  0;
        
        printf("Dylos connection has been closed.\n");
    }
}

/******************************************************* 
 * Dylos will sent every minute an update which is
 * captured by child program. The results are passed to
 * parent with a read_dyls() call
 * 
 * @param verbose : display progress messages
 ******************************************************/
void constant_read(int verbose)
{
    char cmdbuf[10];
    char buf[20],sbuf[20] = {0};
    
    /* wait for data */
    while(1)
    {
        /* read from parent (none blocking) */
        if (read(ptoc[0], cmdbuf, 20) > 0)
        {
            switch(cmdbuf[0])
            {
                case 'b':
                    if (verbose > 1) 
                        printf("Dylos child received request for buffer\n");
                    
                    /* if incomplete message in sbuf */
                    if (sbuf[strlen(sbuf)-1] != 0xa )
                        write(ctop[1], "empty", 5);
                    else
                        write(ctop[1], sbuf, strlen(sbuf));
                    
                    break;
            
                case 's':
                    if (verbose > 1) 
                        printf("Dylos child received request to stop Dylos\n");
                    close_dylos();
                    exit(0);
                    break;
            }
        }
        
        /* clear buffer */
        memset(buf, 0x0, 20);

        /* try to read Dylos data (max 20 characters) */
        if (read(fd, buf, 20) > 0)
        {
            // if an incomplete received message was stored : append
            if (sbuf[strlen(sbuf) -1] != 0x0a) sprintf(sbuf,"%s%s",sbuf,buf);
            
            // else write new (start)
            else strncpy(sbuf,buf,20);
        }
    }
}

/*************************************************************
 * set the serial configuration correct to read Dylos device
 * returns Ok(0) or error (-1) 
 *************************************************************/
 
int serial_configure()
{
    struct termios options;
    
    tcgetattr(fd, &options);
    tcgetattr(fd, &old_options);    // restore later
    
    cfsetispeed(&options, B9600);   // set input speed
    cfsetospeed(&options, B9600);   // set output speed
    
    options.c_cflag &=  ~CSIZE;
    options.c_cflag |= CS8;         // 8 bit
    options.c_cflag &= ~CSTOPB;     // 0 stopbit
    options.c_cflag &= ~CRTSCTS;    // no flow control
    options.c_cflag &= ~PARENB;     // no parity
    
    options.c_iflag &= ~(IXON|IXOFF);   // no flow control
    
    // if open with  O_NDELAY | O_NONBLOCK this is ignored. otherwise blocks
    options.c_cc[VMIN] = 1;         // need at least character
    options.c_cc[VTIME] = 0;        // no timeout between bytes
    
    options.c_cflag |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(ISTRIP | IGNCR | INLCR | ICRNL);
    
    options.c_oflag &= ~(OPOST);
    
    if (tcsetattr(fd, TCSANOW, &options) < 0) 
    {
        printf("Unable to configure Dylos port.\n");
        return(-1);
    }
   
    return(0);
}


/********************************************************************
 * open connection to Dylos & start child
 * 
 @param device: the device to use to connect to Dylos
 * @param verbose : display progress level
 * 
 * if device = NULL, default device will be used 
 *******************************************************************/
 
int open_dylos(char * device, int verbose)
{
    /* if already initialised */
    if (connected)  return(0);
    
    /* create pipes */
    if (pipe(ctop) == -1 || pipe(ptoc) == -1)
    {
        printf("can not create pipes for Dylos read\n");
        return(-1);
    }   
    
    /* create child */
    dylos_ch = fork();
    
    if (dylos_ch < 0)
    {
        printf(" can not create child process for Dylos\n");
        return(-1);
    }
    
    /* if parent */
    if (dylos_ch > 0)
    {
        close(ctop[1]); // close write end to parent
        close(ptoc[0]); // close read end to child
 
        /* set for non-blocking write to child */
        if (fcntl(ptoc[1],F_SETFL,O_NONBLOCK) == -1)
        {
            printf("can not set non-blocking pipes for Dylos write\n");
            return(-1);
        }   

        return(0);
    } 
    
    /* below is for child */
    close(ctop[0]); // close read end to parent
    close(ptoc[1]); // close write end to child   

    if (fcntl(ptoc[0],F_SETFL,O_NONBLOCK) == -1)
    {
        printf("can not set non-blocking pipes for Dylos read\n");
        exit(-1);
    }  
    
    /* open device */
    if (device == NULL) device = DYLOS_USB;
    
    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
    
    if (fd < 0)
    {
        printf("Unable to open device %s.\n",device);
        
        if (geteuid() != 0)
            printf("You do not have root permission. Start with: sudo  ...\n");
        
        return(-1);
    }
    
    /* flush any pending input or output data */
    tcflush(fd,TCIOFLUSH);
    
    /* configure */
    if (serial_configure() < 0) return(-1);
    
    /* set as opened / initialised */
    connected = 1;

    if (verbose > 1) printf("Dylos device %s is ready.\n",device);
    
    /* go in loop */
    constant_read(verbose);
    
    return(0);
}

/***********************************************************
 * This routine is used by the parent to obtain data that
 * has been received by the child from the Dylos device
 * 
 * @param buf : pointer to buffer allocated by user 
 * @param len : length of allocated buffer 
 * @param wait : max time in seconds before returning
 * @param verbose : display progress messages
 * if wait = 0 the read will block untill received data
 * 
 * returns number of characters read into buffer or 0 
 **********************************************************/
int read_dylos (char * buf, int len, int wait, int verbose)
{
    int     num;
    time_t  time_start;
    
    /* clear buffer */
    memset(buf, 0x0, len);
    
    /* get start time */
    time_start=time(NULL);
    
    /* loop for data */
    while(1)
    {
        /* send request buffer (will be handled by child
         * in constant_read)*/
        write(ptoc[1],"b",1);
        
        /* try to read */
        num = read(ctop[0], buf, len);
        
        /* if nothing, sleep second */
        if (num == -1)  sleep(1);
        
        else 
        {
            if (verbose > 1) printf("Dylos reader got : %s\n", buf);
        
            /* got empty or something. expected format like 2240,126*/
            if (num < 7) return(0);
            return(num);
        }
            
        /* if wait time was requested, check whether elapsed */
        if (wait > 0)
        {
            if (wait < time(NULL) - time_start)
            return(0);
        }
    } 
}
