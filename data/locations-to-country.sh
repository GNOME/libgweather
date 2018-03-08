#!/bin/sh -e


git checkout Locations.xml
TMPFILE=`mktemp`

xsltproc --path `pwd` locations-flatten.xsl Locations.xml > $TMPFILE && \
	xsltproc --path `pwd` locations-reorder.xsl $TMPFILE | \
	sed 's,"/>," />,' | \
	sed 's,  <!-- Could not find information about the following stations:,<!-- Could not find information about the following stations:,' > Locations.xml
