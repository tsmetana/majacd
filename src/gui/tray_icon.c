#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <gtk/gtk.h>

#include "tray_icon.h"
#include "backend_ctl.h"
#include "callbacks.h"
#include "main_window.h"

static void on_activate(GtkStatusIcon *icon, gpointer user_data)
{
	main_window_toggle_visible();
}

static void on_popup(GtkStatusIcon *icon, guint button,
		guint activate_time, gpointer user_data)
{
	GtkWidget *popup_menu = gtk_menu_new();
	GtkWidget *play_menu_item =
		gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_PLAY, NULL);
	GtkWidget *pause_menu_item =
		gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_PAUSE, NULL);
	GtkWidget *stop_menu_item =
		gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_STOP, NULL);
	GtkWidget *next_menu_item =
		gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_NEXT, NULL);
	GtkWidget *prev_menu_item =
		gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS, NULL);
	GtkWidget *random_menu_item =
		gtk_check_menu_item_new_with_label("Random");
	GtkWidget *shuffle_menu_item =
		gtk_check_menu_item_new_with_label("Shuffle");
	GtkWidget *separator =
		gtk_separator_menu_item_new();
	GtkWidget *quit_menu_item =
		gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
	backend_status_t status;
	
	status.state = STATUS_OTHER;

	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), play_menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), pause_menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), stop_menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), next_menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), prev_menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), random_menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), shuffle_menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), quit_menu_item);
	
	g_signal_connect((gpointer) play_menu_item, "activate",
			G_CALLBACK(on_play_activate), NULL);
	g_signal_connect((gpointer) pause_menu_item, "activate",
			G_CALLBACK(on_pause_activate), NULL);
	g_signal_connect((gpointer) stop_menu_item, "activate",
			G_CALLBACK(on_stop_activate), NULL);
	g_signal_connect((gpointer) next_menu_item, "activate",
			G_CALLBACK(on_next_activate), NULL);
	g_signal_connect((gpointer) prev_menu_item, "activate",
			G_CALLBACK(on_prev_activate), NULL);
	g_signal_connect((gpointer) random_menu_item, "toggled",
			G_CALLBACK(on_random_toggled), NULL);
	g_signal_connect((gpointer) shuffle_menu_item, "toggled",
			G_CALLBACK(on_shuffle_toggled), NULL);
	g_signal_connect((gpointer) quit_menu_item, "activate",
			G_CALLBACK(on_quit_activate), NULL);

	backend_ctl_command(CMD_STATUS, (gpointer) &status);

	if (status.state == STATUS_OTHER)
	{
		gtk_widget_set_sensitive(play_menu_item, FALSE);
		gtk_widget_set_sensitive(pause_menu_item, FALSE);
		gtk_widget_set_sensitive(stop_menu_item, FALSE);
		gtk_widget_set_sensitive(next_menu_item, FALSE);
		gtk_widget_set_sensitive(prev_menu_item, FALSE);
		gtk_widget_set_sensitive(random_menu_item, FALSE);
		gtk_widget_set_sensitive(shuffle_menu_item, FALSE);
	}

	gtk_widget_show_all(GTK_WIDGET(popup_menu));
	gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
			button, activate_time);
}

void tray_icon_create(void)
{
	GtkStatusIcon *tray_icon;

	tray_icon = gtk_status_icon_new_from_stock(GTK_STOCK_CDROM);
	gtk_status_icon_set_visible(tray_icon, TRUE);

	g_signal_connect((gpointer) tray_icon, "activate",
			G_CALLBACK(on_activate), NULL);
	g_signal_connect((gpointer) tray_icon, "popup-menu",
			G_CALLBACK(on_popup), NULL);
}

