m4_define(libgweather_major_version,   3)
m4_define(libgweather_minor_version,  15)
m4_define(libgweather_micro_version,  1)
m4_define(libgweather_version, libgweather_major_version.libgweather_minor_version.libgweather_micro_version)

AC_INIT([libgweather],
        [libgweather_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=libgweather],
        [libgweather],
        [http://live.gnome.org/LibGWeather])
AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl === Versioning ============================================================

LIBGWEATHER_MAJOR_VERSION=libgweather_major_version
LIBGWEATHER_MINOR_VERSION=libgweather_minor_version
LIBGWEATHER_MICRO_VERSION=libgweather_micro_version
LIBGWEATHER_VERSION=libgweather_version
AC_SUBST(LIBGWEATHER_MAJOR_VERSION)
AC_SUBST(LIBGWEATHER_MINOR_VERSION)
AC_SUBST(LIBGWEATHER_MICRO_VERSION)
AC_SUBST(LIBGWEATHER_VERSION)

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LT_VERSION=9:2:3
AC_SUBST(LT_VERSION)

GTK_REQUIRED=3.13.5
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

GNOME_COMPILE_WARNINGS([maximum])

PKG_CHECK_MODULES(DEPS, [gtk+-3.0 >= $GTK_REQUIRED
                         libxml-2.0 >= $LIBXML_REQUIRED
                         libsoup-2.4 >= $LIBSOUP_REQUIRED
                         glib-2.0 >= $GLIB_REQUIRED
                         gio-2.0
                         geocode-glib-1.0])

dnl -- check for glib; redundant at this point, but sets $GLIB_MKENUMS
AM_PATH_GLIB_2_0($GLIB_REQUIRED)

dnl gettext
AM_GNU_GETTEXT_VERSION([0.18])
AM_GNU_GETTEXT([external])

IT_PROG_INTLTOOL([0.50.0])
IT_PO_SUBDIR([po-locations])

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

dnl ***************************************************************************
dnl *** Glade                                                               ***
dnl ***************************************************************************

AC_ARG_ENABLE([glade-catalog],
	[AS_HELP_STRING([--enable-glade-catalog],
		[Install a glade catalog file [default=auto]])],
	[enable_glade_catalog=$enableval],
	[enable_glade_catalog=auto])

if test "x$enable_glade_catalog" = "xno"; then
  found_glade_catalog="no (disabled, use --enable-glade-catalog to enable)"
else
  AC_MSG_CHECKING([for Glade.])

  PKG_CHECK_EXISTS([gladeui-2.0],
			 [found_glade_catalog=yes],
			 [found_glade_catalog=no])
fi

if test "x$enable_glade_catalog" = "xyes" -a "x$found_glade_catalog" = "xno"; then
  AC_MSG_ERROR([You need to have gladeui-2.0 installed to build libgweather with glade enabled])
fi
AC_MSG_RESULT([$found_glade_catalog])

if test "x$found_glade_catalog" = "xyes"; then
  AC_MSG_CHECKING([for Glade catalog dir])
  GLADE_DATA_DIR=`$PKG_CONFIG --variable=datadir gladeui-2.0 2>&AS_MESSAGE_LOG_FD`
  GLADE_CATALOG_DTD_DIR=`$PKG_CONFIG --variable=catalogdir gladeui-2.0 2>&AS_MESSAGE_LOG_FD`
  GLADE_CATALOG_DIR=`$PKG_CONFIG --variable=catalogdir gladeui-2.0 2>&AS_MESSAGE_LOG_FD | sed -e "s#^$GLADE_DATA_DIR#$datadir#"`
  AC_MSG_RESULT([$GLADE_CATALOG_DIR])
  AC_SUBST(GLADE_CATALOG_DIR)
  AC_SUBST(GLADE_CATALOG_DTD_DIR)
fi

AM_CONDITIONAL([ENABLE_GLADE_CATALOG],[test "x$found_glade_catalog" = "xyes"])

AC_ARG_WITH([owm-apikey],
        [AS_HELP_STRING([--with-owm-apikey=APIKEY],
        [Specify an API key for OpenWeatherMap (optional)])])
if test "x$with_owm_apikey" != x ; then
   AC_DEFINE_UNQUOTED(OWM_APIKEY, "$with_owm_apikey", [OpenWeatherMap API key])
fi

##################################################
# Checks for gtk-doc and docbook-tools
##################################################
GTK_DOC_CHECK([1.11],[--flavour no-tmpl])

GOBJECT_INTROSPECTION_CHECK([0.9.5])

dnl ***************************************************************************
dnl *** Vala bindings support                                               ***
dnl ***************************************************************************
AC_ARG_ENABLE([vala],
	[AS_HELP_STRING([--enable-vala],
		[Install vala bindings [default=auto]])],
	[enable_vala=$enableval],
	[enable_vala=auto])

m4_ifdef([VAPIGEN_CHECK],
	[VAPIGEN_CHECK([0.18],,,[no])],
	[AM_CONDITIONAL([ENABLE_VAPIGEN],[false])])

dnl ***************************************************************************
dnl *** Output Makefiles et al                                              ***
dnl ***************************************************************************
AC_CONFIG_FILES([
Makefile
doc/Makefile
doc/version.xml
po/Makefile.in
po-locations/Makefile.in
libgweather/Makefile
libgweather/gweather-3.0.pc
libgweather/gweather-3.0-uninstalled.pc
libgweather/gweather-version.h
data/Makefile
data/glade/Makefile
schemas/Makefile
])
AC_OUTPUT

dnl ***************************************************************************
dnl *** Display Summary                                                     ***
dnl ***************************************************************************
echo "
libgweather-$VERSION configure summary:

	Prefix:				${prefix}
	Source code location:		${srcdir}
	Compiler:			${CC}
	Introspection support:          ${found_introspection}
	Build Vala bindings:            ${enable_vala}
" >&2
