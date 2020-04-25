#!/usr/bin/python3

import sys
from gi.repository import GLib
from collections import OrderedDict
import math
import struct

import xml.etree.ElementTree as ET
tree = ET.parse(sys.argv[1])
root = tree.getroot()

assert root.tag == "gweather"
assert root.attrib['format'] == "1.0"

levels = {
  'gweather' : 0,
  'region' : 1,
  'country' : 2,
  'state' : 3,
  'city' : 4,
  'location' : 5,
  'named-timezone' : 7,
}

locations = []
timezones = []
loc_by_metar = []
loc_by_country = []

all_ccodes = set()

def get_name(elem):
    name = elem.find('_name')
    if name is None:
        name = elem.find('name')
        msgctx = ''
    else:
        msgctx = name.get('msgctx', default='')
    if name is None:
        return '', ''
    else:
        return name.text, msgctx

def get_coordinates(elem):
    coordinates = elem.findtext('coordinates')
    if coordinates:
        return tuple(float(c) * math.pi / 180.0 for c in coordinates.split())
    else:
        return None

def calc_distance(loc_a, loc_b):
    # average earth radius
    radius = 6372.795

    c_a = get_coordinates(loc_a)
    c_b = get_coordinates(loc_b)
    if c_a is None or c_b is None:
        return float("inf")

    if c_a == c_b:
        return 0

    return math.acos(math.cos(c_a[0]) * math.cos(c_b[0]) * math.cos(c_a[1] - c_b[1]) +
                     math.sin(c_a[0]) * math.sin(c_b[0])) * radius;

def tz_variant(tz):
    obsoletes = []
    for item in tz.findall('obsoletes'):
        obsoletes.append(item.text)

    return GLib.Variant('((ss)as)', (
            get_name(loc),
            obsoletes
        ))

parent_map = {c:p for p in root.iter() for c in p}
def loc_variant(loc):
    children = []
    # find direct children
    for child in loc.find('.'):
        if child.tag in levels:
            children.append(locations.index(child))

    # Sort by distance from self; this is assumed for the locations inside a city
    children.sort(key=lambda c: calc_distance(loc, locations[c]))

    zones = []
    for tz in loc.findall('timezone'):
        zones.append(timezones.index(tz))

    coordinate = get_coordinates(loc)

    tz_hint = loc.findtext('tz-hint')
    if tz_hint:
        for i, tz in enumerate(timezones):
            if tz.get('id') == tz_hint:
                tz_hint = (i,)
                break
        else:
            assert "Should not be reached"

    name = get_name(loc)

    parent = parent_map.get(loc)
    try:
        parent_idx = locations.index(parent)
    except:
        # point to self
        parent_idx = locations.index(loc)

    nearest_idx = None
    if loc.tag == 'city' and len(children) == 0:
        # Try to lookup the nearest sibbling location
        nearest = None
        nearest_dist = -1
        for l in parent.findall('location'):
            dist = calc_distance(loc, l)
            if dist > 100:
                continue
            if nearest is None or dist < nearest_dist:
                nearest = l
                nearest_dist = dist

        if nearest:
            nearest_idx = (locations.index(nearest), )

    return GLib.Variant('((ss)ssm(dd)ssm(q)ym(q)(q)a(q)a(q))', (
            name,
            loc.findtext('zone', default=''),
            loc.findtext('radar', default=''),
            coordinate,
            loc.findtext('iso-code', default=''),
            loc.findtext('code', default=''),
            tz_hint,
            levels[loc.tag],
            nearest_idx,
            (parent_idx, ),
            [(c,) for c in children],
            [(z,) for z in zones]
        ))

locations.append(root)
# Pre-populate the lists to be able to generate indices
for loc in root.iter('named-timezone'):
    locations.append(loc)
    loc_by_metar.append(loc)
    assert loc.findtext('code') is not None
for loc in root.iter('region'):
    locations.append(loc)
for loc in root.iter('country'):
    locations.append(loc)
    loc_by_country.append(loc)
    c = loc.findtext('iso-code')
    assert c is not None
    assert c not in all_ccodes
    all_ccodes.add(c)
for loc in root.iter('state'):
    locations.append(loc)
for loc in root.iter('city'):
    locations.append(loc)
for loc in root.iter('location'):
    locations.append(loc)
    loc_by_metar.append(loc)
    assert loc.findtext('code') is not None

for tz in root.iter('timezone'):
    timezones.append(tz)
    assert tz.get('id') is not None

timezones.sort(key=lambda tz: tz.get('id'))
loc_by_country.sort(key=lambda loc: loc.findtext('iso-code'))
loc_by_metar.sort(key=lambda loc: loc.findtext('code'))

loc_by_country_var = [(loc.findtext('iso-code'), (locations.index(loc),)) for loc in loc_by_country]
loc_by_metar_var = [(loc.findtext('code'), (locations.index(loc),)) for loc in loc_by_metar]

timezones_var = [(tz.get('id'), tz_variant(tz)) for tz in timezones]
locations_var = [loc_variant(loc) for loc in locations]

res = GLib.Variant("(a{s(q)}a{s(q)}a{s((ss)as)}a((ss)ssm(dd)ssm(q)ym(q)(q)a(q)a(q)))", (
    loc_by_country_var,
    loc_by_metar_var,
    timezones_var,
    locations_var
    ))

if struct.pack('h', 0x01)[0]:
    # byteswap on little endian
    res = res.byteswap()

data = res.get_data_as_bytes().get_data()
open(sys.argv[2], 'bw').write(data)


