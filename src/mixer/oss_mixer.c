/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../output_api.h"
#include "../mixer_api.h"

#include <glib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <soundcard.h>
#else /* !(defined(__OpenBSD__) || defined(__NetBSD__) */
# include <sys/soundcard.h>
#endif /* !(defined(__OpenBSD__) || defined(__NetBSD__) */

#define VOLUME_MIXER_OSS_DEFAULT		"/dev/mixer"

struct oss_mixer {
	/** the base mixer class */
	struct mixer base;

	char *device;
	char *control;
	int device_fd;
	int volume_control;
};

static struct mixer *
oss_mixer_init(const struct config_param *param)
{
	struct oss_mixer *om = g_new(struct oss_mixer, 1);

	mixer_init(&om->base, &oss_mixer);

	om->device = config_dup_block_string(param, "mixer_device", NULL);
	om->control = config_dup_block_string(param, "mixer_control", NULL);

	om->device_fd = -1;
	om->volume_control = SOUND_MIXER_PCM;

	return &om->base;
}

static void
oss_mixer_finish(struct mixer *data)
{
	struct oss_mixer *om = (struct oss_mixer *) data;

	g_free(om->device);
	g_free(om->control);
	g_free(om);
}

static void
oss_mixer_close(struct mixer *data)
{
	struct oss_mixer *om = (struct oss_mixer *) data;
	if (om->device_fd != -1)
		while (close(om->device_fd) && errno == EINTR) ;
	om->device_fd = -1;
}

static int
oss_find_mixer(const char *name)
{
	const char *labels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
	size_t name_length = strlen(name);

	for (unsigned i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (strncasecmp(name, labels[i], name_length) == 0 &&
		    (labels[i][name_length] == 0 ||
		     labels[i][name_length] == ' '))
			return i;
	}
	return -1;
}

static bool
oss_mixer_open(struct mixer *data)
{
	struct oss_mixer *om = (struct oss_mixer *) data;
	const char *device = VOLUME_MIXER_OSS_DEFAULT;

	if (om->device) {
		device = om->device;
	}

	if ((om->device_fd = open(device, O_RDONLY)) < 0) {
		g_warning("Unable to open oss mixer \"%s\"\n", device);
		return false;
	}

	if (om->control) {
		int i;
		int devmask = 0;

		if (ioctl(om->device_fd, SOUND_MIXER_READ_DEVMASK, &devmask) < 0) {
			g_warning("errors getting read_devmask for oss mixer\n");
			oss_mixer_close(data);
			return false;
		}
		i = oss_find_mixer(om->control);

		if (i < 0) {
			g_warning("mixer control \"%s\" not found\n",
				om->control);
			oss_mixer_close(data);
			return false;
		} else if (!((1 << i) & devmask)) {
			g_warning("mixer control \"%s\" not usable\n",
				om->control);
			oss_mixer_close(data);
			return false;
		}
		om->volume_control = i;
	}
	return true;
}

static bool
oss_mixer_control(struct mixer *data, int cmd, void *arg)
{
	struct oss_mixer *om = (struct oss_mixer *) data;
	switch (cmd) {
	case AC_MIXER_GETVOL:
	{
		int left, right, level;
		int *ret;

		if (om->device_fd < 0 && !oss_mixer_open(data)) {
			return false;
		}

		if (ioctl(om->device_fd, MIXER_READ(om->volume_control), &level) < 0) {
			oss_mixer_close(data);
			g_warning("unable to read oss volume\n");
			return false;
		}

		left = level & 0xff;
		right = (level & 0xff00) >> 8;

		if (left != right) {
			g_warning("volume for left and right is not the same, \"%i\" and "
				"\"%i\"\n", left, right);
		}
		ret = (int *) arg;
		*ret = left;
		return true;
	}
	case AC_MIXER_SETVOL:
	{
		int new;
		int level;
		int *value = arg;

		if (om->device_fd < 0 && !oss_mixer_open(data)) {
			return false;
		}

		new = *value;
		if (new < 0) {
			new = 0;
		} else if (new > 100) {
			new = 100;
		}

		level = (new << 8) + new;

		if (ioctl(om->device_fd, MIXER_WRITE(om->volume_control), &level) < 0) {
			g_warning("unable to set oss volume\n");
			oss_mixer_close(data);
			return false;
		}
		return true;
	}
	default:
		g_warning("Unsuported oss control\n");
		break;
	}
	return false;
}

const struct mixer_plugin oss_mixer = {
	.init = oss_mixer_init,
	.finish = oss_mixer_finish,
	.open = oss_mixer_open,
	.control = oss_mixer_control,
	.close = oss_mixer_close
};
