SUBDIRS = data libgweather schemas doc po po-locations

AM_DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-introspection

EXTRA_DIST =		\
	HACKING		\
	MAINTAINERS

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/ABOUT-NLS \
	$(srcdir)/build-aux \
	$(srcdir)/config.rpath \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/m4 \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL)

GITIGNOREFILES = \
	$(PACKAGE)-\*.tar.{gz,bz2,xz} \
	po-locations/Makefile.in.in \
	po-locations/Makefile.in \
	po-locations/Makefile \
	po-locations/Makevars.template \
	po-locations/POTFILES \
	po-locations/Rules-quot \
	po-locations/stamp-it \
	po-locations/.intltool-merge-cache \
	"po-locations/*.gmo" \
	"po-locations/*.header" \
	"po-locations/*.mo" \
	"po-locations/*.sed" \
	"po-locations/*.sin" \
	po-locations/libgweather-locations.pot

-include $(top_srcdir)/git.mk
