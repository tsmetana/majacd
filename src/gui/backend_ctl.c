#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/file.h>

#include "backend_ctl.h"

static gint cld_stdin;
static gint cld_stdout;
static gint cld_stderr;

enum _response_t
{
	RESP_OK,
	RESP_ERROR,
	RESP_OTHER,
	RESP_IOERROR
};

typedef enum _response_t response_t;

#define ATTEMPT_MAX 10
static response_t get_stdout_line(gchar *buf, guint len)
{
	gchar c = '\0';
	guint i = 0;
	gint bytes_read;
	guint attempt = 0;

	if (len > 0)
	{
		buf[0] = '\0';
	}
	else
	{
		return RESP_IOERROR;
	}
	fsync(cld_stdout);
	while ((c != '\n') && (i < (len)))
	{
		do
		{
			bytes_read = read(cld_stdout, (void*)&c, sizeof(c));
		}
		while ((errno == EAGAIN) && (attempt++ < ATTEMPT_MAX));
		if (bytes_read < 0)
		{
			g_warning("Error reading child's stdout: %s", g_strerror(errno));
			return RESP_IOERROR;
		}
		buf[i++] = c;
	}
	buf[i - 1] = '\0';
	
	if (strncmp(buf, "OK", 2) == 0)
	{
		g_debug("Got \"%s\" response", buf);
		return RESP_OK;
	}
	else if (strncmp(buf, "ERROR", 5) == 0)
	{
		g_warning("Got \"%s\" response", buf);
		return RESP_ERROR;
	}
	g_debug("Got \"%s\" response", buf);

	return RESP_OTHER;
}

#define STD_BUF_SIZE 512
#define SMALL_BUF_SIZE 32
inline static response_t check_ok(void)
{
	gchar buf[STD_BUF_SIZE];

	return get_stdout_line(buf, STD_BUF_SIZE);
}

static backend_status_t get_status(void)
{
	gchar buf[STD_BUF_SIZE];
	response_t response = get_stdout_line(buf, STD_BUF_SIZE);
	static backend_status_t status =
	{
		.device = NULL
	};
	gchar state_str[STD_BUF_SIZE];
	gchar device_str[STD_BUF_SIZE];
	gchar *cp = NULL;
	
	status.state = STATUS_OTHER;
	g_free(status.device);
	status.device = NULL;

	switch (response)
	{
		case RESP_OK:
		{
			status.state = STATUS_READY;
			break;
		}
		case RESP_ERROR:
		{
			status.state = STATUS_ERROR;
			break;
		}
		case RESP_IOERROR:
		{
			status.state = STATUS_ERROR;
			g_warning("Read error");
			break;
		}
		case RESP_OTHER:
		{
			g_debug("status read out: %s", buf);
			if (strncmp(buf, "Stopped", 7) == 0)
			{
				status.state = STATUS_READY;
			}
			else if (strncmp(buf, "Playing", 7) == 0)
			{
				status.state = STATUS_PLAYING;
			}
			else if (strncmp(buf, "Paused", 6) == 0)
			{
				status.state = STATUS_PAUSED;
			}
			sscanf(buf, "%s %s track #%d, %u:%u of %u:%u", (char *)(&state_str),
					(char *)(&device_str), &(status.track),
					&(status.min), &(status.sec),
					&(status.lmin), &(status.lsec));
			status.device =	g_strdup(device_str);
			if ((cp = strrchr(status.device, ':')))
			{
				*cp = '\0';
			}
			g_debug("%s track %d (%u:%u/%u:%u)", status.device, status.track, status.min, status.sec, status.lmin, status.lsec);
			break;
		}
		default:
		{
			break;
		}
	}

	return status;
}


inline static void send_cmd(const gchar *cmd)
{
	if (write(cld_stdin, cmd, strlen(cmd)) != strlen(cmd))
	{
		g_warning("Error sending command: %s", cmd);
	}
	fsync(cld_stdin);
}

#define BACKEND_ARGC 3
#define MAX_DEVICES 8
/* data:
 * cmd == CMD_INIT => gchar* with path to the application
 * cmd == CMD_PLAY_TRK => track number to play
 * cmd == CMD_STATUS => pointer (backend_status_t *) to store the status
 * cmd == CMD_INFO => pointer (dev_info_t *) to store the info
 * cmd == CMD_DEVICE => pointer (char ***) to store array of CD device names or
 *                      the device name (char *) to use
 * cmd == CMD_VOLUME => pointer to int (volume level)
 * cmd == CMD_SEEK => pointer to int (seek to second)
 */
gint backend_ctl_command(const backend_cmd_t cmd, gpointer data)
{
	gchar *backend_argv[BACKEND_ARGC] = {NULL, "-s", NULL};
	GPid backend_pid = -1;
	GError *err = NULL;
	GtkWidget *err_dialog = NULL;
	backend_status_t status;
	response_t response;
	static gchar response_buffer[MAX_DEVICES][STD_BUF_SIZE];
	gchar *cmd_str;
	gchar *buf_ptr;
	static dev_info_t dev_info;
	guint i, j;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
	gint retval = 0;

	status.state = STATUS_OTHER;

	/* there might be more threads */
	g_static_mutex_lock(&mutex);
	switch (cmd)
	{
		case CMD_INIT:
		{
			backend_argv[0] = (gchar *)data;
			if (!g_spawn_async_with_pipes(NULL, backend_argv, NULL,
					G_SPAWN_SEARCH_PATH, NULL, NULL, &backend_pid,
					&cld_stdin, &cld_stdout, &cld_stderr, &err))
			{
				err_dialog = gtk_message_dialog_new(NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"Error starting backend: %s",
						err->message);
				gtk_window_set_title(GTK_WINDOW(err_dialog), "Error");
				gtk_dialog_run(GTK_DIALOG(err_dialog));
				gtk_widget_destroy(err_dialog);
				g_warning("Error starting backend: %s", err->message);
				retval = -1;
				break;
			}
			/* the O_RDWR is needed to block writing to the file while we read
			 * it -- it prevents de-syncing with the child process */
			fcntl(cld_stdout, F_SETFL, O_RDWR);
			fcntl(cld_stderr, F_SETFL, O_RDWR);
			fcntl(cld_stdin, F_SETFL, O_RDWR);
			i = 0U;
			while ((i++ < ATTEMPT_MAX) && (check_ok() != RESP_OK));

			break;
		}
		case CMD_SHUTDOWN:
		{
			send_cmd("quit\n");
			check_ok();
			break;
		}
		case CMD_PLAY:
		{
			send_cmd("play\n");
			response = check_ok();
			if (response == RESP_OTHER)
			{
				response = check_ok();
			}
			else
			{
				g_warning("Unexpected response from the backend");
			}
			break;
		}
		case CMD_PLAY_TRK:
		{
			cmd_str = g_strdup_printf("play %d\n", *((gint *)data));
			send_cmd(cmd_str);
			g_free(cmd_str);
			break;
		}
		case CMD_PAUSE:
		{
			send_cmd("toggle\n");
			check_ok();
			break;
		}
		case CMD_TOGGLE:
		{
			send_cmd("toggle\n");
			check_ok();
			break;
		}
		case CMD_STOP:
		{
			send_cmd("stop\n");
			check_ok();
			break;
		}
		case CMD_NEXT:
		{
			send_cmd("next\n");
			check_ok();
			break;
		}
		case CMD_PREV:
		{
			send_cmd("prev\n");
			check_ok();
			break;
		}
		case CMD_INFO:
		{
			send_cmd("info\n");
			response = get_stdout_line(response_buffer[0], STD_BUF_SIZE);
			/* response_buffer looks like
			 * "Device: <something> tracks: <number>" */
			if (response == RESP_OTHER)
			{
				while ((response = check_ok()) == RESP_OTHER);
				if (response != RESP_OK)
				{
					*((dev_info_t **)data) = NULL;
					break;
				}
				buf_ptr = (gchar *)response_buffer;
				while ((*buf_ptr != ' ') && (*buf_ptr != '\0'))
				{
					++buf_ptr;
				}
				++buf_ptr;
				dev_info.name = buf_ptr;
				while ((*buf_ptr != ':') && (*buf_ptr != '\0'))
				{
					++buf_ptr;
				}
				*buf_ptr = '\0';
				++buf_ptr;
				dev_info.tracks = atoi(buf_ptr);
				*((dev_info_t **)data) = &dev_info;
			}
			else
			{
				*((dev_info_t **)data) = NULL;
			}
			break;
		}
		case CMD_DEVICE:
		{
			/* Not too elegant... data may be either address of a NULL pointer
			 * which means to return the array of available devices, or
			 * data may be just a non-NULL address of an array of chars (string)
			 * which means to change the device to the one in the string */
			if (*((gchar ***)data) != NULL)
			{
				g_debug("Changing device to %s", (gchar *) data);
				cmd_str = g_strdup_printf("device %s\n",(gchar *)data);
				send_cmd(cmd_str);
				g_free(cmd_str);
				response = check_ok();
				if ((response = RESP_OTHER) && (check_ok() != RESP_OK))
				{
					g_warning("Error changing device to %s", (gchar *)data);
					retval = -1;
				}
			}
			else
			{
				g_debug("Scanning devices");
				send_cmd("device\n");
				i = 0;
				do
				{
					response =
						get_stdout_line(response_buffer[i++], STD_BUF_SIZE);
				}
				while ((i < MAX_DEVICES) && (response == RESP_OTHER));
				*((gchar ***)data) =
					(gchar **)g_malloc(sizeof(gchar *) * i);
				for (j = 0; j < (i - 1); j++)
				{
					(*((gchar ***)data))[j] = g_strdup(response_buffer[j]);
				}
				(*((gchar ***)data))[i - 1] = NULL;
				
			}
			break;
		}
		case CMD_STATUS:
		{
			send_cmd("status\n");
			status = get_status();
			if ((status.state != STATUS_ERROR))
			{
				check_ok();
			}
			else
			{
				retval = -1;
			}
			if (data)
			{
				memcpy((void *)data, &status, sizeof(backend_status_t));
			}
			break;
		}
		case CMD_SCAN:
		{
			if (!data)
			{
				break;
			}
			send_cmd("scan\n");
			i = 0;
			*((char **)data) = NULL;
			do
			{
				gchar **split_str;
				response =
					get_stdout_line(response_buffer[i], STD_BUF_SIZE);
				split_str = g_strsplit(response_buffer[i], ": ", 2);
				if (!(*((char **)data)) && split_str[1]
						&& (strncmp(split_str[1], "OK", 2) == 0))
				{
					g_debug("found audio device %s", split_str[0]);
					*((char **)data) = g_strdup(split_str[0]);
				}
				g_strfreev(split_str);
				++i;
			}
			while ((i < MAX_DEVICES) && (response == RESP_OTHER));
			if (!(*((char **)data)))
			{
				retval = -1;
			}
			break;
		}
		case CMD_SEEK:
		{
			cmd_str = g_strdup_printf("seek %d\n", *((int *)data));
			send_cmd(cmd_str);
			response = check_ok();
			if (response == RESP_OTHER)
			{
				response = check_ok();
			}
			else
			{
				g_warning("Unexpected response from the backend");
			}
			break;
		}
		case CMD_VOLUME:
		{
			cmd_str = g_strdup_printf("volume %d\n", *((int *)data));
			send_cmd(cmd_str);
			response = check_ok();
			if (response == RESP_OTHER)
			{
				response = check_ok();
			}
			else
			{
				g_warning("Unexpected response from the backend");
			}
			break;
		}
		default:
		{
			g_warning("Unknown command: %d", cmd);
			break;
		}
	}
	g_static_mutex_unlock(&mutex);

	return retval;
}

