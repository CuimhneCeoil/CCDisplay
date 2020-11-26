#
# Regular cron jobs for the cuimhne-plugin package
#
0 4	* * *	root	[ -x /usr/bin/cuimhne-plugin_maintenance ] && /usr/bin/cuimhne-plugin_maintenance
