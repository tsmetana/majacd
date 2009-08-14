#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdlib.h>

#include "backend_ctl.h"
#include "tray_icon.h"
#include "main_window.h"

#define MAJACD_APP "majacd"

int main(int argc, char **argv)
{
	gchar *backend = NULL;
	gchar *backend_opt = NULL;
	gchar *majacd_app = NULL;
	GOptionEntry entries[] =
	{
		{"backend", 'b', 0, G_OPTION_ARG_STRING, &backend_opt,
			"Path to majacd application", "<path>"},
		{NULL}
	};
	GError *err = NULL;
	GOptionContext *context;
	GtkWidget *err_dialog;

	context = g_option_context_new("- system tray icon to "
			"control the majacd application");
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_parse(context, &argc, &argv, &err);
	g_option_context_free(context);

	if (backend_opt)
	{
		backend = g_strdup(backend_opt);
	}
	else
	{
		backend = g_strdup(MAJACD_APP);
	}

	gtk_init(&argc, &argv);
	if (!(majacd_app = g_find_program_in_path(backend)))
	{
		err_dialog = gtk_message_dialog_new(NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"Could not locate the majacd application");
		gtk_window_set_title(GTK_WINDOW(err_dialog), "Error");
		gtk_dialog_run(GTK_DIALOG(err_dialog));
		gtk_widget_destroy(err_dialog);
		g_error("Could not locate the majacd application\n");
		/* NOT REACHED */
		return EXIT_FAILURE;
	}
	if (backend_ctl_command(CMD_INIT, (gpointer) majacd_app) < 0)
	{
		g_error("Error starting %s", majacd_app);
	}
	main_window_create();
	main_window_hide();
	tray_icon_create();
	gtk_main();

	g_free(backend);
	g_free(majacd_app);
	
	return EXIT_SUCCESS;
}

