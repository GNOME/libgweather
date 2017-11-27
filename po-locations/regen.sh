#!/bin/sh

# Find a file called Locations.xml.h which is newer than both
# ../data/Locations.xml and extract.xsl, and if it's not found,
# build it. In other words, regenerate Locations.xml.h if it's
# missing or out of date.
find . -name Locations.xml.h \
       -newer ../data/Locations.xml \
       -newer extract.xsl \
       -print | grep -q . || {
    echo "Rebuilding Locations.xml.h"
    xsltproc extract.xsl ../data/Locations.xml > Locations.xml.h
}

# Likewise locations.pot
find . -name locations.pot \
       -newer Locations.xml.h \
       -print | grep -q . || {
    echo "Rebuilding locations.pot"
    xgettext --add-comments --output=locations.pot --from-code=utf-8 \
        --keyword=N_ --keyword=NC_:1c,2 --no-location Locations.xml.h
}

# Now rebuild po files given on command line, if any
status=0
if [ -n "$*" ]; then
    for po in "$@"; do
        if [ -f $po ]; then
            echo "Rebuilding $po"
            msgmerge -U $po locations.pot
        elif [ -f $po.po ]; then
            echo "Rebuilding $po.po"
            msgmerge -U $po.po locations.pot
        else
            echo "No such file: $po" 1>&2
            status=1
        fi
    done
fi

exit $status
