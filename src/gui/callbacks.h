#ifndef _CALLBACKS_H
#define _CALLBACKS_H

struct _cb_controls_t
{
	GtkWidget *device_combo;
	GtkWidget *track_combo;
	GtkWidget *pos_slider;
	GtkWidget *vol_slider;
	GtkToolItem *play_btn;
	GtkToolItem *pause_btn;
	GtkToolItem *stop_btn;
	GtkToolItem *prev_btn;
	GtkToolItem *next_btn;
/*	GtkToolItem *eject_btn;*/
};

typedef struct _cb_controls_t cb_controls_t;

cb_controls_t *cb_get_controls(void);
void cb_refresh_controls(void);
gboolean cb_periodic_check(gpointer data);
void on_play_activate(GtkWidget *menuitem, gpointer user_data);
void on_pause_activate(GtkWidget *menuitem, gpointer user_data);
void on_next_activate(GtkWidget *menuitem, gpointer user_data);
void on_prev_activate(GtkWidget *menuitem, gpointer user_data);
void on_stop_activate(GtkWidget *menuitem, gpointer user_data);
void on_random_toggled(GtkWidget *menuitem, gpointer user_data);
void on_shuffle_toggled(GtkWidget *menuitem, gpointer user_data);
void on_quit_activate(GtkWidget *menuitem, gpointer user_data);
gchar *on_pos_scale_format(GtkScale *scale, gdouble arg1, gpointer user_data);
gboolean on_pos_scale_button_release(GtkWidget *pos_scale,
		GdkEventButton *event, gpointer user_data);
gboolean on_vol_scale_button_release(GtkWidget *pos_scale,
		GdkEventButton *event, gpointer user_data);
void on_track_combo_changed(GtkComboBox *combo, gpointer user_data);
void on_device_combo_changed(GtkComboBox *combo, gpointer user_data);

#endif
