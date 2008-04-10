#!/bin/sh

locations=${1:-Locations.xml.in}
used=`mktemp`
correct=`mktemp`

sed -ne 's/.*<tz-hint>\(.*\)<.*/\1/p' $locations | sort -u > $used
awk '{print $3;}' /usr/share/zoneinfo/zone.tab  | sort -u > $correct
bad=`comm -13 $correct $used`
rm $correct $used

if [ -n "$bad" ]; then
    echo "Invalid timezones in ${locations}: $bad" 1>&2
    exit 1
fi
exit 0
