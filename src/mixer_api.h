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

#ifndef MPD_MIXER_H
#define MPD_MIXER_H

#include "conf.h"

/*
 * list of currently implemented mixers
 */

extern const struct mixer_plugin alsa_mixer;
extern const struct mixer_plugin oss_mixer;

struct mixer_plugin {
	/**
         * Alocates and configures a mixer device.
	 */
	struct mixer *(*init)(const struct config_param *param);

        /**
	 * Finish and free mixer data
         */
        void (*finish)(struct mixer *data);

        /**
    	 * Open mixer device
	 */
	bool (*open)(struct mixer *data);

        /**
	 * Control mixer device.
         */
	bool (*control)(struct mixer *data, int cmd, void *arg);

        /**
    	 * Close mixer device
	 */
	void (*close)(struct mixer *data);
};

struct mixer {
	const struct mixer_plugin *plugin;
};

static inline void
mixer_init(struct mixer *mixer, const struct mixer_plugin *plugin)
{
	mixer->plugin = plugin;
}

struct mixer *
mixer_new(const struct mixer_plugin *plugin, const struct config_param *param);

void
mixer_free(struct mixer *mixer);

bool mixer_open(struct mixer *mixer);
bool mixer_control(struct mixer *mixer, int cmd, void *arg);
void mixer_close(struct mixer *mixer);

#endif
