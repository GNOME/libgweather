#!/usr/bin/perl -w

use File::Temp "tempfile";
use Text::Unidecode;

sub myunidecode {
  my ($text) = @_;
  $text = unidecode($text);
  $text =~ s/\@/a/g;
  return $text;
}

@intlglob = glob("sources/geonames_dd_dms_date_*.txt");
$intlsrc = $intlglob[$#intlglob];

$uscitysrc = "sources/POP_PLACES.txt";
$usbigsrc = "sources/US_CONCISE.txt";
$majorsrc = "major-cities.txt";
$weathersrc = "sources/nsd_cccc.txt";

$locationdb = "locationdb.sqlite";
unlink $locationdb;

($states, $states_filename) = tempfile();
binmode $states, ":utf8";
($counties, $counties_filename) = tempfile();
binmode $counties, ":utf8";
($cities, $cities_filename) = tempfile();
binmode $cities, ":utf8";

# The columns in $intlsrc: (interesting ones in caps)
# RC UFI uni LAT LONG dms_lat dms_long mgrs jog fc DSG PC CC1 ADM1 adm2 POP ELEV CC2 NT LC SHORT_FORM generic SORT_NAME FULL_NAME FULL_NAME_ND modify_date

print "Reading $intlsrc\n";
open INTLSRC, "-|:utf8", "perl ./city-fixups.pl < $intlsrc";
while (<INTLSRC>) {
  # skip header, and historical records
  next if $. == 1 || /\(\(/ || /\(historical\)/i;

  @fields = split /\t/;
  next if $#fields != 25;

  ($rc, $id, $junk, $latitude, $longitude, $junk, $junk, $junk,
   $junk, $junk, $designation, $pc, $country_code, $state_code, $junk,
   $population, $elevation, $cc2, $name_type, $language, $short_name,
   $junk, $flat_name, $long_name, $long_name_nd, $junk) = @fields;

  # Don't use wacky complex transliteration rules for non-European
  # names; just use the non-diacritic version of the name instead
  if ($name_type eq 'N' && (!$rc || $rc > 2)) {
    $long_name = myunidecode($long_name);
    $short_name = myunidecode($short_name);
  } elsif ($name_type eq 'NS' && (!$rc || $rc > 2)) {
    $long_name = $long_name_nd;
  }

  # Also fix smart quotes and dumb spacing
  $short_name =~ s/\x{2019}/'/g;
  $short_name =~ s/  +/ /;
  $short_name =~ s/ $//;
  $long_name =~ s/\x{2019}/'/g;
  $long_name =~ s/  +/ /;
  $long_name =~ s/ $//;

  if ($designation =~ /^PCL/) {
    # /^PCL/ matches countries and other ISO-3166-like things:
    # PCLD - dependent political entity
    # PCLF - freely associated state
    # PCLI - independent political entity (ie, "country")
    # PCLIX - section of independent political entity

    if ($short_name) { $countries{$short_name} = $country_code; }
    if ($long_name) { $countries{$long_name} = $country_code; }

  } elsif ($designation eq "ADM1") {
    # ID COUNTRY_CODE STATE_CODE NAME_TYPE LANGUAGE SHORT_NAME
    # LONG_NAME FLAT_NAME
    print $states "intl-$id\t$country_code\t$country_code$state_code\t$name_type\t$language\t$short_name\t$long_name\t$flat_name\n";

  } elsif ($designation =~ /^PPL/) {
    # "PPL*" is "Populated Place"

    # Assign an "importance" to the city. Having a positive
    # $importance makes a city "bigger" in terms of figuring out how
    # close it is to a given coordinate, and having negative
    # $importance makes it "smaller". Furthermore, a negative
    # importance city can never match solely on coordinates; it needs
    # to match based on name as well.
    #
    # $importance = 2 means furthermore that if we haven't found a
    # weather station for the city after going through the whole
    # stations table, then we'll add an entry using whatever weather
    # station is closest. $importance = 3 means it's a national
    # capital.
    #
    # Our current definition of "important" is that it has designation
    # of "PPLC" (capital of a country [that is, a /PCL.*/]) or "PPLA"
    # (capital of an ADM1). We also look at the "Populated Place
    # Classification" ($pc) and population, but as of 2008-05 those
    # aren't filled in yet for most countries.
    #
    # If $pc is 4 or 5 ("unimportant"), or $designation is "PPLX"
    # (neighborhood/subdivision), we give the city $importance = -1,
    # meaning it will only be used when a weather station actually
    # matches its name; it won't get pulled in as a "nearest city"
    # match.
    if ($designation eq "PPLC") {
      $importance = 3;
    } elsif ($population && $population > 100000) {
      $importance = 2;
    } elsif ($designation eq "PPLA" || $pc eq "1" || $pc eq "2") {
      $importance = 1;
    } elsif ($designation eq "PPLX" || $pc eq "4" || $pc eq "5") {
      $importance = -1;
    } else {
      $importance = 0;
    }

    # either sqlite or pysqlite treats ".9" as a string rather than a
    # number, so make sure we say "0.9" instead
    $latitude = 0 + $latitude;
    $longitude = 0 + $longitude;

    # ID LATITUDE LONGITUDE ELEVATION IMPORTANCE COUNTRY_CODE
    # STATE_CODE COUNTY_CODE NAME_TYPE LANGUAGE SHORT_NAME LONG_NAME
    # FLAT_NAME
    print $cities "intl-$id\t$latitude\t$longitude\t$elevation\t$importance\t$country_code\t$country_code$state_code\t\t$name_type\t$language\t$short_name\t$long_name\t$flat_name\n";

    # The dataset gives two countries for some features, usually
    # meaning either that they cross a national border, or are
    # disputed.
    if ($cc2) {
      print $cities "intl-$id\t$latitude\t$longitude\t$elevation\t$importance\t$cc2\t\t\t$name_type\t$language\t$short_name\t$long_name\t$flat_name\n";
    }
  }
}
close INTLSRC;

# $usbigsrc is the "Concise Features Gazetteer", containing "Large
# features that should be labeled on maps with a scale of 1:250,000".
# We use this to find medium-to-large cities. After reading through
# that, we read $uscitysrc, the "Populated Places Gazetteer", and add
# any remaining cities from there with $importance=-1. Both of these
# files have DOS newlines. (After importing the data into sqlite,
# we'll use $majorsrc to set $importance=2 on the largest US cities.)
%uscities = ();
$/ = "\r\n";

# The columns in $usbigsrc:
# feature_id feature_name class st_alpha st_num county county_num primary_lat_dms primary_lon_dms primary_lat_dec primary_lon_dec source_lat_dms source_lon_dms source_lat_dec source_lon_dec elev map_name

print "Reading $usbigsrc\n";
open USBIGSRC, "<:utf8", $usbigsrc;
while (<USBIGSRC>) {
  # skip header, and historical records
  next if $. == 1 || /\(historical\)/i;

  # remove CRLF and split
  chomp;
  @fields = split /\t/;
  next if $#fields != 16;
  ($feature_id, $name, $class, $state, $state_num, $junk, $county_num, $junk,
   $junk, $latitude, $longitude, $junk, $junk, $junk, $junk,
   $elevation, $junk) = @fields;
  $flat = uc(myunidecode($name));

  if ($class eq "Populated Place") {
    $importance = 0;
  } else {
    next;
  }

  # ID LATITUDE LONGITUDE ELEVATION IMPORTANCE COUNTRY_CODE STATE_CODE
  # COUNTY_CODE NAME_TYPE LANGUAGE SHORT_NAME LONG_NAME FLAT_NAME
  print $cities "us-$feature_id\t$latitude\t$longitude\t$elevation\t$importance\tUS\tUS$state_num\t$county_num\tN\teng\t$name\t\t$flat\n";
  $uscities{$feature_id} = 1;
}
close USBIGSRC;

# The columns in $uscitysrc:
# feature_id feature_name class	st_alpha st_num county county_num primary_lat_dms primary_lon_dms primary_lat_dec primary_lon_dec elev map_name
print "Reading $uscitysrc\n";
open USCITYSRC, "<:utf8", $uscitysrc;
while (<USCITYSRC>) {
  # skip header, and historical records
  next if $. == 1 || /\(historical\)/i;

  # remove CRLF and split
  chomp;
  @fields = split /\t/;
  next if $#fields != 12;
  ($feature_id, $name, $class, $state, $state_num, $county, $county_num,
   $junk, $junk, $latitude, $longitude, $elevation, $junk) = @fields;
  $flat = uc(myunidecode($name));

  if (!$uscities{$feature_id}) {
    # ID LATITUDE LONGITUDE ELEVATION IMPORTANCE COUNTRY_CODE STATE_CODE
    # COUNTY_CODE NAME_TYPE LANGUAGE SHORT_NAME LONG_NAME FLAT_NAME
    print $cities "us-$feature_id\t$latitude\t$longitude\t$elevation\t-1\tUS\tUS$state_num\t$county_num\tN\teng\t$name\t\t$flat\n";
  }

  # There are no separate entries for counties, so we just extract
  # that data out of the cities, keeping track of which counties we've
  # already added
  $county_id = "us-$state_num-$county_num";
  if (!$counties{$county_id}) {
    $counties{$county_id} = 1;

    # ID COUNTRY_CODE STATE_CODE COUNTY_CODE NAME_TYPE LANGUAGE
    # SHORT_NAME LONG_NAME FLAT_NAME
    print $counties "$county_id\tUS\tUS$state_num\t$county_num\tN\teng\t$county\t\t\n";
  }
}
close USCITYSRC;

# Now add some additional data that's not in any source file...

# $intlsrc has no entry for the US or its dependencies
$countries{"United States"} = "US";
$countries{"United States of America"} = "US";
$countries{"United States Minor Outlying Islands"} = "US";
$countries{"Micronesia, Federated States of"} = "US";
# These are in $intlsrc, but not listed as countries
$countries{"Åland Islands"} = "FI01";
$countries{"Antarctica"} = "AY";
$countries{"Faroe Islands"} = "FO";
$countries{"French Guiana"} = "FG";
$countries{"French Polynesia"} = "FP";
$countries{"Greenland"} = "GL";
$countries{"Guadeloupe"} = "GP";
$countries{"Martinique"} = "MB";
$countries{"New Caledonia"} = "NC";
$countries{"Reunion"} = "RE";
$countries{"Saint Pierre and Miquelon"} = "SB";
$countries{"Svalbard"} = "SV";
$countries{"Wallis and Futuna"} = "WF";
# These are variations that appear in $weathersrc
$countries{"Congo, Republic of the"} = "CF";
$countries{"Congo, Democratic Republic of the"} = "CG";
$countries{"Cote d'Ivoire"} = "IV";
$countries{"Falkland Islands, Islas Malvinas"} = "FK";
$countries{"Gambia, The"} = "GA";
$countries{"Korea, North"} = "KN";
$countries{"Korea, South"} = "KS";
$countries{"Macedonia, The Republic of"} = "MK";
$countries{"People's Republic of China"} = "CH";
$countries{"Viet Nam"} = "VM";

$usstatedata = << "EOF";
01	AL	Alabama
02	AK	Alaska
04	AZ	Arizona
05	AR	Arkansas
06	CA	California
08	CO	Colorado
09	CT	Connecticut
10	DE	Delaware
11	DC	District of Columbia
12	FL	Florida
13	GA	Georgia
15	HI	Hawaii
16	ID	Idaho
17	IL	Illinois
18	IN	Indiana
19	IA	Iowa
20	KS	Kansas
21	KY	Kentucky
22	LA	Louisiana
23	ME	Maine
24	MD	Maryland
25	MA	Massachusetts
26	MI	Michigan
27	MN	Minnesota
28	MS	Mississippi
29	MO	Missouri
30	MT	Montana
31	NE	Nebraska
32	NV	Nevada
33	NH	New Hampshire
34	NJ	New Jersey
35	NM	New Mexico
36	NY	New York
37	NC	North Carolina
38	ND	North Dakota
39	OH	Ohio
40	OK	Oklahoma
41	OR	Oregon
42	PA	Pennsylvania
44	RI	Rhode Island
45	SC	South Carolina
46	SD	South Dakota
47	TN	Tennessee
48	TX	Texas
49	UT	Utah
50	VT	Vermont
51	VA	Virginia
53	WA	Washington
54	WV	West Virginia
55	WI	Wisconsin
56	WY	Wyoming
60	AS	American Samoa
64	FM	Federated States of Micronesia
66	GU	Guam
69	MP	Northern Mariana Islands
70	PW	Palau
72	PR	Puerto Rico
74	UM	United States Minor Outlying Islands
78	VI	United States Virgin Islands
EOF
# ID COUNTRY_CODE STATE_CODE NAME_TYPE LANGUAGE SHORT_NAME
# LONG_NAME FLAT_NAME
for $usstate (split(/\n/, $usstatedata)) {
  ($fips, $abbrev, $name) = split(/\t/, $usstate);
  ($flat = uc $name) =~ s/ //g;
  $states{$name} = "US$fips";
  $states{$abbrev} = "US$fips";
  print $states "us-$fips\tUS\tUS$fips\tN\teng\t$abbrev\t$name\t$flat\n";
}

close $states;
close $counties;
close $cities;

print "\nCreating adm1 ('states') table\n";
open SQLITE, "|-:utf8", "sqlite3 $locationdb";
print SQLITE "CREATE TABLE adm1 (id TEXT, country_code TEXT, state_code TEXT, name_type TEXT, language TEXT, short_name TEXT, long_name TEXT, flat_name TEXT);\n";
print SQLITE ".separator '\t'\n";
print SQLITE ".import $states_filename adm1\n";
close SQLITE;
#unlink $states_filename;

print "Creating adm2 ('counties') table\n";
open SQLITE, "|-:utf8", "sqlite3 $locationdb";
print SQLITE "CREATE TABLE adm2 (id TEXT, country_code TEXT, state_code TEXT, county_code TEXT, name_type TEXT, language TEXT, short_name TEXT, long_name TEXT, flat_name TEXT);\n";
print SQLITE ".separator '\t'\n";
print SQLITE ".import $counties_filename adm2\n";
close SQLITE;
#unlink $counties_filename;

print "Creating cities table\n";
open SQLITE, "|-:utf8", "sqlite3 $locationdb";
print SQLITE "CREATE TABLE cities (id TEXT, latitude REAL, longitude REAL, elevation REAL, importance INTEGER, country_code TEXT, state_code TEXT, county_code TEXT, name_type TEXT, language TEXT, short_name TEXT, long_name TEXT, flat_name TEXT);\n";
print SQLITE ".separator '\t'\n";
print SQLITE ".import $cities_filename cities\n";
close SQLITE;
#unlink $cities_filename;

print "Indexing cities\n";
open SQLITE, "|-:utf8", "sqlite3 $locationdb";
print SQLITE "CREATE INDEX short_name ON cities (country_code, short_name);\n";
print SQLITE "CREATE INDEX long_name ON cities (country_code, long_name);\n";
print SQLITE "CREATE INDEX flat_name ON cities (country_code, flat_name);\n";
print SQLITE "CREATE INDEX long_lat ON cities (country_code, longitude, latitude);\n";
print SQLITE "CREATE INDEX id ON cities (id);\n";
close SQLITE;

# Update cities by marking cities "important" if they have the same
# name as their ADM1. (This seems to do a good job of picking up major
# cities in some countries that don't have PC or POP data.)
open SQLITE, "|-:utf8", "sqlite3 $locationdb";
print SQLITE "UPDATE cities SET importance=1 WHERE importance < 1 AND id IN (SELECT cities.id FROM cities INNER JOIN adm1 ON cities.country_code = adm1.country_code AND cities.state_code = adm1.state_code AND cities.flat_name = adm1.flat_name);\n";
print SQLITE "UPDATE cities SET importance=1 WHERE importance < 1 AND id IN (SELECT cities.id FROM cities INNER JOIN adm1 ON cities.country_code = adm1.country_code AND cities.state_code = adm1.state_code AND cities.long_name = adm1.short_name);\n";
close SQLITE;

# Update cities table with additional information about major cities
$/ = "\n";
open MAJORSRC, "<:utf8", $majorsrc;
open SQLITE, "|-:utf8", "sqlite3 $locationdb";

while (<MAJORSRC>) {
  chomp;
  s/\s*#.*//;

  @fields = split /\t/;
  next if $#fields != 4;
  ($country, $latitude, $longitude, $importance, $city) = @fields;

  $city =~ s/'/''/g; # for SQL

  if ($latitude && $longitude) {
    $latlo = $latitude - 0.1;
    $lathi = $latitude + 0.1;
    $lonlo = $longitude - 0.1;
    $lonhi = $longitude + 0.1;
    print SQLITE "UPDATE cities SET importance=$importance WHERE ( short_name='$city' OR long_name='$city' ) AND country_code='$country' AND importance!=-1 AND latitude BETWEEN $latlo AND $lathi AND longitude BETWEEN $lonlo AND $lonhi;\n";
  } else {
    print SQLITE "UPDATE cities SET importance=$importance WHERE ( short_name='$city' OR long_name='$city' ) AND country_code='$country' AND importance!=-1;\n";
  }
}

close MAJORSRC;
close SQLITE;

# The columns in $weathersrc are
# CODE wmo_zone wmo_id NAME STATE COUNTRY region LATITUDE_DMS LONGITUDE_DMS ue_lat ue_long ELEVATION upper_elevation primary
# (however, some lines are mising one or both of the last two fields!)
#
# The columns in the stations table will be
# CODE NAME STATE COUNTRY LATITUDE LONGITUDE ELEVATION

sub dms_to_dec {
  my ($coord) = @_;
  return 0 if $coord !~ /(\d+)-(\d+)(-(\d+))?([NSEW])/;
  my $dec = $1 + $2 / 60;
  if ($4) { $dec += $4 / 3600; }
  if ($5 =~ /[SW]/) { $dec = -$dec; }
  return $dec;
}

print "\nCreating weather stations\n";
($stations, $stations_filename) = tempfile();
open WEATHERSRC, "-|:utf8", "perl ./station-fixups.pl < $weathersrc";
while (<WEATHERSRC>) {
  # remove CRLF and split
  chomp;
  @fields = split /;/;
  next if $#fields < 11;
  
  ($code, $junk, $id, $location, $state, $country, $junk, $latitude, $longitude, $junk, $junk, $elevation) = @fields;

  $latitude = dms_to_dec($latitude);
  $longitude = dms_to_dec($longitude);

  $country_code = $countries{$country};
  if (!$country_code) {
    $alt_country = s/^(.*), (.*)$/$2 $1/;
    $country_code = $countries{$alt_country};
  }
  if (!$country_code) {
    if ($country) { print "No country for '$country'\n"; }
    $country_code = '';
  }
  if ($state) {
    $state_code = $states{$state};
    if (!$state_code) {
      print "No state for '$state'\n";
      next;
    }
  } else {
    $state_code = "";
  }

  print $stations "$code;$location;$state_code;$country_code;$latitude;$longitude;$elevation\n";
}
close WEATHERSRC;
close $stations;

open SQLITE, "|-:utf8", "sqlite3 $locationdb";
print SQLITE "CREATE TABLE stations (code TEXT, name TEXT, state TEXT, country TEXT, latitude REAL, longitude REAL, elevation REAL);\n";
print SQLITE ".separator ';'\n";
print SQLITE ".import $stations_filename stations\n";
print SQLITE "CREATE INDEX station_long_lat ON stations (longitude, latitude);\n";
close SQLITE;

# unlink $stations_filename;
