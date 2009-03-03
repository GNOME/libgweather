#!/usr/bin/perl

binmode STDIN, ":utf8";
binmode STDOUT, ":utf8";

while (<>) {
  ### Drop certain stations
  next if / Platform *;/; # offshort oil platforms
  next if /^LYPZ;/; # buggy duplicate

  ### Whitespace/punctuation cleanup
  s/   */ /g;
  s/\\/\//g;
  s/([^ ])\//$1 \//g;
  s/\/([^ ])/\/ $1/g;
  s/,([^ ])/, $1/g;
  s/ ,/,/g;
  s/[ ,\/]*;/;/g;
  s/; /;/g;

  ### Capitalization, etc
  s/Mc /Mc/g;
  s/ Of / of /g;
  s/([a-z]) D(a |e |el |es |i |o |u |\')/$1 d$2/g;
  s/([a-z]) L(a|es?) /$1 l$2 /g;
  # lowercasify a capital letter after an apostrophe, unless the
  # preceding letter was "d" (eg, "Cote d'Ivoire")
  s/([a-ce-z]\'[A-Z])/\L$1/g; 


  ### Fix incorrect or outdated codes
  if (/;Angelholm;;Sweden;/)   { s/ESDB/ESTA/; }
  if (/;M\. Calamita;;Italy;/) { s/LIRJ/LIRX/; }
  if (/;Yerevan;;Armenia;/)    { s/UGEE/UDYZ/; }
  if (/;Novosibirsk;;Russia;/) { s/UNNN/UNNT/; }
  if (/;Jinan;;China;/)        { s/ZSTN/ZSJN/; }

  ### Fix invalid or incorrect coordinates
  if (/^K3MW;/) { s/;40-26-94N;106-44-95W;/;40-27N;106-45W;/; }
  if (/^KBKB;/) { s/;092-97W;/;093-00W;/; }
  if (/^KBJN;/) { s/;37-37-02;/;37-37-02N;/; }
  if (/^KFHU;/) { s/;46-98N;/;31-35N;/; }
  if (/^KWTR;/) { s/;104-87W;/;105-00W;/; }
  if (/^MMML;/) { s/;117-00W;/;115-14W;/; }
  if (/^MNBL;/) { s/;086-46W;/;083-46W;/; }
  if (/^PGNT;/) { s/;14-96N;/;15-00N;/; }


  ### Fix some country divisions to match FIPS codes
  if (/^EGJA;/) { s/;United Kingdom;/;Guernsey;/; }
  if (/^EGJB;/) { s/;United Kingdom;/;Guernsey;/; }
  if (/^EGJJ;/) { s/;United Kingdom;/;Jersey;/; }
  if (/^EGNS;/) { s/;United Kingdom;/;Isle of Man;/; }
  if (/^EKVG;/) { s/;Denmark;/;Faroe Islands;/; }
  if (/^ENSB;/) { s/;Norway;/;Svalbard;/; }
  if (/^FMCZ;/) { s/;Comoros;/;Mayotte;/; }
  if (/^NLWW;/) { s/;France;/;Wallis and Futuna;/; }
  if (/^PLCH;/) { s/;New Zealand;/;Kiribati;/; }
  if (/^TI..;/) { s/;;Virgin Islands;/;VI;United States;/; }
  if (/^YSNF;/) { s/;Australia;/;Norfolk Island;/; }


  ### Fix some country/state divisions to correct bugs
  if (/^EGYP;/) { s/;South Georgia and the Islands;/;Falkland Islands (Islas Malvinas);/; }
  if (/^HHAS;/) { s/;Ethiopia;/;Eritrea;/; } # 502576
  if (/^LY..;/) { s/;Serbia and Montenegro;/;;/; } # This will force update-locations to figure them out itself
  if (/^NIUE;/) { s/;Cook Islands;/;Niue;/; }
  if (/^NSTU;/) { s/;;United States Minor Outlying Islands;/;AS;United States;/; }
  if (/^NZWD;/) { s/, Antarctic;;New Zealand;/;;Antarctica;/; }
  if (/^PMDY;/) { s/;HI;/;UM;/; }
  if (/^PWAK;/) { s/;GU;/;UM;/; }
  if (/^TKPN;/) { s/;Antigua and Barbuda;/;Saint Kitts and Nevis;/; }
  if (/^YPCC;/) { s/;Christmas Island;/;Cocos (Keeling) Islands;/; }


  ### Fix some spelling mistakes/wackiness/nonstandardnesses. Mostly
  ### alphabetical by station code
  if (/^EDDF;/) { s/ \/ M-Flughafen//; }
  if (/^ETWM;/) { s/-Mil;/ Military Base;/; }
  if (/^FAJS;/) { s/;Johannesburg International Airport;/;O. R. Tambo International Airport;/; } # 533622
  if (/^HESH;/) { s/Sheikhintl/Sheikh Intl/; }
  if (/^HKJK;/) { s/ TWR \/ APP \/ NOF \/ Civil Airlines//; }
  if (/^LBGO;/) { s/Orechovista/Oryakhovitsa/; } # 313655
  if (/^LEJR;/) { s/Fronteraaeropuerto/Frontera Aeropuerto/; }
  if (/^LFPG;/) { s/Paris-Aeroport Charles de Gaulle/Paris, Charles de Gaulle International Airport/; }
  if (/^LIPO;/) { s/Montichia;/Montichiara;/; } # 350945
  if (/^LOAV;/) { s/Lugplatz/Flugplatz/; }
  if (/^MMGL;/) { s/Guadalaj;/Guadalajara;/; }
  if (/^MMMD;/) { s/ lic / /; }
  if (/^MNMG;/) { s/Managua A. C. Sandino/Managua, A. C. Sandino Airport/; }
  if (/^MTPP;/) { s/ \/ Aeroport International/ International Airport/; }
  if (/^MWCR;/) { s/Airportgrand/Airport, Grand/; }
  # 319538 - the entry for OIAG actually has the data for OIAJ
  if (/^OIAG;/) { s/Omidieh/Aghajari/; s/30-46N;049-40E/30-44-44N;049-40-35E/; }
  if (/^OIKB;/) { s/Bandarabbass/Bandar Abbas/; }
  if (/^OINN;/) { s/Noshahr/Now Shahr/; }
  if (/^OITR;/) { s/Orumieh/Orumiyeh/; }
  if (/^PGUM;/) { s/Agana/Hagåtña/; } # to match POP_PLACES
  if (/^SABE;/) { s/Aeroparque Bs\. As\./Buenos Aires, Jorge Newbery/; }
  if (/^SBFZ;/) { s/pinto/Pinto/; }
  if (/^SPHY;/) { s/Andahuayla/Andahuaylas/; }
  if (/^SPIM;/) { s/Aerop\. Internacional Jorgechavez/Jorge Chavez International Airport/; }
  if (/^SVMI;/) { s/Maiquetia Aerop\. Intl\. Simon Bolivar/Simon Bolivar International/; }
  if (/^TAPA;/) { s/Vc /V. C. /; }
  if (/^TKPN;/) { s/Newcast;/Newcastle;/; }
  if (/^UBBG;/) { s/Gyanca/Gyandzha/; }
  if (/^UKDR;/) { s/Krivyy/Kryvyy/; }


  ### "Move" some stations to keep them from matching irrelevant cities
  if (/^VTBD;/) { s/Don Muang/Bangkok/; }


  ### Untranslate/unabbreviate the word "Airport". (The names in
  ### nsd_cccc.txt don't seem to be especially close to
  ### correct/official, so this is a net win.
  s/Aerop\. Internacional ([^,;]*)/$1 International Airport/;
  s/Aeropuerto[^ ]* ([^,;]*)/$1 Airport/;
  s/Aeroporto* d[ea] ([^,;]*)/$1 Airport/;
  s/[ -]Aero(|\.|drome|porto?|-Porto|puerto)( |;)/ Airport$2/;
  s/Air(-Port|p\.)/Airport/;
  s/Civ \/ (Mil|Afb)/Airport/;
  s/( \/)? Civ(|il|ilian);/$1 Airport;/;
  s/Lufthavn/Airport/;
  s/Int\'?l\.?/International/;
  s/Int\./International/;
  s/Inter-National/International/;
  s/Internationalairport;/International Airport;/;
  s/International;/International Airport;/;
  s/Airport ([A-Z])/Airport, $1/;
  # Change "Foo / Airport" to "Foo Airport"
  s/;([^;]*)(,| \/) (International Airport|Airport);/;$1 $3;/;
  # And "Foo / Bar Airport" to "Foo, Bar Airport"
  s/;([^;\/,]*) \/ ([^;\/,]* Airport)/;$1, $2/;

  s/,? ([a-z][a-z]*-)?afb/ Air Force Base/i;
  s/ ([A-Z][a-z]*-)?Ab;/ Air Base;/;
  s/Usa . Af/US Air Force Base/;
  s/Usaf/US Air Force Base/;
  s/Air Force Operated Base In Foreign Country/Air Force Base/;
  s/ (Can-)?Mil(\.|itary);/ Military Base;/;

  s/Obs(\.|erv\.|ervatory|ervatorio)/Observatory/;

  # US National Weather Service, but appears not just in /;United States;/
  s/, NWS Office//;
  # Likewise Australian Weather Service (or Automated Weather Station?)
  s/,? Aws;/;/;

  ### Country-specific fixups, sorted alphabetically by country

  if (/;Argentina;/) {
    # Remove province name from location description
    s/, (BA|B\. A\.|CHT|SF);/;/;
  }

  if (/;Australia;/) {
    # ??
    s/ (Amo|Mo);/ Airport;/;
    s/,? M\. O\.?;/;/;

    s/ Ran / Royal Australian Navy /;
  }

  if (/;Austria;/) {
    s/-Flughafen/ Airport/;
    s/Flugplatz/Airport/;
    s/ Am / am /;
    s/ Im / im /;
  }

  if (/;Canada;/) {
    # Remove province/territory name from location description
    s/,? (Alta|B\. C|Man|N\. B|Nfld|N\. S|N\. W\. T|Ont|P\. E\. I|Prince Edward Island|Que|Sask|Y\. T)\.?;/;/;

    # Canadian Department of Agriculture, [Remote] Climate Station
    s/ Cda//i;
    s/ R?CS//;

    s/Airport, [^;]*Station;/Airport;/;

    # /CX../ stations are automated. Maybe we should drop all of them,
    # but for now we'll just drop the ones where there's also a
    # corresponding non-automated station
    if (/^CX(DE|EC|EG|MI|MM|OX|TV|WN|ZU)/) {
      next;
    }
  }

  if (/;Cuba;/) {
    s/, Oriente//;
  }

  if (/;Mexico;/) {
    # Remove state name from location description
    s/,? (Ags|B\. C\. S|Camp|Chis|Coah|Mor|N\. L|Nay|Pue|Q\. Roo|Qro|S\. L\. P|Sin|Son)\.?;/;/;
  }

  if (/;Netherlands;/) {
    s/;([^;]*) Airport, ([^;]*);/;$1, $2 Airport;/;
  }

  if (/;New Caledonia;/) {
    s/ Nlle-Caledonie//;
    s/ Ile [^;]*//;
  }

  if (/;Sweden;/) {
    s/Flygplats/Airport/;
  }

  if (/;United States;/) {
    s/Nexrad/NEXRAD Station/;
  }

  ### Final airport fixing...

  ### The location data for several countries (including all of South
  ### America) uses the convention "City / Airport Name"
  if (/^(AG|AY|DA|EL|FC|FMM|FO|FX|FZ|GA|GM|HE|LI|LJ|MK|MM|MP|OI|PL|S|US|UU|UW|VV|WA|WI|WM|WR|WS|Z)/) {
    s/;([^;]*) \/ ([^;\/]*)/;$1, $2 Airport/;
  }

  ### Some do it backwards
  if (/^(EN|LE|MS|TT)/) {
    s/;([^;]*) \/ ([^;\/]*)/;$1 Airport, $2/;
  }

  ### In some countries, you generally need to prefix the city name to
  ### the airport name
  if (/^(C|ES|GO|LF|LH|LK|LS|LT|LZ)/) {
    s/;([^;]*) \/ ([^;\/]*)/;$1, $1-$2 Airport/;
  }

  # Some of our fixes end up resulting in "Airport Airport"
  s/Airport Airport/Airport/;

  # Remove numbers in "Foo 1", "Foo 2", "Foo Iii", etc
  s/ [123];/;/;
  s/ I[iv]i*([^a-z])/$1/;

  print;
}
