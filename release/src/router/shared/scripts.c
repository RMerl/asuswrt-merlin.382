#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <dirent.h>
#include <ctype.h>
#include "shutils.h"
#include "shared.h"


int _xstart(const char *cmd, ...)
{
        va_list ap;
        char *argv[16];
        int argc;
        int pid;

        argv[0] = (char *)cmd;
        argc = 1;
        va_start(ap, cmd);
        while ((argv[argc++] = va_arg(ap, char *)) != NULL) {
                //
        }
        va_end(ap);

        return _eval(argv, NULL, 0, &pid);
}

long fappend(FILE *out, const char *fname)
{
	FILE *in;
	char buf[1024];
	int n;
	long r;

	if ((in = fopen(fname, "r")) == NULL) return -1;
	r = 0;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			r = -1;
			break;
		}
		else {
			r += n;
		}
	}
	fclose(in);
	return r;
}

void run_custom_script(char *name, char *args)
{
	char script[120];

	snprintf(script, sizeof(script), "/jffs/scripts/%s", name);

	if(f_exists(script)) {
		if (nvram_match("jffs2_scripts", "0")) {
			logmessage("custom_script", "Found %s, but custom script execution is disabled!", name);
			return;
		}
		if (args)
			logmessage("custom_script" ,"Running %s (args: %s)", script, args);
		else
			logmessage("custom_script" ,"Running %s", script);
		xstart(script, args);
	}
}

void run_custom_script_blocking(char *name, char *arg1, char *arg2)
{
	char script[120];
	char *cmd[4];

	snprintf(script, sizeof(script), "/jffs/scripts/%s", name);

	if(f_exists(script)) {
		if (nvram_match("jffs2_scripts", "0")) {
			logmessage("custom_script", "Found %s, but custom script execution is disabled!", name);
			return;
		}
		if (arg1)
			logmessage("custom_script" ,"Running %s (args: %s %s) - max timeout = 120s", script, arg1, (arg2 ? arg2 : ""));
		else
			logmessage("custom_script" ,"Running %s - max timeout = 120s", script);

		cmd[0] = script;
		cmd[1] = arg1;
		cmd[2] = arg2;
		cmd[3] = NULL;
		_eval( cmd, NULL, 120, NULL);
	}
}

void run_postconf(char *name, char *config)
{
	char filename[64];

	snprintf(filename, sizeof (filename), "%s.postconf", name);
	run_custom_script_blocking(filename, config, NULL);
}


void use_custom_config(char *config, char *target)
{
        char filename[256];

        snprintf(filename, sizeof(filename), "/jffs/configs/%s", config);

	if (check_if_file_exist(filename)) {
		if (nvram_match("jffs2_scripts", "0")) {
			logmessage("custom config", "Found %s, but custom configs are disabled!", filename);
			return;
		}
		logmessage("custom config", "Using custom %s config file.", filename);
		eval("cp", filename, target, NULL);
	}
}


void append_custom_config(char *config, FILE *fp)
{
	char filename[256];

	snprintf(filename, sizeof(filename), "/jffs/configs/%s.add", config);

	if (check_if_file_exist(filename)) {
		if (nvram_match("jffs2_scripts", "0")) {
			logmessage("custom config", "Found %s, but custom configs are disabled!", filename);
			return;
		}
		logmessage("custom config", "Appending content of %s.", filename);
		fappend(fp, filename);
	}
}

int _shutdown_start(const char *cmd, ...)
{
	va_list ap;
	char args[128] = {0};
	char *arg = NULL;
	int maxlength = (sizeof(args) / sizeof(args[0])) - 1;

	va_start(ap, cmd);
	arg = (char *)cmd;
	while (arg != NULL && maxlength > 0) {
		if (args[0] != '\0')
			strncat(args, " ", maxlength);
		strncat(args, arg, maxlength);
		maxlength -= (strlen(arg) + 1);
		arg = va_arg(ap, char *);
	}
	va_end(ap);

	return _shutdown_start_str(args);
}

// returns zero if custom script allows the system shutdown
 int _shutdown_start_str(const char *args)
{
	int result = 0;
	int shutdown_started = nvram_get_int("shutdown_started");

	if (shutdown_started == 0) {
		nvram_set_int("shutdown_started", 1); // custom script is running
		nvram_unset("shutdown_cancel");
		run_custom_script_blocking("shutdown-start", args);

		if (nvram_get_int("shutdown_cancel") == 1) {
			// custom script has canceled the system shutdown
			nvram_unset("shutdown_started"); // reset the flag
			result = 1; // shutdown was canceled by custom script
		}
		else
			nvram_set_int("shutdown_started", 2); // custom script finished (or disabled)
	}
	else if (shutdown_started == 1)
		result = 2; // simultaneous shutdown attempts are prohibited

	return result;
}

