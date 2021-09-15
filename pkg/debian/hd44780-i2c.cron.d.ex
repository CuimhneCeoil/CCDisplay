#
# Regular cron jobs for the hd44780-i2c package
#
0 4	* * *	root	[ -x /usr/bin/hd44780-i2c_maintenance ] && /usr/bin/hd44780-i2c_maintenance
