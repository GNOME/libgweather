/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather.h
 *
 * SPDX-FileCopyrightText: 2008, Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

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
