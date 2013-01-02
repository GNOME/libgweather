#!/usr/bin/python3
#
# locations_diff.py
#
# Generate a report of what locations have changed between to versions of the
# Locations.xml.in file
#
# (c) 2004, Davyd Madeley <davyd@madeley.id.au>
#

import sys, codecs
from xml.dom.minidom import parse

sys.stdout = codecs.getwriter("utf-8")(sys.__stdout__)

try:
	old_dom = parse (sys.argv[1])
except:
	sys.stderr.write ("Error: could not parse file %s, aborting...\n" % sys.argv[1])
	sys.exit (1)
try:
	new_dom = parse (sys.argv[2])
except:
	sys.stderr.write ("Error: could not parse file %s, aborting...\n" % sys.argv[2])
	sys.exit (1)

old_locations = old_dom.getElementsByTagName ('location')
new_locations = new_dom.getElementsByTagName ('location')

print('There are %i new locations' % (len (new_locations) - len (old_locations)))

old_locations_dict = {}
new_locations_dict = {}

for location in old_locations:
	for node in location.childNodes:
		if node.nodeType == 1 and node.tagName == '_name':
			location_name = node.childNodes[0].nodeValue
		elif node.nodeType == 1 and node.tagName == 'code':
			location_code = node.childNodes[0].nodeValue
	old_locations_dict[location_code] = location_name

for location in new_locations:
	for node in location.childNodes:
		if node.nodeType == 1 and node.tagName == '_name':
			location_name = node.childNodes[0].nodeValue
		elif node.nodeType == 1 and node.tagName == 'code':
			location_code = node.childNodes[0].nodeValue
	if location_code not in old_locations_dict:
		print('New Location %s - %s' % (location_code, location_name))
	elif old_locations_dict[location_code] != location_name:
		print('Location %s changed name %s => %s' % (location_code,
			old_locations_dict[location_code], location_name))
	new_locations_dict[location_code] = location_name

for location in list(old_locations_dict.keys()):
	if location not in new_locations_dict:
		print('Location Removed %s - %s' % (location,
			old_locations_dict[location]))
