LIBRARY_VERSION = 0:0:0

lib_LTLIBRARIES = libgweather.la

libgweatherincdir = $(includedir)/libgweather
libgweatherinc_HEADERS = weather.h gweather-gconf.h gweather-prefs.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gweather.pc

libgweather_la_SOURCES = \
	weather.c weather.h weather-priv.h \
	weather-metar.c weather-iwin.c weather-met.c \
	weather-bom.c weather-wx.c \
	weather-sun.c \
	gweather-prefs.c gweather-prefs.h \
	gweather-gconf.c gweather-gconf.h

libgweather_la_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(srcdir)			\
	$(WARN_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(GNOME_VFS_APPLETS_CFLAGS)	\
	$(GNOME_APPLETS_CFLAGS)		\
	-DG_LOG_DOMAIN=\"GWeather\"	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

libgweather_la_LIBADD = \
	$(GTK_LIBS)	\
	$(GNOME_VFS_APPLETS_LIBS)

libgweather_la_LDFLAGS = \
	-version-info $(LIBRARY_VERSION) -no-undefined

test_metar_SOURCES = test_metar.c

test_metar_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(srcdir)			\
	$(WARN_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(GNOME_APPLETS_CFLAGS)		\
	$(GNOME_VFS_APPLETS_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GWeather\"

test_metar_LDADD = \
	$(GNOME_APPLETS_LIB) \
	$(GNOME_VFS_APPLETS_LIB) \
	libgweather.la

noinst_HEADERS = weather-priv.h
noinst_PROGRAMS = test_metar

schemasdir   = @GCONF_SCHEMA_FILE_DIR@
schemas_in_files = gweather.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schemas_DATA) ;
uninstall-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-uninstall-rule $(schemas_DATA) ;
endif

EXTRA_DIST = gweather.pc.in $(schemas_in_files)

EXTRA_PROGRAMS = test_metar

CLEANFILES = $(schemas_DATA) $(EXTRA_PROGRAMS)
