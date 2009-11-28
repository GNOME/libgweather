lib_LTLIBRARIES = libgweather.la

libgweatherincdir = $(includedir)/libgweather
gweather_old_headers = \
	weather.h gweather-gconf.h gweather-prefs.h gweather-xml.h
gweather_new_headers = \
	gweather-location.h location-entry.h \
	gweather-timezone.h timezone-menu.h
libgweatherinc_HEADERS = \
	$(gweather_old_headers)		\
	$(gweather_new_headers)		\
	gweather-enum-types.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gweather.pc

libgweather_la_SOURCES = \
	weather.c weather.h weather-priv.h \
	weather-metar.c weather-iwin.c weather-met.c \
	weather-bom.c weather-wx.c \
	weather-sun.c weather-moon.c \
	gweather-enum-types.c \
	gweather-prefs.c gweather-prefs.h \
	gweather-gconf.c gweather-gconf.h \
	gweather-xml.c gweather-xml.h \
	gweather-location.c gweather-location.h \
	gweather-timezone.c gweather-timezone.h \
	gweather-win32.h \
	location-entry.c location-entry.h \
	timezone-menu.c timezone-menu.h \
	parser.c parser.h

if OS_WIN32
libgweather_la_SOURCES += gweather-win32.c
else
EXTRA_libgweather_la_SOURCES = gweather-win32.c
endif

libgweather_la_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(srcdir)			\
	$(WARN_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(LIBXML_CFLAGS)		\
	$(LIBSOUP_CFLAGS)		\
	$(GCONF_CFLAGS)			\
	-DG_LOG_DOMAIN=\"GWeather\"	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DGWEATHER_XML_LOCATION_DIR=\""$(pkgdatadir)"\"

libgweather_la_LIBADD = \
	-lm		\
	$(GTK_LIBS)	\
	$(LIBXML_LIBS)	\
	$(LIBSOUP_LIBS)	\
	$(GCONF_LIBS) \
	$(REGEX_LIBS)

libgweather_la_LDFLAGS = \
	-version-info $(LT_VERSION) -no-undefined

MKENUMS_GENERATED = gweather-enum-types.c gweather-enum-types.h

gweather-enum-types.h: $(gweather_new_headers)
	$(AM_V_GEN)( cd $(srcdir) && $(GLIB_MKENUMS) --template gweather-enum-types.h.tmpl \
		$(gweather_new_headers) ) > gweather-enum-types.h.tmp \
	&& mv gweather-enum-types.h.tmp gweather-enum-types.h \
	|| rm -f gweather-enum-type.h.tmp

gweather-enum-types.c: $(libgweatherinclude_HEADERS)
	$(AM_V_GEN)( cd $(srcdir) && $(GLIB_MKENUMS) --template gweather-enum-types.c.tmpl \
		$(gweather_new_headers) ) > gweather-enum-types.c.tmp \
	&& mv gweather-enum-types.c.tmp gweather-enum-types.c \
	|| rm -f gweather-enum-type.c.tmp

BUILT_SOURCES = $(MKENUMS_GENERATED)

test_metar_SOURCES = test_metar.c

test_metar_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(srcdir)			\
	$(WARN_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(LIBSOUP_CFLAGS)		\
	-DG_LOG_DOMAIN=\"GWeather\"

test_metar_LDADD = \
	$(LIBSOUP_LIBS)	\
	$(REGEX_LIBS) \
	libgweather.la

test_locations_SOURCES = test_locations.c

test_locations_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(srcdir)			\
	$(WARN_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(GNOME_VFS_APPLETS_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GWeather\"

test_locations_LDADD = libgweather.la

test_sun_moon_SOURCES = \
	test_sun_moon.c

test_sun_moon_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(srcdir)			\
	$(WARN_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(LIBSOUP_CFLAGS)		\
	-DG_LOG_DOMAIN=\"GWeather\"

test_sun_moon_LDADD = \
	$(LIBSOUP_LIBS)	\
	libgweather.la

noinst_HEADERS = weather-priv.h gweather-win32.h
noinst_PROGRAMS = test_metar test_locations test_sun_moon

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = gweather.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p ; \
		done \
	fi
uninstall-local:
	for p in $(schema_DATA) ; do \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-uninstall-rule $$p ; \
	done
endif

EXTRA_DIST = gweather.pc.in gweather-uninstalled.pc.in $(schema_in_files)

EXTRA_PROGRAMS = test_metar test_sun_moon

CLEANFILES = $(schema_DATA) $(EXTRA_PROGRAMS) $(MKENUMS_GENERATED)

-include $(top_srcdir)/git.mk
