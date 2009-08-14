#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/file.h>

#include "device_ctl.h"

#ifndef VERSION
#define VERSION "0.1"
#endif

#ifndef FALSE
#define FALSE (0)
#define TRUE (!FALSE)
#endif

#define CLAMP(__x, __l, __h) \
	(((__x) > (__h)) ? (__h) : (((__x) < (__l)) ? (__l) : (__x)))

#define CDROM_PROCFILE "/proc/sys/dev/cdrom/info"
#define VOL_MIN 0
#define VOL_MAX 255

enum _command_e {
	CMD_NONE = 0,
	CMD_PLAY,
	CMD_STOP,
	CMD_PAUSE,
	CMD_NEXT,
	CMD_PREV,
	CMD_TOGGLE,
	CMD_STATUS,
	CMD_SCAN,
	CMD_DEVICE,
	CMD_INFO,
	CMD_EJECT,
	CMD_CLOSE,
	CMD_VOLUME,
	CMD_SEEK,
	CMD_QUIT,
	NUM_COMMANDS
};

typedef enum _command_e command_t;

static const char const *commands[NUM_COMMANDS] = {
	[CMD_PLAY]		= "play",
	[CMD_STOP]		= "stop",
	[CMD_PAUSE]		= "pause",
	[CMD_NEXT]		= "next",
	[CMD_PREV]		= "prev",
	[CMD_TOGGLE]	= "toggle",
	[CMD_STATUS]	= "status",
	[CMD_SCAN]		= "scan",
	[CMD_DEVICE]	= "device",
	[CMD_INFO]		= "info",
	[CMD_EJECT]		= "eject",
	[CMD_CLOSE]		= "close",
	[CMD_VOLUME]	= "volume",
	[CMD_SEEK]		= "seek",
	[CMD_QUIT]		= "quit"
};

enum _msg_severity_e {
	MSG_STD = 0,
	MSG_ERR
};

typedef enum _msg_severity_e msg_severity_t;

static int msg(msg_severity_t severity, char *format, ...)
{
	FILE *msg_file = (severity < MSG_ERR) ? stdout : stderr;
	va_list args;
	int ret;

	va_start(args, format);
	ret = vfprintf(msg_file, format, args);
	fflush(msg_file);
	
	return ret;
}

inline void free_list(char ***list, int len)
{
	int i;

	if (*list) {
		for (i = 0; i < len; i++)
			free((*list)[i]);
		free(*list);
	}
	*list = NULL;
}

#define DRIVE_NAME_STR "drive name:"

int get_cd_devices(char ***device_list)
{
	FILE *procfile;
	char *buf = NULL;
	char *buf_ptr = NULL;
	char **tmp_list;
	int i;
	size_t len;
	int num_found, ret;

	if ((procfile = fopen(CDROM_PROCFILE, "r")) == NULL) {
		msg(MSG_ERR, "Error opening " CDROM_PROCFILE " \n");
		return -1;
	}
	while ((getline(&buf, &len, procfile) > -1) &&
			(strncmp(buf, DRIVE_NAME_STR, strlen(DRIVE_NAME_STR)) != 0));
	if (fclose(procfile) < 0)
		msg(MSG_ERR, "Error closing " CDROM_PROCFILE "\n");
	if ((buf_ptr = strrchr(buf, '\n')))
		*buf_ptr = '\0';
	tmp_list = (char **) buf;
	num_found = 0;
	buf_ptr = buf;
	while ((buf_ptr = strchr(buf_ptr, '\t'))) {
		while (*buf_ptr == '\t') {
			*buf_ptr = '\0';
			buf_ptr++;
		}
		tmp_list[num_found++] = buf_ptr;
	}

	*device_list = (char **)malloc(sizeof(char*) * num_found);
	
	ret = num_found;
	for (i = 0; i < num_found; i++) {
		if (tmp_list[i][0] == '\0') {
			--ret;
			continue;
		}
		if (asprintf(&((*device_list)[i]), "/dev/%s", tmp_list[i]) < 0) {
			msg(MSG_ERR, "Error allocating memory for device list\n");
			ret = i;
			break;
		}
	}
	free(buf);

	return ret;
}

#define STATUS_OUT(__dev, __track, __state_str, __min, __sec) \
	do {\
		unsigned __emin, __esec;\
		dev_get_track_time(__track, NULL, NULL, &__emin, &__esec);\
		msg(MSG_STD, "%s %s: track #%d, %02u:%02u of %02u:%02u\n", \
				__state_str, __dev, __track, __min, __sec, __emin, __esec); \
	} while(0)

int print_status(const char *dev)
{
	dev_status_t status;
	int track = 1;
	unsigned min, sec;
	static const char *status_str[] =
		{"", "Playing", "Paused", "Stopped"};
	int rv;

	if ((rv = dev_query_status(&status, &track, &min, &sec)) < 0)
		return rv;
	
	if ((status > ST_INVALID) && (status < ST_ERROR))
		STATUS_OUT(dev, track, status_str[status], min, sec);
	else
		STATUS_OUT(dev, track, "Stopped", 0U, 0U);

	return rv;
}

int print_info(const char *dev)
{
	unsigned tracks = 0;
	unsigned i, st_min, st_sec, len_min, len_sec;

	tracks = dev_get_track_num();

	msg(MSG_STD, "Device %s: %d tracks\n", dev, tracks);
	for (i = 1; i < tracks + 1; i++) {
		if (dev_get_track_time(i, &st_min, &st_sec, &len_min, &len_sec) < 0)
			return -1;
		else
			msg(MSG_STD, "Track %2d\tstart %02d:%02d\tlength %02d:%02d\n",
					i, st_min, st_sec, len_min, len_sec);
	}

	return 0;
}

command_t get_cmd_num(const char *str)
{
	command_t ret = CMD_NONE, i;

	for (i = CMD_PLAY; i < NUM_COMMANDS; i++)
		if (strncmp(str, commands[i], strlen(commands[i])) == 0) {
			ret = i;
			break;
		}
	return ret;
}

int main(int argc, char **argv)
{
#define IOCTL_ERROR(__str) \
	do { \
		msg(MSG_ERR, "%s: Error during ioctl call\n", __str);\
		dev_close();\
		cd_device = NULL;\
		is_error = TRUE;\
	} while (0)

	int opt;
	int opt_index;
	int tracklist = -1;
	static struct option options[] = {
		{"help", 0, 0, 'h'},
		{"verbose", 0, 0, 'v'},
		{"version", 0, 0, 'V'},
		{"device", 1, 0, 'd'},
		{"noscan", 0, 0, 'n'},
		{"slave", 0, 0, 's'},
		{0, 0, 0, 0}
	};
	char *cmd_str = NULL;
	size_t line_len;
	char *cmd_ptr = NULL;
	command_t cmd;
	char *cd_device = NULL;
	char *tmp_device = NULL;
	char **device_list = NULL;
	int num_devices = 0;
	unsigned short verbose = 0;
	unsigned short slave = 0;
	int arg_val;
	int noscan = 0;
	int i;
	int is_error = FALSE;
	dev_status_t status;
	
	while ((opt = getopt_long(argc, argv, "hvVd:ns",
			options, &opt_index)) != -1) {
		switch (opt) {
			case 'h':
				msg(MSG_STD, "help not implemented yet... sorry\n");
				exit(0);
				break;
			case 'v':
				msg(MSG_STD, "verbose\n");
				verbose = 1;
				break;
			case 'V':
				msg(MSG_STD, "Maja's CD player, version " VERSION "\n");
				exit(0);
				break;
			case 'd':
				cd_device = optarg;
				break;
			case 'n':
				noscan = 1;
				break;
			case 's':
				slave = 1;
				break;
			default:
				break;
		}
	}
	if ((!cd_device) && (noscan) && (!slave)) {
		msg(MSG_ERR, "No device specified and scanning prohibited. "
				"Don't know what to do.\nExiting.\n\n");
		return EXIT_FAILURE;
	}
	if (cd_device) {
		if (dev_open(cd_device) < 0) {
			msg(MSG_ERR, "Error opening device %s\n", cd_device);
			return EXIT_FAILURE;
		}
		dev_prepare();
		if (!dev_isaudio()) {
			msg(MSG_STD, "%s: no audio disc\n", cd_device);
			dev_close();
			cd_device = NULL;
			is_error = TRUE;
		}
	}
	
	num_devices = get_cd_devices(&device_list);
	if (!cd_device && !noscan && !is_error) {
		msg(MSG_STD, "Scanning devices...\n");
		for (i = 0; i < num_devices; i++) {
			cd_device = device_list[i];
			if (dev_open(cd_device) < 0)
				msg(MSG_STD, "%s: could not open", cd_device);
			dev_prepare();
			if (dev_isaudio()) {
				msg(MSG_STD, "%s: OK\n", cd_device);
				break;
			} else
				msg(MSG_STD, "%s: no audio disc\n", cd_device);
			dev_close();
			cd_device = NULL;
		}
	}
	if ((!cd_device) && (!slave)) {
		msg(MSG_ERR, "No usable CD device found.\n");
		free_list(&device_list, num_devices);
		return EXIT_FAILURE;
	}
	if (slave) {
		msg(MSG_STD, is_error ? "ERROR\n" : "OK\n");
	}
	if (optind == argc) {
		if (!slave) {
			msg(MSG_STD, "No command given\n");
			free_list(&device_list, num_devices);
			return EXIT_FAILURE;
		}
	} else if (argv[optind])
		cmd_str = strdup(argv[optind]);
	
	do {
		is_error = FALSE;
		if ((!cmd_str) && (slave)) {
			if (getline(&cmd_str, &line_len, stdin) < 0) {
				free(cmd_str);
				cmd_str = strdup("quit");
			}
			if ((cmd_ptr = strrchr(cmd_str, '\n')))
				*cmd_ptr = '\0';
		}
		if ((!cd_device)
				&& (strncmp(cmd_str, "device", 6) != 0)
				&& (strcmp(cmd_str, "scan") != 0)
				&& (strcmp(cmd_str, "quit") != 0)) {
			msg(MSG_STD, "No device opened. "
					"Only 'scan', 'device' and 'quit' commands are allowed\n");
			msg(MSG_STD, "ERROR\n");
			free(cmd_str);
			cmd_str = NULL;
			continue;
		}
		switch (cmd = get_cmd_num(cmd_str)) {
			case CMD_PLAY:
				arg_val = -1;
				if (++optind < argc)
					arg_val = atoi(argv[optind]);
				cmd_ptr = &(cmd_str[4]);
				while (*cmd_ptr == ' ')
					cmd_ptr++;
				if ((arg_val < 0) && (*cmd_ptr != '\0'))
					arg_val = atoi(cmd_ptr);
				if (arg_val <= 0)
					arg_val = 1;
				if (dev_query_status(&status, &tracklist, NULL, NULL) < 0)
					IOCTL_ERROR("Play");
				else {
					if ((status != ST_PLAYING) || (arg_val != tracklist)) {
						STATUS_OUT(cd_device, arg_val, "Playing", 0U, 0U);
						if (dev_play(arg_val) < 0)
							IOCTL_ERROR("Play");
					} else
						if (print_status(cd_device) < 0)
							IOCTL_ERROR("Play");
					tracklist = -1;
				}
				break;
			case CMD_STOP:
				if (dev_stop() < 0)
					IOCTL_ERROR("Stop");
				break;
			case CMD_PAUSE:
				if (dev_pause() < 0)
					IOCTL_ERROR("Pause");
				break;
			case CMD_NEXT:
				if ((dev_query_status(NULL, &tracklist, NULL, NULL) < 0) ||
						(dev_stop < 0) || (dev_play(tracklist + 1) < 0))
					IOCTL_ERROR("Next");
				break;
			case CMD_PREV:
				if ((dev_query_status(NULL, &tracklist, NULL, NULL) < 0) ||
						(dev_stop < 0) || (dev_play(tracklist - 1) < 0))
					IOCTL_ERROR("Prev");
				break;
			case CMD_TOGGLE:
				if ((dev_query_status(&status, NULL, NULL, NULL) < 0) ||
						((status == ST_PLAYING) && (dev_pause() < 0)) ||
						((status == ST_PAUSED) && (dev_resume() < 0)))
					IOCTL_ERROR("Toggle");
				break;
			case CMD_EJECT:
				if (dev_eject() < 0)
					IOCTL_ERROR("Eject");
				break;
			case CMD_CLOSE:
				if (dev_close_tray() < 0)
					IOCTL_ERROR("Close");
				break;
			case CMD_STATUS:
				if (print_status(cd_device) < 0)
					IOCTL_ERROR("Status");
				break;
			case CMD_INFO:
				if (print_info(cd_device) < 0)
					is_error = TRUE;
				break;
			case CMD_SCAN:
				dev_close();
				for (i = 0; i < num_devices; i++) {
					tmp_device = device_list[i];
					if (dev_open(tmp_device) < 0) {
						msg(MSG_STD, "%s: could not open\n", tmp_device);
						msg(MSG_STD, "ERROR\n");
						is_error = TRUE;
						continue;
					}
					dev_prepare();
					if (dev_isaudio())
						msg(MSG_STD, "%s: OK\n", tmp_device);
					else
						msg(MSG_STD, "%s: no audio disc\n", tmp_device);
					dev_close();
				}
				if (cd_device) {
					if (dev_open(cd_device) < 0) {
						msg(MSG_STD, "%s: error reopening\n", cd_device);
						is_error = TRUE;
					} else
						dev_prepare();
				}
				break;
			case CMD_DEVICE:
				cmd_ptr = &(cmd_str[6]);
				while (*cmd_ptr == ' ')
					++cmd_ptr;
				is_error = TRUE;
				if (*cmd_ptr == '\0') {
					for(i = 0; i < num_devices; i++)
						msg(MSG_STD, "%s\n", device_list[i]);
					is_error = FALSE;
				} else {
					dev_close();
					for (i = 0; i < num_devices; i++) {
						if(strcmp(cmd_ptr, device_list[i]) != 0)
							continue;
						tmp_device = device_list[i];
						if (dev_open(tmp_device) < 0) {
							msg(MSG_STD, "%s: could not open\n", tmp_device);
							msg(MSG_STD, "ERROR\n");
						} else {
							dev_prepare();
							if (!dev_isaudio())
								msg(MSG_STD, "%s: no audio disc\n",
										tmp_device);
							else {
								msg(MSG_STD, "%s: OK\n", tmp_device);
								cd_device = device_list[i];
								is_error = FALSE;
							}
						}
						break;
					}
					if ((is_error) && (cd_device)) {
						if (dev_open(cd_device) < 0)
							msg(MSG_STD, "%s: error reopening\n", tmp_device);
						else
							dev_prepare();
					}
				}
				break;
			case CMD_VOLUME:
				cmd_ptr = &(cmd_str[6]);
				while (*cmd_ptr == ' ')
					cmd_ptr++;
				switch (*cmd_ptr) {
					case '+':
						arg_val = dev_get_volume();
						cmd_ptr++;
						arg_val = CLAMP(arg_val + atoi(cmd_ptr), VOL_MIN, VOL_MAX);
						break;
					case '-':
						arg_val = dev_get_volume();
						cmd_ptr++;
						arg_val = CLAMP(arg_val - atoi(cmd_ptr), VOL_MIN, VOL_MAX);
						break;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						arg_val = CLAMP(atoi(cmd_ptr), VOL_MIN, VOL_MAX);
						break;
					default:
						arg_val = -1;
						break;
				}
				if ((arg_val >= 0) && (dev_set_volume(arg_val) < 0))
					IOCTL_ERROR("Volume");
				else {
					arg_val = dev_get_volume();
					msg(MSG_STD, "Volume %d\n", arg_val);
				}
				break;
			case CMD_SEEK: {
				unsigned min, sec, st_min, st_sec;
				int trk;

				is_error = (dev_query_status(NULL, &trk, &min, &sec) < 0);
				if (!is_error)
					is_error =
						(dev_get_track_time(trk, &st_min, &st_sec, NULL, NULL) < 0);
				if (!is_error) {
					cmd_ptr = &(cmd_str[4]);
					while (*cmd_ptr == ' ')
						cmd_ptr++;
					/* Current position from beginning of the CD in seconds */
					arg_val = (int)((min + st_min) * 60 + st_sec + sec);
					switch (*cmd_ptr) {
						case '+':
							cmd_ptr++;
							arg_val += atoi(cmd_ptr);
							break;
						case '-':
							cmd_ptr++;
							arg_val -= atoi(cmd_ptr);
							break;
						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
							arg_val = atoi(cmd_ptr);
							break;
						default:
							arg_val = -1;
							break;
					}
					msg(MSG_STD, "Seeking to: %d\n", arg_val);
					if (dev_seek(arg_val) < 0) {
						msg(MSG_ERR, "Error seeking to %d sec\n", arg_val);
						is_error = TRUE;
					}
				} else
					msg(MSG_ERR, "Error querying current position\n");
				break;
			}
			case CMD_QUIT:
				noscan = 1;
				slave = 0;
				if ((cd_device) && (dev_close() < 0)) {
					msg(MSG_ERR, "Error closing device %s\n", cd_device);
					is_error = TRUE;
				}
				break;
			default:
				if (cmd_str) {
					msg(MSG_ERR, "Unknown command: %s\n", cmd_str);
					is_error = TRUE;
				} else {
					msg(MSG_ERR, "Nothing to do\n");
					is_error = TRUE;
				}
		} /* switch */
		msg(MSG_STD, is_error ? "ERROR\n" : "OK\n");
		fflush(stderr);
		
		free(cmd_str);
		cmd_str = NULL;
	} while (slave);
	
	free_list(&device_list, num_devices);

	return EXIT_SUCCESS;
}

