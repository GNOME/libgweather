catalogdir = $(GLADE_CATALOG_DIR)
catalog_in_files = libgweather.xml.in
catalog_DATA = $(catalog_in_files:.xml.in=.xml)

EXTRA_DIST = \
	$(catalog_in_files)

check-local: $(catalog_DATA)
	xmllint --dtdvalid $(GLADE_CATALOG_DTD_DIR)/glade-catalog.dtd --noout $<

CLEANFILES = $(catalog_DATA)

@INTLTOOL_XML_NOMERGE_RULE@

-include $(top_srcdir)/git.mk
