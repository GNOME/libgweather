AC_INIT([libgweather],
        [3.8.2],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=libgweather],
        [libgweather],
        [http://live.gnome.org/LibGWeather])
AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LT_VERSION=3:3:0
AC_SUBST(LT_VERSION)

GTK_REQUIRED=2.90.0
GLIB_REQUIRED=2.35.1
LIBSOUP_REQUIRED=2.34.0
LIBXML_REQUIRED=2.6.0

LT_PREREQ([2.2.6])
LT_INIT([disable-static])
LT_LIB_M

PKG_PROG_PKG_CONFIG([0.19])

AC_PROG_CC
AM_PROG_CC_C_O

GLIB_GSETTINGS

AC_ARG_ENABLE(all-translations-in-one-xml,
        [AS_HELP_STRING([--enable-all-translations-in-one-xml],
                        [Put all translations in a big Locations.xml file (slow to parse)])],
        [enable_big_xml=yes],
        [enable_big_xml=no])
AM_CONDITIONAL(USE_ONE_BIG_XML, test "x$enable_big_xml" = "xyes")

AC_ARG_ENABLE(locations-compression,
        [AS_HELP_STRING([--enable-locations-compression],
                        [Compress Locations.xml files])],
        [enable_locations_compression=yes],
        [enable_locations_compression=no])
if test "x$enable_locations_compression" = "xyes"; then
    COMPRESS_EXT=.gz
else
    COMPRESS_EXT=
fi
AC_SUBST(COMPRESS_EXT)

GNOME_COMPILE_WARNINGS([maximum])

dnl -- Check for GTK+ 3.0 (required) ------------------------------------------
PKG_CHECK_MODULES(GTK, gtk+-3.0 >= $GTK_REQUIRED)

dnl -- Check for libxml (required) ------------------------------------------
PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= $LIBXML_REQUIRED)

dnl -- check for libsoup (required) -----------------------------------------
PKG_CHECK_MODULES(LIBSOUP, [libsoup-2.4 >= $LIBSOUP_REQUIRED])

dnl -- check for glib and gio (required) -----------------------------------------
PKG_CHECK_MODULES(GIO,
		  [glib-2.0 >= $GLIB_REQUIRED gio-2.0 >= $GLIB_REQUIRED])

dnl -- check for glib; redundant at this point, but sets $GLIB_MKENUMS
AM_PATH_GLIB_2_0($GLIB_REQUIRED)

dnl gettext
IT_PROG_INTLTOOL([0.50.0])

AM_GNU_GETTEXT_VERSION([0.18])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=libgweather-3.0
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext Package])

dnl ***************************************************************************
dnl ***  REGEX                                                              ***
dnl ***************************************************************************
AC_CHECK_FUNCS(regexec,,[AC_CHECK_LIB(regex,regexec,
               [REGEX_LIBS=-lregex],
               [AC_MSG_ERROR([No regex library found])])])
AC_SUBST(REGEX_LIBS)


dnl ***************************************************************************
dnl *** Check for presence of tm.tm_gmtoff on the system                    ***
dnl ***************************************************************************
AC_CHECK_MEMBER([struct tm.tm_gmtoff],[struct_tm_tm_gmtoff=true],[struct_tm_tm_gmtoff=false],[#include <time.h>])
case $struct_tm_tm_gmtoff in
        true)
                AC_DEFINE_UNQUOTED(HAVE_TM_TM_GMOFF, 1, Have the tm.tm_gmtoff member.)
                ;;
        *)
                AC_CHECK_DECL(timezone,AC_DEFINE(HAVE_TIMEZONE,1,Have timezone),,
[
#include <time.h>
])
                ;;
esac

dnl ***************************************************************************
dnl *** _NL_MEASUREMENT_MEASUREMENT is an enum and not a define             ***
dnl ***************************************************************************
AC_MSG_CHECKING([for _NL_MEASUREMENT_MEASUREMENT])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]], [[
char c;
c = *((unsigned char *)  nl_langinfo(_NL_MEASUREMENT_MEASUREMENT));
]])],[gtk_ok=yes],[gtk_ok=no])
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_MEASUREMENT_MEASUREMENT], [1],
            [Define if _NL_MEASUREMENT_MEASUREMENT is available])
fi

dnl ***************************************************************************
dnl *** Find tzdata                                                         ***
dnl ***************************************************************************
AC_ARG_WITH(zoneinfo-dir,
	    [  --with-zoneinfo-dir     tzdata zoneinfo directory],
	    ZONEINFO_DIR="$withval")
if test -z "$ZONEINFO_DIR"; then
    if test -d /usr/share/zoneinfo; then
	ZONEINFO_DIR="/usr/share/zoneinfo"
    elif test -d /usr/share/lib/zoneinfo/tab; then
	ZONEINFO_DIR="/usr/share/lib/zoneinfo/tab"
    else
	AC_MSG_ERROR(Can't find zoneinfo directory. Use --with-zoneinfo-dir)
    fi
fi
AC_DEFINE_UNQUOTED(ZONEINFO_DIR, "$ZONEINFO_DIR", [zoneinfo directory])

AC_SUBST(VERSION)
AC_SUBST(PACKAGE)
AC_SUBST(DATADIR)
AC_SUBST(LIBDIR)
AC_SUBST(GLADEDIR)

##################################################
# Checks for gtk-doc and docbook-tools
##################################################
GTK_DOC_CHECK([1.11],[--flavour no-tmpl])

GOBJECT_INTROSPECTION_CHECK([0.9.5])

dnl ***************************************************************************
dnl *** Output Makefiles et al                                              ***
dnl ***************************************************************************
AC_CONFIG_FILES([
Makefile
doc/Makefile
po/Makefile.in
po-locations/Makefile
libgweather/Makefile
libgweather/gweather-3.0.pc
libgweather/gweather-3.0-uninstalled.pc
data/Makefile
icons/Makefile
schemas/Makefile
])
AC_OUTPUT

if test "x$enable_big_xml" = "xyes"; then
	LOCATIONS_XML_TRANSLATIONS="one big file"
else
	LOCATIONS_XML_TRANSLATIONS="one file per translation"
fi

dnl ***************************************************************************
dnl *** Display Summary                                                     ***
dnl ***************************************************************************
echo "
libgweather-$VERSION configure summary:

	Prefix:				${prefix}
	Source code location:		${srcdir}
	Compiler:			${CC}
	Introspection support:          ${found_introspection}
	Locations.xml translations:	${LOCATIONS_XML_TRANSLATIONS}
	Locations.xml compression:	${enable_locations_compression}
" >&2
