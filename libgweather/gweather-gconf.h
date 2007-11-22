/*
 * gweather-gconf.h: GConf interaction methods for gweather.
 *
 * Copyright (C) 2005 Philip Langdale, Papadimitriou Spiros
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *     Philip Langdale <philipl@mail.utexas.edu>
 *     Papadimitriou Spiros <spapadim+@cs.cmu.edu>
 */

#ifndef __GWEATHER_GCONF_WRAPPER_H__
#define __GWEATHER_GCONF_WRAPPER_H__

#include <glib/gmacros.h>
#include <glib/gerror.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>

#include <libgweather/weather.h>

G_BEGIN_DECLS

typedef struct		_GWeatherGConf			GWeatherGConf;

GWeatherGConf *		gweather_gconf_new		(const char *prefix);
void			gweather_gconf_free		(GWeatherGConf *ctx);

GConfClient *		gweather_gconf_get_client	(GWeatherGConf *ctx);

WeatherLocation *	gweather_gconf_get_location	(GWeatherGConf *ctx);

gchar *			gweather_gconf_get_full_key	(GWeatherGConf *ctx,
							 const gchar *key);

void			gweather_gconf_set_bool		(GWeatherGConf *ctx,
							 const gchar *key,
							 gboolean the_bool,
							 GError **opt_error);
void			gweather_gconf_set_int		(GWeatherGConf *ctx,
							 const gchar *key,
							 gint the_int,
							 GError **opt_error);
void			gweather_gconf_set_string	(GWeatherGConf *ctx,
							 const gchar *key,
							 const gchar *the_string,
							 GError **opt_error);

gboolean		gweather_gconf_get_bool		(GWeatherGConf *ctx,
							 const gchar *key,
							 GError **opt_error);
gint			gweather_gconf_get_int		(GWeatherGConf *ctx,
							 const gchar *key,
							 GError **opt_error);
gchar *			gweather_gconf_get_string	(GWeatherGConf *ctx,
							 const gchar *key,
							 GError **opt_error);

G_END_DECLS

#endif /* __GWEATHER_GCONF_WRAPPER_H__ */
