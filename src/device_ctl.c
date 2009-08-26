/*
 * Maja's CD Player
 * Copyright (C) 2008-2009 Tomas Smetana
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <device_ctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#include <stdio.h>

enum _track_e {
	TRACK_AUDIO = 0,
	TRACK_DATA,
	TRACK_INVALID
};

typedef enum _track_e track_t;

struct _dev_track_s {
	unsigned start_min;
	unsigned start_sec;
	unsigned len_min;
	int len_sec;
	track_t type;
};

typedef struct _dev_track_s dev_track_t;

struct _dev_disc_s {
	int fd;
	unsigned int track_num;
	int is_audio;
	dev_track_t *track_list;
};

typedef struct _dev_disc_s dev_disc_t;

/* globals */

static dev_disc_t dev = {
	.fd = -1,
	.track_num = 0,
	.is_audio = 0,
	.track_list = NULL
};

#define CHECK_FD(__fd) \
	if (__fd < 0) \
		return -1;

int dev_open(const char *dev_path)
{
	dev.fd = open(dev_path, O_RDONLY | O_NONBLOCK);
	
	return dev.fd >= 0 ? dev.fd : 0;
}

int dev_prepare(void)
{
	struct cdrom_tochdr toc;
	struct cdrom_tocentry trk_info;
	int ret;
	unsigned i;
	__u8 status;

	status = ioctl(dev.fd, CDROM_DRIVE_STATUS);
	if (status != CDS_DISC_OK)
		return -1;
	status = ioctl(dev.fd, CDROM_DISC_STATUS);
	dev.is_audio = (status == CDS_AUDIO);

	if ((ret = ioctl(dev.fd, CDROMREADTOCHDR, &toc)) < 0)
		return ret;
	dev.track_num = toc.cdth_trk1 - toc.cdth_trk0 + 1;
	dev.track_list = calloc(dev.track_num, sizeof(dev_track_t));
	if (!dev.track_list)
		return -1;
	trk_info.cdte_format = CDROM_MSF;
	for (i = 0; i < dev.track_num; i++) {
		trk_info.cdte_track = i + 1;
		ret = ioctl(dev.fd, CDROMREADTOCENTRY, &trk_info);
		if (ret < 0) {
			dev.track_list[i].type = TRACK_INVALID;
			continue;
		}
		if (trk_info.cdte_ctrl == CDROM_DATA_TRACK) {
			dev.track_list[i].type = TRACK_DATA;
			continue;
		}
		dev.track_list[i].type = TRACK_AUDIO;
		dev.track_list[i].start_min = trk_info.cdte_addr.msf.minute;
		dev.track_list[i].start_sec = trk_info.cdte_addr.msf.second;
	}
	trk_info.cdte_track = CDROM_LEADOUT;
	ret = ioctl(dev.fd, CDROMREADTOCENTRY, &trk_info);
	if (ret < 0)
		return ret;
	i = dev.track_num - 1;
	dev.track_list[i].len_min =
		trk_info.cdte_addr.msf.minute - dev.track_list[i].start_min;
	dev.track_list[i].len_sec =
		trk_info.cdte_addr.msf.second - dev.track_list[i].start_sec;
	if (dev.track_list[i].len_sec < 0) {
		--dev.track_list[i].len_min;
		dev.track_list[i].len_sec += CD_SECS;
	}
	for (i = 0; i < dev.track_num - 1; i++) {
		dev.track_list[i].len_min =
			dev.track_list[i + 1].start_min - dev.track_list[i].start_min;
		dev.track_list[i].len_sec =
			dev.track_list[i + 1].start_sec - dev.track_list[i].start_sec;
		if (dev.track_list[i].len_sec < 0) {
			--dev.track_list[i].len_min;
			dev.track_list[i].len_sec += CD_SECS;
		}
	}
	return 0;
}

int dev_isaudio(void)
{
	return dev.is_audio;
}

int dev_get_track_num(void)
{
	return dev.track_num;
}

int dev_get_track_time(int trk, unsigned *st_min, unsigned *st_sec,
		unsigned *len_min, unsigned *len_sec)
{
	CHECK_FD(dev.fd);
	
	if (trk < 1)
		trk = 1;
	if (trk > dev.track_num)
		trk = dev.track_num;
	--trk;
	if (st_min)
		*st_min = dev.track_list[trk].start_min;
	if (st_sec)
		*st_sec = dev.track_list[trk].start_sec;
	if (len_min)
		*len_min = dev.track_list[trk].len_min;
	if (len_sec)
		*len_sec = dev.track_list[trk].len_sec;
	return 0;
}

int dev_close(void)
{
	int ret;

	CHECK_FD(dev.fd);
	ret = close(dev.fd);
	dev.fd = -1;
	dev.track_num = 0;
	dev.is_audio = 0;
	free(dev.track_list);
	dev.track_list = NULL;
	return ret;
}

int dev_play_msf(unsigned min, unsigned sec)
{
	int ret;
	struct cdrom_msf msf;

	CHECK_FD(dev.fd);
	ret = ioctl(dev.fd, CDROMSTART);
	if (ret < 0)
		return ret;
	msf.cdmsf_min0 = min;
	msf.cdmsf_sec0 = sec;
	msf.cdmsf_frame0 = (__u8) 0U;
	msf.cdmsf_min1 = dev.track_list[dev.track_num - 1].start_min +
		dev.track_list[dev.track_num - 1].start_min;
	msf.cdmsf_sec1 = dev.track_list[dev.track_num - 1].start_sec +
		dev.track_list[dev.track_num - 1].start_sec;
	msf.cdmsf_frame1 = (__u8) 0U;

	ret = ioctl(dev.fd, CDROMPLAYMSF, &msf);

	return ret;
}

int dev_play(const int track)
{
	int ret;
	struct cdrom_ti track_idx;

	CHECK_FD(dev.fd);

	track_idx.cdti_trk0 = track;
	track_idx.cdti_trk1 = CDROM_LEADOUT;
	track_idx.cdti_ind0 = track_idx.cdti_ind1 = 0;
	ret = ioctl(dev.fd, CDROMSTART);
	if (ret < 0)
		return ret;
	ret = ioctl(dev.fd, CDROMPLAYTRKIND, &track_idx);

	return ret;
}

int dev_stop(void)
{
	CHECK_FD(dev.fd);
	return ioctl(dev.fd, CDROMSTOP);
}

int dev_pause(void)
{
	CHECK_FD(dev.fd);
	return ioctl(dev.fd, CDROMPAUSE);
}

int dev_resume(void)
{
	CHECK_FD(dev.fd);
	return ioctl(dev.fd, CDROMRESUME);
}

int dev_eject(void)
{
	CHECK_FD(dev.fd);
	if (dev_stop() < 0)
		return -1;
	return ioctl(dev.fd, CDROMEJECT);
}

int dev_close_tray(void)
{
	CHECK_FD(dev.fd);
	return ioctl(dev.fd, CDROMCLOSETRAY);
}

int dev_query_status(dev_status_t *status, int *track,
		unsigned *min, unsigned *sec)
{
	struct cdrom_subchnl sc_info;
	int ret;
	
	CHECK_FD(dev.fd);
	sc_info.cdsc_format = CDROM_MSF;
	ret = ioctl(dev.fd, CDROMSUBCHNL, &sc_info);
	if (ret < 0)
		return ret;

	if (status)
		switch (sc_info.cdsc_audiostatus) {
			case CDROM_AUDIO_INVALID:
				*status = ST_INVALID;
				break;
			case CDROM_AUDIO_PLAY:
				*status = ST_PLAYING;
				break;
			case CDROM_AUDIO_PAUSED:
				*status = ST_PAUSED;
				break;
			case CDROM_AUDIO_COMPLETED:
				*status = ST_COMPLETED;
				break;
			case CDROM_AUDIO_ERROR:
				*status = ST_ERROR;
				break;
			case CDROM_AUDIO_NO_STATUS:
				*status = ST_NONE;
			default:
				break;
		}
	if (track)
		*track = sc_info.cdsc_trk;
	if (min)
		*min = (unsigned)sc_info.cdsc_reladdr.msf.minute;
	if (sec)
		*sec = (unsigned)sc_info.cdsc_reladdr.msf.second;

	return 0;
}

int dev_get_volume(void)
{
	struct cdrom_volctrl volctrl;
	int ret;

	CHECK_FD(dev.fd);
	ret = ioctl(dev.fd, CDROMVOLREAD, &volctrl);
	if (ret < 0)
		return ret;

	/* FIXME: this is a bit rough assumption... */
	return volctrl.channel0;
}

int dev_set_volume(int vol)
{
	struct cdrom_volctrl volctrl;
	int ret;

	CHECK_FD(dev.fd);

	volctrl.channel0 = vol;
	volctrl.channel1 = vol;
	volctrl.channel2 = vol;
	volctrl.channel3 = vol;

	ret = ioctl(dev.fd, CDROMVOLCTRL, &volctrl);

	return ret;
}

int dev_seek(int sec)
{
	/* Absolute time */
	ldiv_t seek_time = ldiv((long)sec, (long)CD_SECS);
	dev_status_t status;

	CHECK_FD(dev.fd);
	if (dev_query_status(&status, NULL, NULL, NULL) < 0)
		return -1;
	if (status == ST_PLAYING)
		dev_play_msf((unsigned)seek_time.quot, (unsigned)seek_time.rem);
	return 0;
}

