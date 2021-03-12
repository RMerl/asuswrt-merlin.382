#include "rc.h"

void start_uuacc(void)
{
	stop_uuacc();

	if(getpid()!=1) {
		notify_rc("start_uuacc");
		return;
	}
	exec_uu();
}

void stop_uuacc(void)
{
	doSystem("killall uuplugin_monitor.sh");
	if (pidof("uuplugin"))
		doSystem("killall uuplugin");
}

