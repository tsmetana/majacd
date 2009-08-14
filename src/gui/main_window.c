#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <gtk/gtk.h>
#include <string.h>

#include "main_window.h"
#include "callbacks.h"
#include "backend_ctl.h"

static GtkWidget *main_window = NULL;

static gboolean on_main_window_delete(GtkWidget *widget,
		GdkEvent  *event, gpointer user_data)
{
	if (main_window)
	{
		gtk_widget_hide_all(main_window);
	}

	return TRUE;
}

GtkWidget *main_window_create(void)
{
	GtkWidget *toolbar;
	GtkWidget *layout_table;
	cb_controls_t *ctrls = cb_get_controls();
	gchar **device_list = NULL;
	guint i;

	if (main_window)
	{
		return main_window;
	}
	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(main_window), "MajaCD");
	gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_MOUSE);
	gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);
	gtk_window_set_icon_name(GTK_WINDOW(main_window), "gtk-cdrom");

	/* 4 rows, 2 columns, non-homogenous */
	layout_table = gtk_table_new(4, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(main_window), layout_table);
	
	ctrls->device_combo = gtk_combo_box_new_text();
	ctrls->track_combo = gtk_combo_box_new_text();
	
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), FALSE);
	ctrls->play_btn = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	ctrls->pause_btn = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_PAUSE);
	ctrls->stop_btn = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
	ctrls->prev_btn = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS);
	ctrls->next_btn = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_NEXT);
/*	ctrls->eject_btn =
			gtk_tool_button_new(gtk_image_new_from_icon_name("media-eject",
				gtk_toolbar_get_icon_size(GTK_TOOLBAR(toolbar))), "Eject");*/
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ctrls->play_btn, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ctrls->pause_btn, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ctrls->stop_btn, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ctrls->prev_btn, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ctrls->next_btn, -1);
/*	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ctrls->eject_btn, -1);*/
	
	ctrls->pos_slider = gtk_hscale_new_with_range(0, 100, 1);
	/* FIXME: could volume be in a different range? */
	ctrls->vol_slider = gtk_vscale_new_with_range(0, 255, 1);
	gtk_scale_set_draw_value(GTK_SCALE(ctrls->pos_slider), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(ctrls->pos_slider), GTK_POS_TOP);
	gtk_scale_set_draw_value(GTK_SCALE(ctrls->vol_slider), FALSE);
	gtk_range_set_update_policy(GTK_RANGE(ctrls->pos_slider),
				GTK_UPDATE_DISCONTINUOUS);
	gtk_range_set_update_policy(GTK_RANGE(ctrls->vol_slider),
				GTK_UPDATE_DISCONTINUOUS);

	g_signal_connect((gpointer) ctrls->pos_slider, "format-value",
			G_CALLBACK(on_pos_scale_format), NULL);
	g_signal_connect((gpointer) ctrls->pos_slider, "button-release-event",
			G_CALLBACK(on_pos_scale_button_release), NULL);
	g_signal_connect((gpointer) ctrls->vol_slider, "button-release-event",
			G_CALLBACK(on_vol_scale_button_release), NULL);
	g_signal_connect((gpointer) ctrls->play_btn, "clicked",
			G_CALLBACK(on_play_activate), NULL);
	g_signal_connect((gpointer) ctrls->pause_btn, "clicked",
			G_CALLBACK(on_pause_activate), NULL);
	g_signal_connect((gpointer) ctrls->stop_btn, "clicked",
			G_CALLBACK(on_stop_activate), NULL);
	g_signal_connect((gpointer) ctrls->prev_btn, "clicked",
			G_CALLBACK(on_prev_activate), NULL);
	g_signal_connect((gpointer) ctrls->next_btn, "clicked",
			G_CALLBACK(on_next_activate), NULL);

	g_signal_connect((gpointer) main_window, "delete-event",
			G_CALLBACK(on_main_window_delete), NULL);

	gtk_table_attach(GTK_TABLE(layout_table), ctrls->device_combo, 0, 1, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(layout_table), ctrls->track_combo, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(layout_table), ctrls->pos_slider, 0, 1, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(layout_table), ctrls->vol_slider, 1, 2, 0, 3,
			GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(layout_table), toolbar, 0, 2, 3, 4,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	backend_ctl_command(CMD_DEVICE, &device_list);
	
	for (i = 0; device_list[i]; i++)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(ctrls->device_combo),
				device_list[i]);
		g_free(device_list[i]);
	}
	g_free(device_list);
	if (i == 1)
	{
		/* just one device */
		gtk_widget_set_sensitive(ctrls->device_combo, FALSE);
	}
	
	g_signal_connect((gpointer) ctrls->device_combo, "changed",
			G_CALLBACK(on_device_combo_changed), NULL);
	
	cb_refresh_controls();
	
	g_signal_connect((gpointer) ctrls->track_combo, "changed",
			G_CALLBACK(on_track_combo_changed), NULL);

	g_timeout_add(1000, cb_periodic_check, NULL);

	return main_window;
}

void main_window_show(void)
{
	if (main_window)
	{
		gtk_widget_show_all(main_window);
	}
	else
	{
		g_warning("main window not created");
	}
}

void main_window_hide(void)
{
	if (main_window)
	{
		gtk_widget_hide_all(main_window);
	}
	else
	{
		g_warning("main window not created");
	}
}

void main_window_toggle_visible(void)
{
	if (main_window)
	{
		if (GTK_WIDGET_VISIBLE(main_window))
		{
			gtk_widget_hide_all(main_window);
		}
		else
		{
			gtk_widget_show_all(main_window);
		}
	}
	else
	{
		g_warning("main window not created");
	}
}

