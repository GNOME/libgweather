lib_LTLIBRARIES = libgweather.la

libgweatherincdir = $(includedir)/libgweather
libgweatherinc_HEADERS = weather.h gweather-gconf.h gweather-prefs.h gweather-xml.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gweather.pc

libgweather_la_SOURCES = \
	weather.c weather.h weather-priv.h \
	weather-metar.c weather-iwin.c weather-met.c \
	weather-bom.c weather-wx.c \
	weather-sun.c \
	gweather-prefs.c gweather-prefs.h \
	gweather-gconf.c gweather-gconf.h \
	gweather-xml.c gweather-xml.h

libgweather_la_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(srcdir)			\
	$(WARN_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(LIBXML_CFLAGS)		\
	$(GNOME_VFS_APPLETS_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GWeather\"	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DGWEATHER_XML_LOCATION_DIR=\""$(pkgdatadir)"\"

libgweather_la_LIBADD = \
	$(GTK_LIBS)	\
	$(LIBXML_LIBS)	\
	$(GNOME_VFS_APPLETS_LIBS)

libgweather_la_LDFLAGS = \
	-version-info $(LT_VERSION) -no-undefined

test_metar_SOURCES = test_metar.c

test_metar_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(srcdir)			\
	$(WARN_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(GNOME_VFS_APPLETS_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GWeather\"

test_metar_LDADD = \
	$(GNOME_VFS_APPLETS_LIB) \
	libgweather.la

noinst_HEADERS = weather-priv.h
noinst_PROGRAMS = test_metar

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

EXTRA_DIST = gweather.pc.in $(schema_in_files)

EXTRA_PROGRAMS = test_metar

CLEANFILES = $(schema_DATA) $(EXTRA_PROGRAMS)
