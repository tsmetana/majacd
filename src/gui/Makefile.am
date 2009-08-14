INCLUDES = \
	$(GTK_CFLAGS)
	
AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = majacd-gui

majacd_gui_SOURCES = \
	main.c \
	tray_icon.c \
	tray_icon.h \
	backend_ctl.c \
	backend_ctl.h \
	main_window.c \
	main_window.h \
	callbacks.c \
	callbacks.h

majacd_gui_LDFLAGS = 

majacd_gui_LDADD =  \
	$(GTK_LIBS)

