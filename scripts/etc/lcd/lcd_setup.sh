#!/bin/bash
 
case $1 in
    "start")
        echo hd44780 0x27 > /sys/class/i2c-adapter/i2c-1/new_device
        dev=`ls /sys/class/hd44780/`
        sleep 1
        chmod ugo+w /dev/${dev}
        printf "\e[2J\e[HCuimhne Ceoil\nStartup" > /dev/${dev}
        echo "0" > /sys/class/hd44780/${dev}/cursor_blink
        echo "0" > /sys/class/hd44780/${dev}/cursor_display
        ;;
    "stop")
        dev=`ls /sys/class/hd44780/`
        printf "\e[2J\e[HCuimhne Ceoil\nShutdown" > /dev/${dev}
        echo 0x27 > /sys/class/i2c-adapter/i2c-1/delete_device
        ;;
    '*')
        echo "$0 [start|stop]"
        ;;
esac

