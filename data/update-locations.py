#!/usr/bin/python3
# -*- coding: utf-8 -*-

import codecs
import locale
import math
import os
import re
import sqlite3
import sys
import urllib.request, urllib.parse, urllib.error
from xml.dom import minidom
from xml.sax import saxutils
from functools import reduce

# The database...
db = sqlite3.connect('locationdb.sqlite')
fips_codes = {}
station_timezones = {}
station_zones = {}
station_radars = {}
station_comments = {}
station_states = {}

# xml helpers
def getChildByName(node, name):
    node = node.firstChild
    while node is not None:
        if hasattr(node, 'tagName') and node.tagName == name:
            return node
        node = node.nextSibling
    return None

def getChildContentByName(node, name):
    node = getChildByName(node, name)
    if node is not None:
        return node.firstChild.nodeValue
    else:
        return None

def getChildrenByName(node, name):
    children = []
    node = node.firstChild
    while node is not None:
        if hasattr(node, 'tagName') and node.tagName == name:
            children.append(node)
        node = node.nextSibling
    return children

def getNodeChildren(node):
    children = []
    node = node.firstChild
    while node is not None:
        if hasattr(node, 'tagName'):
            children.append(node)
        node = node.nextSibling
    return children

def getComment(node):
    names = getChildrenByName(node, '_name')
    if len(names) == 0:
        return None
    node = names[0].previousSibling
    if node is None:
        return None
    if node.nodeType == node.TEXT_NODE and not node.nodeValue.strip():
        node = node.previousSibling
        if node is None:
            return None
    if node.nodeType == node.COMMENT_NODE:
        return node.nodeValue.strip()
    else:
        return None

def printComment(indent, comment, keep_newlines=False):
    if not keep_newlines:
        comment = re.sub(r'\n[ \t]*', r' ', comment)
    prefix = '%s<!-- ' % indent
    width = 72 - len(prefix)
    if len(comment) < width and comment.find('\n') == -1:
        print('%s<!-- %s -->' % (indent, comment))
        return

    while len(comment) > width or comment.find('\n') != -1:
        brk = comment.find('\n', 0, width)
        if brk == -1:
            brk = comment.rfind(' ', 0, width)
            if brk == -1:
                brk == comment.find(' ')
                if brk == -1:
                    break
        print('%s%s' % (prefix, comment[:brk]))
        prefix = '%s     ' % indent
        brk += 1
        comment = comment[brk:]
    if len(comment):
        print('%s%s' % (prefix, comment))
    print('%s  -->' % indent)

# other helpers
def getFipsCodes(node, container):
    # values is a list of string values of <fips-code> elements
    values = [node.firstChild.nodeValue for node in getChildrenByName(node, 'fips-code')]
    # code_lists is a list of lists of individual codes
    code_lists = [value.split("|") for value in values]
    if code_lists:
        for code in reduce(list.__add__, code_lists):
            fips_codes[code] = container

    return values

def getFipsCountry(city):
    if city.state_code != "" and city.state_code in fips_codes:
        country = fips_codes[city.state_code]
    elif city.country_code != "" and city.country_code in fips_codes:
        country = fips_codes[city.country_code]
    else:
        return None
    while not isinstance(country, Country):
        country = country.parent
    return country

languages = {}
# FIXME, configurable
iso639 = minidom.parse('/usr/share/xml/iso-codes/iso_639_3.xml')
for lang in iso639.getElementsByTagName('iso_639_3_entry'):
    name = lang.getAttribute('name')
    name = re.sub(r",.*", r"", name)
    languages[lang.getAttribute('id')] = name
languages.setdefault('(Unknown Language)')

# classes
class Timezones:
    def __init__(self, node):
        self.zones = [Timezone(z) for z in getChildrenByName(node, 'timezone')]

    def print_xml(self, indent):
        print('%s<timezones>' % indent)
        for zone in self.zones:
            zone.print_xml(indent + '  ')
        print('%s</timezones>' % indent)

class Timezone:
    def __init__(self, node):
        self.id = node.getAttribute('id')
        self.name = getChildContentByName(node, '_name')
        self.comment = getComment(node)
        self.obsoletes = [obs.firstChild.nodeValue for obs in getChildrenByName(node, 'obsoletes')]
        self.dup_name = False

    def print_xml(self, indent):
        if self.name is not None or len(self.obsoletes):
            print('%s<timezone id="%s">' % (indent, self.id))
            if self.comment is not None:
                printComment(indent + '  ', self.comment)
            if self.name is not None:
                if self.dup_name:
                    msgctxt=' msgctxt="Timezone"'
                else:
                    msgctxt=''
                print('%s  <_name%s>%s</_name>' % (indent, msgctxt, self.name))
            for obs in self.obsoletes:
                print('%s  <obsoletes>%s</obsoletes>' % (indent, obs))
            print('%s</timezone>' % indent)
        else:
            print('%s<timezone id="%s" />' % (indent, self.id))

class LocBase:
    def __init__(self, parent, arg):
        self.parent = parent
        self.contents = []
        self.dup_name = False

        if isinstance(arg, minidom.Element):
            self.name = getChildContentByName(arg, '_name')
            self.iso_code = getChildContentByName(arg, 'iso-code')
            self.tz_hint = getChildContentByName(arg, 'tz-hint')
            self.pref_lang = getChildContentByName(arg, 'pref-lang')
            self.zone = getChildContentByName(arg, 'zone')
            self.radar = getChildContentByName(arg, 'radar')
            self.coordinates = getChildContentByName(arg, 'coordinates')
            self.fips_codes = getFipsCodes(arg, self)
            self.comment = getComment(arg)
            zones = getChildByName(arg, 'timezones')
            if zones is not None:
                self.timezones = Timezones(zones)
            else:
                self.timezones = None
        else:
            self.name = None
            self.iso_code = None
            self.tz_hint = None
            self.pref_lang = None
            self.fips_codes = None
            self.comment = None
            self.zone = None
            self.radar = None
            self.timezones = None

    def __cmp__(self, other):
        return cmp(self.name, other.name)

    def print_xml(self, indent):
        if self.iso_code is not None:
            print('%s  <iso-code>%s</iso-code>' % (indent, self.iso_code))
        if self.fips_codes is not None:
            for value in self.fips_codes:
                print('%s  <fips-code>%s</fips-code>' % (indent, value))
        if self.pref_lang is not None:
            print('%s  <pref-lang>%s</pref-lang>' % (indent, self.pref_lang))
        if self.timezones is not None:
            self.timezones.print_xml(indent + '  ')
        if self.tz_hint is not None:
            print('%s  <tz-hint>%s</tz-hint>' % (indent, self.tz_hint))
        if self.zone is not None:
            print('%s  <zone>%s</zone>' % (indent, self.zone))
        if self.radar is not None:
            print('%s  <radar>%s</radar>' % (indent, self.radar))
        if self.coordinates is not None:
            print('%s  <coordinates>%s</coordinates>' % (indent, self.coordinates))
        for item in self.contents:
            item.print_xml(indent + '  ')

    def print_name(self, indent):
        if self.dup_name:
            msgctxt = ' msgctxt="%s' % self.__class__.__name__
            if self.parent is not None:
                msgctxt += ' in '
                c = self.parent
                while c is not None:
                    if c is not self.parent:
                        msgctxt += ', '
                    # although it would be prettier to use c.in_name
                    # here for countries, that would be bad because if
                    # we changed the list of "the"-using countries,
                    # that would then cause translations to get
                    # fuzzified
                    msgctxt += saxutils.escape(c.name)
                    c = c.parent
            msgctxt += '"'
        else:
            msgctxt = ''

        print('%s  <_name%s>%s</_name>' % (indent, msgctxt, saxutils.escape(self.name)))

    def station_prefixes(self):
        return reduce(set.__or__, [x.station_prefixes() for x in self.contents], set())

class Region(LocBase):
    def __init__(self, elt):
        LocBase.__init__(self, None, elt)

    def print_xml(self, indent):
        print('%s<region>' % indent)
        if self.comment is not None:
            printComment(indent + '  ', self.comment)
        self.print_name(indent)
        LocBase.print_xml(self, indent)
        print('%s</region>' % indent)

class Country(LocBase):
    def __init__(self, elt):
        LocBase.__init__(self, None, elt)
        self.missing_stations = []
        self.unknown_stations = []
        self.missing_cities = []

        if self.name.find(',') != -1:
            comma = self.name.find(',')
            self.in_name = 'the %s %s' % (self.name[comma + 2:], self.name[:comma])
        elif self.name.startswith('United') or \
           self.name.find('Republic') != -1 or \
           self.name.find('lands') != -1:
            self.in_name = 'the ' + self.name
        elif re.search(r'(BS|IM|MV|PH|PS|SC|TF|VA)', self.iso_code):
            self.in_name = 'the ' + self.name
        else:
            self.in_name = self.name

    def print_xml(self, indent):
        print('%s<country>' % indent)
        if self.comment is not None:
            printComment(indent + '  ', self.comment)
        self.print_name(indent)
        if len(self.missing_stations):
            self.missing_stations.sort()
            comment = 'Could not find cities for the following weather stations:\n'
            for station in self.missing_stations:
                comment += '%s - %s (%s)\n' % (station.code, station.name, station.coordinates)
            printComment(indent + '  ', comment, True)
        if len(self.unknown_stations):
            self.unknown_stations.sort()
            comment = 'Could not find information about the following stations, which may be in %s:\n' % self.name
            for code in self.unknown_stations:
                comment += '%s ' % code
            printComment(indent + '  ', comment, True)
        if len(self.missing_cities):
            self.missing_cities.sort()
            comment = 'Could not find weather stations for the following major cities:\n'
            comment += self.missing_cities[0].name
            for city in self.missing_cities[1:]:
                comment += ', %s' % city.name
            printComment(indent + '  ', comment, True)
        LocBase.print_xml(self, indent)
        print('%s</country>' % indent)

class State(LocBase):
    def __init__(self, parent, elt):
        LocBase.__init__(self, parent, elt)
        if self.comment is None:
            self.comment = 'A state/province/territory in %s' % self.parent.name

    def print_xml(self, indent):
        print('%s<state>' % indent)
        if self.comment is not None:
            printComment(indent + '  ', self.comment)
        self.print_name(indent)
        LocBase.print_xml(self, indent)
        print('%s</state>' % indent)

class City(LocBase):
    def __init__(self, arg):
        LocBase.__init__(self, None, arg)

        self.name_comment = []

        if isinstance(arg, tuple):
            (self.id, self.latitude, self.longitude, elevation, self.importance, self.country_code, self.state_code, self.county_code, name_type, self.name_lang, short_name, long_name, flat_name) = arg
            self.coordinates = "%f %f" % (self.latitude, self.longitude)
            self.is_capital = self.importance == 3
            self.name = short_name or long_name
            self.has_conventional_name = name_type == 'C'
        elif isinstance(arg, LocBase):
            self.name = arg.name
            self.name_lang = None
            self.has_conventional_name = False
            self.id = name
            self.country_code = arg.country_code
            self.state_code = arg.state_code
            self.county_code = None
            self.is_capital = False
            self.coordinates = arg.coordinates
            self.latitude = arg.latitude
            self.longitude = arg.longitude

    def print_xml(self, indent):
        self.contents.sort(lambda l1, l2: cmp(distance(self.latitude, self.longitude, l1.latitude, l1.longitude),
                                              distance(self.latitude, self.longitude, l2.latitude, l2.longitude)))

        if self.comment is None and self.parent is not None:
            if self.is_capital:
                country = self.parent
                while not isinstance(country, Country):
                    country = country.parent
                self.comment = 'The capital of %s' % country.in_name
            elif self.parent.parent is not None:
                self.comment = 'A city in %s in %s' % \
                               (self.parent.name, self.parent.parent.in_name)
            else:
                self.comment = 'A city in %s' % self.parent.in_name

        print('%s<city>' % indent)
        comment = self.comment or ''
        if len(self.name_comment):
            if len(comment):
                if comment[-1] != '.':
                    comment += '.\n'
                else:
                    comment += '\n'
            comment += '\n'.join(self.name_comment)
        if len(comment):
            printComment(indent + '  ', comment, True)
        self.print_name(indent)
        print('%s  <coordinates>%s</coordinates>' % (indent, self.coordinates))
        for item in self.contents:
            item.print_xml(indent + '  ', self)
        print('%s</city>' % indent)

class Location(LocBase):
    def __init__(self, arg):
        LocBase.__init__(self, None, arg)

        if isinstance(arg, tuple):
            (self.code, self.name, self.state_code, self.country_code, self.latitude, self.longitude, elevation) = arg
            self.coordinates = "%f %f" % (self.latitude, self.longitude)
            if self.code in station_timezones:
                self.tz_hint = station_timezones[self.code]
            if self.code in station_zones:
                self.zone = station_zones[self.code]
            if self.code in station_radars:
                self.radar = station_radars[self.code]
            if self.code in station_comments:
                self.comment = station_comments[self.code]

    def print_xml(self, indent, city=None):
        print('%s<location>' % indent)
        name = self.name
        if city is not None:
            if name.startswith("%s, " % city.name):
                if name.startswith("%s, %s" % (city.name, city.name)):
                    name = name[len(city.name) + 2:]
                elif name.endswith("port") or name.endswith("Field") or \
                         name.endswith("Terminal") or name.endswith("Base"):
                    name = name[len(city.name) + 2:]
            elif name.endswith(", %s" % city.name):
                name = name[:-len(city.name) - 2]
        #if self.comment is not None:
        #    print '%s  <!-- %s -->' % (indent, self.comment)
        print('%s  <name>%s</name>' % (indent, saxutils.escape(name)))
        print('%s  <code>%s</code>' % (indent, self.code))
        LocBase.print_xml(self, indent)
        print('%s</location>' % indent)

    def station_prefixes(self):
        return set([self.code[:2]])

def do_locations(container, state, parent_zone, parent_radar):
    for xlocation in getChildrenByName(container, 'location'):
        code = getChildContentByName(xlocation, 'code')
        tz_hint = getChildContentByName(xlocation, 'tz-hint')
        zone = getChildContentByName(xlocation, 'zone') or parent_zone
        radar = getChildContentByName(xlocation, 'radar') or parent_radar
        comment = getComment(xlocation)

        if tz_hint is not None:
            station_timezones[code] = tz_hint
        if zone is not None:
            station_zones[code] = zone
        if radar is not None:
            station_radars[code] = radar
        if comment is not None:
            station_comments[code] = comment
        if state is not None:
            station_states[code] = state

def do_cities(container, state):
    for xcity in getChildrenByName(container, 'city'):
        zone = getChildContentByName(xcity, 'zone')
        radar = getChildContentByName(xcity, 'radar')
        do_locations(xcity, state, zone, radar)

# Read the existing regions/countries/states
old = minidom.parse('Locations.xml.in')

regions = []
for xreg in old.getElementsByTagName('region'):
    reg = Region(xreg)
    regions.append(reg)

    for xcountry in getChildrenByName(xreg, 'country'):
        country = Country(xcountry)
        reg.contents.append(country)
        do_cities(xcountry, None)
        do_locations(xcountry, None, None, None)

        for xstate in getChildrenByName(xcountry, 'state'):
            state = State(country, xstate)
            country.contents.append(state)

            do_cities(xstate, state)
            do_locations(xstate, state, None, None)

regions.sort()

# location-finding helpers
def distance(lat1, long1, lat2, long2):
    long1 = float(long1) * math.pi / 180.0
    lat1  = float(lat1) * math.pi / 180.0
    long2 = float(long2) * math.pi / 180.0
    lat2  = float(lat2) * math.pi / 180.0
    tmp = math.cos(lat1) * math.cos(lat2) * math.cos(long1 - long2) + math.sin(lat1) * math.sin(lat2)
    # rounding errors can cause us to get a result out of range...
    if tmp >= 1.0:
        return 0.0
    return math.acos(tmp) * 6372.795

cc = db.cursor()
cities = {}
def get_city(id, country_code=None):
    if id in cities:
        return cities[id]

    city = None
    best_match = None

    cc.execute("SELECT * FROM cities WHERE id=?", (id,))
    matches = cc.fetchall()

    for match in matches:
        (name_type, language, short_name, long_name) = match[8:12]
        if not country_code:
            country_code = match[5]

        # name type: "N" = standard, "C" = conventional (ie,
        # traditional English)
        if name_type == 'C' and ( language == 'eng' or language == ''):
            city = City(match)
            best_match = match
            break
        elif name_type == 'N':
            if city is None:
                city = City(match)
                best_match = match
            elif not city.has_conventional_name:
                country = getFipsCountry(city)
                if language == '' or (country is not None and country.pref_lang == language):
                    city = City(match)
                    best_match = match

    # Shouldn't happen, but some cities have only a "V" (variant) "D"
    # (unofficial) or "NS"/"VS"/"DS" (N/V/D in alternate script) name
    # given... maybe we should skip these?
    if city is None:
        city = City(matches[0])
        best_match = matches[0]

    # See if the city has an ambiguous name
    (short_name, long_name) = best_match[10:12]
    if short_name and long_name:
        country_code = best_match[5]
        cc.execute("SELECT COUNT(*) FROM cities WHERE short_name=? AND country_code=?", (short_name, country_code))
        count = int(cc.fetchone()[0])
        if count > 1:
            city.name = long_name

    # Note local names
    localnames = {}
    local_short_name =  None
    script_name = None
    for match in matches:
        (name_type, language, short_name, long_name) = match[8:12]
        name = short_name or long_name
        if name_type != 'N' and name_type != 'NS':
            continue
        if language == 'eng' or name == city.name:
            continue

        if long_name == city.name:
            local_short_name = short_name
        elif language == city.name_lang and name_type == 'NS':
            script_name = name
        elif language in localnames:
            if localnames[language].find(name) == -1:
                if name_type == 'N':
                    localnames[language] = "%s / %s" % (name, localnames[language])
                else:
                    localnames[language] = "%s / %s" % (localnames[language], name)
        else:
            localnames[language] = name

    if city.has_conventional_name and (script_name is not None or len(localnames) > 0):
        city.name_comment.append('"%s" is the traditional English name.' % city.name)
    if local_short_name is not None:
        country = getFipsCountry(city)
        city.name_comment.append('One of several cities in %s called "%s".' % (country.name, local_short_name))
    if script_name is not None:
        city.name_comment.append('The name is also written "%s".' % script_name)
    for lang in localnames:
        if lang == "":
            city.name_comment.append('The local name is "%s".' % localnames[lang])
        else:
            city.name_comment.append('The local name in %s is "%s".' % (languages[lang], localnames[lang]))

    cities[id] = city
    if country_code:
        city.country_code = country_code
    return city

def fakeable(station):
    # Use weather station names to fake "city" names for mostly-
    # uninhabited regions. At the moment, that means Antarctica and
    # United States Minor Outlying Islands
    return station.country_code == 'AY' or \
           station.state_code == 'US74'

def add_city(city, station):
    if city.id in stations:
        stations[city.id].append(station.code)
    else:
        stations[city.id] = [station.code]

    for st in city.contents:
        if st.code == station.code:
            return
    city.contents.append(station)

def find_city(c, city_name, station, try_nearby):
    if fakeable(station):
        city = City(station)
        cities[city.id] = city
        add_city(city, station)
        return city

    locs = []
    shortened = False
    while len(locs) == 0 and len(city_name) > 0:
        flat_name = re.sub(r"[^a-zA-Z]", r"", city_name.upper())
        if station.country_code == "":
            c.execute("SELECT * FROM cities WHERE latitude BETWEEN ? AND ? AND longitude BETWEEN ? AND ? AND ( short_name=? OR long_name=? OR flat_name=? )", (station.latitude - 2.0, station.latitude + 2.0, station.longitude - 2.0, station.longitude + 2.0, city_name, city_name, flat_name))
        else:
            c.execute("SELECT * FROM cities WHERE country_code=? AND latitude BETWEEN ? AND ? AND longitude BETWEEN ? AND ? AND ( short_name=? OR long_name=? OR flat_name=? )", (station.country_code[:2], station.latitude - 2.0, station.latitude + 2.0, station.longitude - 2.0, station.longitude + 2.0, city_name, city_name, flat_name))
        locs = c.fetchall()

        # Strip out cities that are too far away; they're probably false matches
        ok_locs = []
        for loc in locs:
            dist = distance(station.latitude, station.longitude, loc[1], loc[2])
            if dist < 50.0:
                ok_locs.append(loc)
        locs = ok_locs

        if len(locs) == 0:
            city_name = re.sub(r"[ -]?[^ -]*$", "", city_name)
            shortened = True

    if len(locs) == 0 and try_nearby:
        if station.country_code != "":
            c.execute("SELECT * FROM cities WHERE country_code=? AND latitude BETWEEN ? AND ? AND longitude BETWEEN ? AND ? AND importance != -1", (station.country_code[:2], station.latitude - 1.0, station.latitude + 1.0, station.longitude - 1.0, station.longitude + 1.0))
        else:
            c.execute("SELECT * FROM cities WHERE latitude BETWEEN ? AND ? AND longitude BETWEEN ? AND ? AND importance != -1", (station.latitude - 1.0, station.latitude + 1.0, station.longitude - 1.0, station.longitude + 1.0))
        locs = c.fetchall()

    if len(locs) == 0:
        return None

    # FIXME: we should factor elevation into this decision as well; we
    # don't want to use a weather station at sea level for a city in
    # the mountains, or vice versa.
    best_dist = 0.0
    best_loc = None
    for loc in locs:
        dist = distance(station.latitude, station.longitude, loc[1], loc[2])
        if dist > 50.0:
            continue
        if loc[4] > 0: # pretend important cities are closer
            dist = dist / 5.0
        elif loc[4] < 0: # and trivial ones are farther
            dist = dist * 2.0
        if best_loc == None or dist < best_dist:
            best_loc = loc
            best_dist = dist

    if best_loc is None:
        return None

    city = get_city(best_loc[0], station.country_code)

    if not shortened:
        station.name = station.name.replace(city_name, city.name)

    return city

def dist_from_station(city, station, cmpcity):
    if city.importance != cmpcity.importance:
        return -cmp(city.importance, cmpcity.importance)
    dist = distance(station.latitude, station.longitude,
                    city.latitude, city.longitude)
    if city.importance > 0:
        dist = dist / 5.0
    elif city.importance < 0: # and trivial ones are farther
        dist = dist * 2.0
    return dist

observations_url = os.getenv('OBSERVATIONS_URL') or 'http://gnome.org/~danw/observations.txt'
observations = urllib.request.urlopen(observations_url)
recent = [obs.rstrip() for obs in observations.readlines()]
observations.close()

stations = {}

c = db.cursor()

# Now read in the stations, filter out the losers, and clean up the rest
sc = db.cursor()
sc.execute("SELECT * FROM stations");
while True:
    loc = sc.fetchone()
    if loc is None:
        break
    station = Location(loc)

    if station.code not in recent:
        continue

    city = None

    # Find a match
    location = station.name.replace(", ", " / ")
    if location.find(" / ") != -1:
        matched_cities = []
        na_parts = [part for part in location.split(" / ")
                    if not part.endswith('Airport') and
                    part.find('County') == -1]
        for part in na_parts:
            city = find_city(c, part, station, False)
            if city is not None:
                matched_cities.append(city)
        if len(matched_cities):
            matched_cities.sort(lambda c1, c2: cmp(dist_from_station(c1, station, c2), dist_from_station(c2, station, c1)))
            add_city(matched_cities[0], station)
            continue

    if city is None:
        airport = re.search(r"([^/ ][^/]*?) (International |Municipal )?Airport", location)
        if airport is not None:
            city = find_city(c, airport.group(1), station, False)
            if city is not None:
                add_city(city, station)

    if city is None:
        city = find_city(c, location, station, True)
        if city is None:
            country = fips_codes[station.country_code]
            country.missing_stations.append(station)
            continue
        else:
            add_city(city, station)


# Now do missing major cities
c.execute("SELECT id FROM cities WHERE importance > 1");
while True:
    loc = c.fetchone()
    if loc is None:
        break

    id = loc[0]
    if id in stations:
        continue
    city = get_city(id)
    country = fips_codes[city.country_code]

    sc.execute("SELECT * FROM stations WHERE latitude BETWEEN ? AND ? AND longitude BETWEEN ? AND ?", (city.latitude - 1.0, city.latitude + 1.0, city.longitude - 1.0, city.longitude + 1.0))
    best_dist = 0.0
    best_station = None
    for station in sc.fetchall():
        if station[0] not in recent:
            continue
        dist = distance(city.latitude, city.longitude, station[4], station[5])
        if dist > 50.0:
            continue
        if best_station is None or dist < best_dist:
            best_station = station
            best_dist = dist

    if best_station is None:
        country.missing_cities.append(city)
        stations[id] = []
        continue

    station = Location(best_station)
    add_city(city, station)
    if station in country.missing_stations:
        country.missing_stations.remove(station)

# Find parent states/countries for cities
for id in cities:
    city = cities[id]
    if len(city.contents) == 0:
        continue
    if city.parent is not None:
        continue

    if city.state_code != "" and city.state_code in fips_codes:
        city.parent = fips_codes[city.state_code]
    else:
        for station in city.contents:
            if station.country_code == city.country_code and \
                   station.code in station_states:
                city.parent = station_states[station.code]
                break
        else:
            if city.country_code != "" and city.country_code in fips_codes:
                city.parent = fips_codes[city.country_code]
            else:
                print("Could not find container for city %s in %s" % (city.name, city.country_code));
                continue
    city.parent.contents.append(city)

# Find duplicate names
names = {}

def add_names(container):
    if container.name in names:
        names[container.name].append(container)
    else:
        names[container.name] = [container]
    if not isinstance(container, City):
        for c in container.contents:
            add_names(c)

for region in regions:
    add_names(region)

# timezone names can conflict with country/state names, but not with
# each other
for region in regions:
    for country in region.contents:
        for zone in country.timezones.zones:
            if zone.name in names:
                names[zone.name].append(zone)

for name in names:
    name_list = names[name]
    if len(name_list) > 1:
        for container in name_list:
            container.dup_name = True

# Note unknown weather stations
for id in stations:
    for code in stations[id]:
        if code in recent:
            recent.remove(code)

for country_code in fips_codes:
    country = fips_codes[country_code]
    if not isinstance(country, Country):
        continue
    for prefix in country.station_prefixes():
        for station_code in recent:
            if station_code[:2] == prefix and \
               not station_code in [station.code for station in country.missing_stations]:
                country.unknown_stations.append(station_code)
    for station_code in country.unknown_stations:
        if station_code in recent:
            recent.remove(station_code)

# Sort containers
for code in fips_codes:
    fips_codes[code].contents.sort()

print('<?xml version="1.0" encoding="utf-8"?>')
print('<!DOCTYPE gweather SYSTEM "locations.dtd">')
print('<gweather format="1.0">')
for region in regions:
    region.contents.sort()
    region.print_xml('  ')
if len(recent):
    comment = 'Could not find information about the following stations:\n'
    for station_code in recent:
        comment += '%s ' % station_code
    printComment('', comment, True)
print('</gweather>')
