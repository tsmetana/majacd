#ifndef _BACKEND_CTL_H
#define _BACKEND_CTL_H

enum _backend_cmd_t
{
	CMD_INIT,
	CMD_SHUTDOWN,
	CMD_PLAY,
	CMD_PLAY_TRK,
	CMD_PAUSE,
	CMD_TOGGLE,
	CMD_STOP,
	CMD_NEXT,
	CMD_PREV,
	CMD_INFO,
	CMD_DEVICE,
	CMD_STATUS,
	CMD_SCAN,
	CMD_SEEK,
	CMD_VOLUME,
	NUM_CMD
};

typedef enum _backend_cmd_t backend_cmd_t;

enum _backend_state_t
{
	STATUS_READY,
	STATUS_PLAYING,
	STATUS_PAUSED,
	STATUS_OTHER,
	STATUS_ERROR
};

typedef enum _backend_state_t backend_state_t;

struct _dev_info_t
{
	gchar *name;
	gint tracks;
};

typedef struct _dev_info_t dev_info_t;

struct _backend_status_t
{
	backend_state_t state;
	int track;
	unsigned min;
	unsigned sec;
	unsigned lmin;
	unsigned lsec;
	gchar *device;
};

typedef struct _backend_status_t backend_status_t;

gint backend_ctl_command(const backend_cmd_t cmd, gpointer data);

#endif
