#ifndef _DEVICE_CTL_H
#define _DEVICE_CTL_H

enum _dev_status_e {
	ST_INVALID = 0,
	ST_PLAYING,
	ST_PAUSED,
	ST_COMPLETED,
	ST_ERROR,
	ST_NONE,
	NUM_ST
};

typedef enum _dev_status_e dev_status_t;

/*
 * If not explicitly stated otherwise all the functions return
 * negative value on error; non-negative (>= 0) on success
 * Tracks are numbered from 1
 */

/* Opens a device at path dev_path; must be called first */
int dev_open(const char *dev_path);
/* Closes the opened device */
int dev_close(void);
/* Initializes the opened device; must be called
 * before any other of the following functions */
int dev_prepare(void);
/* Returns 1 if the prepared device contains audio disc 0 otherwise */
int dev_isaudio(void);
/* Returns the number of tracks */
int dev_get_track_num(void);
/* For given track trk returns the start in minutes and seconds from the
 * beginning of CD and tracks length; any of the pointers can be NULL
 * which means "don't care" */
int dev_get_track_time(int trk, unsigned *st_min, unsigned *st_sec,
		unsigned *len_min, unsigned *len_sec);
/* Starts playback from the given track */
int dev_play(const int track);
/* Stops playback */
int dev_stop(void);
/* Pauses playback */
int dev_pause(void);
/* Resumes paused playback */
int dev_resume(void);
/* Ejects the device */
int dev_eject(void);
/* Closes the tray */
int dev_close_tray(void);
/* Retrieves the status, current track and position from
 * the beginning of the track; any of the pointers can be NULL */
int dev_query_status(dev_status_t *status, int *track,
		unsigned *min, unsigned *sec);
/* Returns the current volume setting */
int dev_get_volume(void);
/* Sets the volume */
int dev_set_volume(int vol);
/* Seeks to given time (absolute from the beginning of CD) */
int dev_seek(int sec);

#endif
