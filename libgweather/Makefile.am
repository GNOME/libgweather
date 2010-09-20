lib_LTLIBRARIES = libgweather-3.la
noinst_PROGRAMS = test_metar test_locations test_sun_moon

AM_CPPFLAGS = 			\
	$(GTK_CFLAGS)		\
	-I$(top_srcdir)		\
	-I$(srcdir)

AM_CFLAGS = $(WARN_CFLAGS)

gweather_old_headers = \
	weather.h gweather-gconf.h gweather-prefs.h gweather-xml.h
gweather_new_headers = \
	gweather-location.h location-entry.h \
	gweather-timezone.h timezone-menu.h
libgweatherincdir = $(includedir)/libgweather-3.0/libgweather
libgweatherinc_HEADERS = \
	$(gweather_old_headers)		\
	$(gweather_new_headers)		\
	gweather-enum-types.h

noinst_HEADERS = weather-priv.h gweather-win32.h

libgweather_3_la_SOURCES = \
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
libgweather_3_la_SOURCES += gweather-win32.c
else
EXTRA_libgweather_3_la_SOURCES = gweather-win32.c
endif

libgweather_3_la_CPPFLAGS = \
	$(AM_CPPFLAGS)			\
	$(LIBXML_CFLAGS)		\
	$(LIBSOUP_CFLAGS)		\
	$(GCONF_CFLAGS)			\
	-DG_LOG_DOMAIN=\"GWeather\"	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DGWEATHER_XML_LOCATION_DIR=\""$(pkgdatadir)"\"

libgweather_3_la_LIBADD = \
	-lm		\
	$(GTK_LIBS)	\
	$(LIBXML_LIBS)	\
	$(LIBSOUP_LIBS)	\
	$(GCONF_LIBS) \
	$(REGEX_LIBS)

libgweather_3_la_LDFLAGS = \
	-version-info $(LT_VERSION) -no-undefined

test_metar_SOURCES = test_metar.c
test_metar_CPPFLAGS = $(AM_CPPFLAGS) $(LIBSOUP_CFLAGS)
test_metar_LDADD = libgweather-3.la

test_locations_SOURCES = test_locations.c
test_locations_LDADD = libgweather-3.la

test_sun_moon_SOURCES = test_sun_moon.c
test_sun_moon_CPPFLAGS = $(AM_CPPFLAGS) $(LIBSOUP_CFLAGS)
test_sun_moon_LDADD = libgweather-3.la

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

BUILT_SOURCES = gweather-enum-types.c gweather-enum-types.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gweather-3.0.pc

schemadir = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = gweather.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST = gweather-3.0.pc.in gweather-3.0-uninstalled.pc.in $(schema_in_files)

CLEANFILES = $(schema_DATA) $(noinst_PROGRAMS) $(BUILT_SOURCES)

# Introspection
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = 	\
	weather.c weather.h \
	gweather-location.c gweather-location.h \
	gweather-timezone.c gweather-timezone.h \
	location-entry.c location-entry.h \
	timezone-menu.c timezone-menu.h \
	gweather-enum-types.h

GWeather-3.0.gir: libgweather-3.la Makefile
GWeather_3_0_gir_INCLUDES = GObject-2.0 Gtk-3.0
GWeather_3_0_gir_CFLAGS = $(libgweather_3_la_CPPFLAGS) -DGWEATHER_I_KNOW_THIS_IS_UNSTABLE
GWeather_3_0_gir_LIBS = libgweather-3.la
GWeather_3_0_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources))
INTROSPECTION_GIRS += GWeather-3.0.gir

girdir = $(INTROSPECTION_GIRDIR)
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(INTROSPECTION_TYPELIBDIR)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/libgweather/$$p ; \
		done \
	fi
uninstall-local:
	for p in $(schema_DATA) ; do \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-uninstall-rule $(top_builddir)/libgweather/$$p ; \
	done
endif

-include $(top_srcdir)/git.mk
