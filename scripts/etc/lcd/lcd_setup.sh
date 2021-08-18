#!/bin/bash -x
dir=`dirname $0`
msg=`cat ${dir}/message.txt`
strt="${msg}
Booting
Please Wait..."
#id=0x27
id=0x21

LOG_ERR=0
LOG_WARNING=1
LOG_NOTICE=2
LOG_INFO=3
LOG_DEBUG=4

modprobe hd44780-i2c loglevel=${LOG_WARNING} startup="${strt}"

case $1 in
    "start")
        ## make sure we have the proper dkms build version
        dkms autoinstall
        echo hd44780 ${id} > /sys/class/i2c-adapter/i2c-1/new_device
        unset dev
        while [ -z "${dev}" ]
        do
            sleep 1
            dev=`ls /sys/class/hd44780/`
        done
        printf "\e[H${msg}\nStartup\nPlease wait..." > /dev/${dev}
        echo "0" > /sys/class/hd44780/${dev}/cursor_blink
        echo "0" > /sys/class/hd44780/${dev}/cursor_display
        ;;
    "stop")
        dev=`ls /sys/class/hd44780/`
        printf "\e[2J\e[H${msg}\nShutdown\nPlease wait..." > /dev/${dev}
        echo ${id} > /sys/class/i2c-adapter/i2c-1/delete_device
        ;;
    *)
        echo "$0 [start|stop]"
        echo "Message: ${msg}"
        echo "--------------------"
        echo Message may be changed by editing ${dir}/message.txt
        ;;
esac

