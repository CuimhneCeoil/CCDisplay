#!/bin/bash -x
dir=`dirname $0`

## name.txt contains the name to be displayed during startup
name=`cat ${dir}/name.txt`
## the text to follow the name on starup
start_msg=`cat ${dir}/start_msg.txt`
## the text to follow the name on shutdown
stop_msg=`cat ${dir}/stop_msg.txt` 

## create the entire start message
start="{name}
${start_msg}"

## create the entire stop message
stop="{name}
${stop_msg}"

## specify the address of the LCD device
#id=0x27
id=0x21

## define the log levels
LOG_ERR=0
LOG_WARNING=1
LOG_NOTICE=2
LOG_INFO=3
LOG_DEBUG=4

## load the driver if necessary
modprobe hd44780-i2c loglevel=${LOG_WARNING} startup="${strt}"

case $1 in
    "start")
        ## make sure we have the proper dkms build version
        dkms autoinstall
        echo hd44780 ${id} > /sys/class/i2c-adapter/i2c-1/new_device
        unset dev
        count=0
        while [ -z "${dev}"]
        do
            sleep 1
            dev=`ls /sys/class/hd44780/`
            count=`expr $count + 1`
            if [ ${count} -gt 60 ]
            then
                echo "Unable to connect to hd44780 device"
                exit 1
            fi
            
        done
        printf "\e[H${start}" > /dev/${dev}
        echo "0" > /sys/class/hd44780/${dev}/cursor_blink
        echo "0" > /sys/class/hd44780/${dev}/cursor_display
        ;;
    "stop")
        dev=`ls /sys/class/hd44780/`
        printf "\e[2J\e[H${stop}" > /dev/${dev}
        echo ${id} > /sys/class/i2c-adapter/i2c-1/delete_device
        ;;
    *)
        echo "$0 [start|stop]"
        echo "Name: ${msg}"
        echo "Start: ${start_msg}"
        echo "Stop: ${stop_msg}"
        echo "--------------------"
        echo Name may be changed by editing ${dir}/name.txt
        echo Start may be changed by editing ${dir}/start_msg.txt
        echo Stop may be changed by editing ${dir}/stop_msg.txt
        ;;
esac

