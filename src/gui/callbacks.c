#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "callbacks.h"
#include "backend_ctl.h"

static cb_controls_t ctrls;
static gboolean keep_going = TRUE;

cb_controls_t *cb_get_controls(void)
{
	return &ctrls;
}

void cb_refresh_controls(void)
{
	backend_status_t status;
	static backend_status_t prev_status =
	{
		.state = STATUS_OTHER,
		.track = 1,
		.min = 0,
		.sec = 0,	
		.lmin = 0,
		.lsec = 0	
	};
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *tmp_str;
	

	/* may be called from periodic refresh as well as from any
	 * other callback */
	static GStaticMutex refresh_lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&refresh_lock);
	if (backend_ctl_command(CMD_STATUS, &status) < 0)
	{
		g_warning("Error getting device status");
		goto leave;
	}
	if (status.state == STATUS_ERROR)
	{
		if (backend_ctl_command(CMD_SCAN, &tmp_str) < 0)
		{
			g_warning("No audio cd found in any device");
			goto leave;
		}
		if (backend_ctl_command(CMD_DEVICE, tmp_str) < 0)
		{
			g_warning("Could not switch to device %s", tmp_str);
			g_free(tmp_str);
			goto leave;
		}
		if (backend_ctl_command(CMD_STATUS, &status) < 0)
		{
			g_warning("Error getting device status");
			goto leave;
		}
	}
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(ctrls.device_combo)) < 0)
	{
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(ctrls.device_combo));
		if (gtk_tree_model_get_iter_first(model, &iter))
		{
			tmp_str = NULL;
			do
			{
				g_free(tmp_str);
				gtk_tree_model_get(model, &iter, 0, &tmp_str, -1);
			}
			while (strcmp(tmp_str, status.device)
					&& gtk_tree_model_iter_next(model, &iter));
			g_free(tmp_str);
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(ctrls.device_combo),
					&iter);
		}
		else
		{
			g_warning("No devices in list");
			goto leave;
		}
	}
	if ((prev_status.track != status.track) ||
			(prev_status.state == STATUS_OTHER))
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(ctrls.track_combo),
				status.track - 1);
		prev_status.track = status.track;
	}
	prev_status.state = status.state;
	if ((prev_status.lsec != status.lsec) ||
			(prev_status.lmin != status.lmin))
	{
		gtk_range_set_range(GTK_RANGE(ctrls.pos_slider),
				0.0, (gdouble)(status.lmin * 60.0 + status.lsec));
		prev_status.lsec = status.lsec;
		prev_status.lmin = status.lmin;
	}
	if ((prev_status.sec != status.sec) ||
			(prev_status.min != status.min))
	{
		gtk_range_set_value(GTK_RANGE(ctrls.pos_slider),
				(gdouble)(status.min * 60.0 + status.sec));
		prev_status.sec = status.sec;
		prev_status.min = status.min;
	}

leave:
	g_static_mutex_unlock(&refresh_lock);
}

gboolean cb_periodic_check(gpointer data)
{
	cb_refresh_controls();
	return keep_going;
}

void on_play_activate(GtkWidget *menuitem, gpointer user_data)
{
	gint track =
		gtk_combo_box_get_active(GTK_COMBO_BOX(ctrls.track_combo)) + 1;

	backend_ctl_command(CMD_PLAY_TRK, &track);
	cb_refresh_controls();
}

void on_pause_activate(GtkWidget *menuitem, gpointer user_data)
{
	backend_ctl_command(CMD_TOGGLE, NULL);
	cb_refresh_controls();
}

void on_next_activate(GtkWidget *menuitem, gpointer user_data)
{
	backend_ctl_command(CMD_NEXT, NULL);
	cb_refresh_controls();
}

void on_prev_activate(GtkWidget *menuitem, gpointer user_data)
{
	backend_ctl_command(CMD_PREV, NULL);
	cb_refresh_controls();
}

void on_stop_activate(GtkWidget *menuitem, gpointer user_data)
{
	backend_ctl_command(CMD_STOP, NULL);
	cb_refresh_controls();
}

void on_random_toggled(GtkWidget *menuitem, gpointer user_data)
{
	g_debug("random toggled");
}

void on_shuffle_toggled(GtkWidget *menuitem, gpointer user_data)
{
	g_debug("shuffle toggled");
}

void on_quit_activate(GtkWidget *menuitem, gpointer user_data)
{
	backend_status_t status;

	keep_going = FALSE;
	sleep(1);
	backend_ctl_command(CMD_STATUS, &status);
	if (status.device)
	{
		g_free(status.device);
	}
	backend_ctl_command(CMD_SHUTDOWN, NULL);
	gtk_main_quit();
}

/* arg1 is the track position in seconds */
gchar *on_pos_scale_format(GtkScale *scale, gdouble arg1, gpointer user_data)
{
	ldiv_t time = ldiv((long) arg1, 60L);
	
	return g_strdup_printf("%02ld:%02ld", time.quot, time.rem);
}

void on_track_combo_changed(GtkComboBox *track_combo, gpointer user_data)
{
	gint track;

	track = gtk_combo_box_get_active(track_combo) + 1;

	if (track < 1)
	{
		return;
	}
	backend_ctl_command(CMD_PLAY_TRK, &track);
}

gboolean on_pos_scale_button_release(GtkWidget *pos_scale,
		GdkEventButton *event, gpointer user_data)
{
	gint value = (int) gtk_range_get_value(GTK_RANGE(pos_scale));

	backend_ctl_command(CMD_SEEK, &value);

	return FALSE;
}

gboolean on_vol_scale_button_release(GtkWidget *vol_scale,
		GdkEventButton *event, gpointer user_data)
{
	gint value = (int) gtk_range_get_value(GTK_RANGE(vol_scale));

	backend_ctl_command(CMD_SEEK, &value);

	return FALSE;
}

void on_device_combo_changed(GtkComboBox *device_combo, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *tmp_str;
	dev_info_t *dev_info = NULL;
	guint i;
	
	model = gtk_combo_box_get_model(device_combo);
	if (!gtk_combo_box_get_active_iter(device_combo, &iter))
	{
		g_warning("No device selected");
		return;
	}
	gtk_tree_model_get(model, &iter, 0, &tmp_str, -1);
	backend_ctl_command(CMD_DEVICE, tmp_str);
	backend_ctl_command(CMD_INFO, &dev_info);
	g_free(tmp_str);

	if (dev_info)
	{
		for (i = 1; i <= dev_info->tracks; i++)
		{
			tmp_str = g_strdup_printf("Track %d", i);
			gtk_combo_box_append_text(GTK_COMBO_BOX(ctrls.track_combo), tmp_str);
			g_free(tmp_str);
		}
	}
	else
	{
		gtk_combo_box_set_active_iter(device_combo, &iter);
		gtk_combo_box_append_text(GTK_COMBO_BOX(ctrls.track_combo),
				"No audio CD");
		gtk_widget_set_sensitive(ctrls.track_combo, FALSE);
	}
}

