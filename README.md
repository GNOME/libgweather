[![Build Status](https://gitlab.gnome.org/GNOME/libgweather/badges/master/build.svg)](https://gitlab.gnome.org/GNOME/libgweather/pipelines)

libgweather
===========

libgweather is a library to access weather information from online
services for numerous locations.

libgweather isn't supported in the devel platform, which means OS vendors
won't guarantee the API/ABI long-term, but authors of open source apps
should feel free to use libgweather as users can always recompile against
a new version.

To use libgweather in your code, you need to define the
`GWEATHER_I_KNOW_THIS_IS_UNSTABLE` preprecessor symbol, e.g. by adding
`-DGWEATHER_I_KNOW_THIS_IS_UNSTABLE` to your `CFLAGS`.

Documentation for the API is available with gtk-doc, and online:
   https://developer.gnome.org/libgweather/stable/

You may download updates to the package from:

   https://download.gnome.org/sources/libgweather/

To discuss libgweather, you may use the desktop-devel-list mailing list:

  https://mail.gnome.org/mailman/listinfo/desktop-devel-list


How to report bugs
==================

Bugs should be reported to the libgweather issues tracking system:

   https://gitlab.gnome.org/GNOME/libgweather/issues

You might need to create an account for yourself.

Please read the following page on how to prepare a useful bug report:

   https://wiki.gnome.org/Community/GettingInTouch/BugReportingGuidelines

If you are interested in adding new locations (weather stations, or
cities), please refer to this page in the GNOME Wiki:
   https://wiki.gnome.org/Projects/LibGWeather/ImprovingLocations

Please read the HACKING file for information on the internal architecture
of libgweather.
