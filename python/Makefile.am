if BUILD_PYTHON
INCLUDES =			\
	-I$(top_srcdir)		\
	$(PYTHON_INCLUDES)	\
	$(PYGOBJECT_CFLAGS)	\
	$(GTK_CFLAGS)

pyunstabledir = $(pyexecdir)/gweather/I_KNOW_THIS_IS_UNSTABLE
pyunstable_LTLIBRARIES = gweather.la
gweather_la_LDFLAGS = -module -avoid-version -export-symbols-regex initgweather
gweather_la_LIBADD = ../libgweather/libgweather.la
gweather_la_SOURCES = gweathermodule.c
nodist_gweather_la_SOURCES = gweather.c
gweather.c: gweather.defs gweather.override

.defs.c:
	$(AM_V_GEN)(cd $(srcdir)\
	 && pygobject-codegen-2.0 \
		--register $(PYGTK_DEFS) \
		--override $*.override \
		--prefix py$* $*.defs) > gen-$*.c \
	&& cp gen-$*.c $*.c \
	&& rm -f gen-$*.c

install-exec-hook:
	$(MKDIR_P) "$(DESTDIR)$(pyunstabledir)"
	$(INSTALL_DATA) $(srcdir)/bad__init__.py "$(DESTDIR)$(pyexecdir)/gweather/__init__.py"
	touch $(DESTDIR)$(pyunstabledir)/__init__.py
	chmod a+x $(DESTDIR)$(pyunstabledir)/__init__.py

endif

EXTRA_DIST = gweather.defs gweather.override bad__init__.py demo.py

CLEANFILES = gweather.c

-include $(top_srcdir)/git.mk
