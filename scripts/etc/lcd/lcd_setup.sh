#!/bin/bash
dir=`dirname $0`
msg=`cat ${dir}/message.txt`

#id=0x27
id=0x21

modprobe hd44780-i2c

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
        printf "\e[2J\e[H${msg}\nStartup" > /dev/${dev}
        echo "0" > /sys/class/hd44780/${dev}/cursor_blink
        echo "0" > /sys/class/hd44780/${dev}/cursor_display
        ;;
    "stop")
        dev=`ls /sys/class/hd44780/`
        printf "\e[2J\e[H${msg}\nShutdown" > /dev/${dev}
        echo ${id} > /sys/class/i2c-adapter/i2c-1/delete_device
        ;;
    *)
        echo "$0 [start|stop]"
        echo "Message: ${msg}"
        echo "--------------------"
        echo Message may be changed by editing ${dir}/message.txt
        ;;
esac

