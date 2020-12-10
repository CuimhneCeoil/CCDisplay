#!/bin/bash
dir=`dirname $0`
msg=`cat ${dir}/message.txt`

modprobe hd44780-i2c

case $1 in
    "start")
        echo hd44780 0x27 > /sys/class/i2c-adapter/i2c-1/new_device
        dev=`ls /sys/class/hd44780/`
        sleep 1
        udevadm trigger /dev/${dev}
        printf "\e[2J\e[H${msg}Startup" > /dev/${dev}
        echo "0" > /sys/class/hd44780/${dev}/cursor_blink
        echo "0" > /sys/class/hd44780/${dev}/cursor_display
        ;;
    "stop")
        dev=`ls /sys/class/hd44780/`
        printf "\e[2J\e[H${msg}Shutdown" > /dev/${dev}
        echo 0x27 > /sys/class/i2c-adapter/i2c-1/delete_device
        ;;
    *)
        echo "$0 [start|stop]"
        echo "Message: ${msg}"
        echo "--------------------"
        echo Message may be changed by editing ${dir}/message.txt
        ;;
esac

