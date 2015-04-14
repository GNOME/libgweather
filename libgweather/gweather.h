/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather.h
 *
 * Copyright 2008, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __GWEATHER_H__
#define __GWEATHER_H__

#if !(defined(GWEATHER_COMPILATION) || defined(GWEATHER_I_KNOW_THIS_IS_UNSTABLE))
#error "libgweather should only be used if you understand that it's subject to change, and is not supported as a fixed API/ABI or as part of the platform"
#endif

#define IN_GWEATHER_H

#include <libgweather/gweather-version.h>
#include <libgweather/gweather-enums.h>
#include <libgweather/gweather-enum-types.h>
#include <libgweather/gweather-location.h>
#include <libgweather/gweather-timezone.h>
#include <libgweather/gweather-weather.h>
#include <libgweather/gweather-location-entry.h>
#include <libgweather/gweather-timezone-menu.h>

#undef IN_GWEATHER_H

#endif /* __GWEATHER_H__ */
