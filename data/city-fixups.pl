#!/usr/bin/perl

binmode STDIN, ":utf8";
binmode STDOUT, ":utf8";

while (<>) {
  # Drop apparently-duplicate cities
  next if /-364867.*Tacna/; # Tacna, Peru
  next if /6034170.*Bujumbura/; # Bujumbura, Burundi
  next if /-225337.*Akita/; # Akita, Japan
  next if /10074674.*\tIR\t/; # Tehran, Iran
  next if /-3414440.*\tTH\t/; # Bangkok, Thailand
  next if /9026906.*\tWarszawa/; # Warsaw, Poland

  # "Zürich" should be listed with an English name of "Zurich"
  if (/-2554935.*Zürich/) {
    # print the local name
    print;
    # change the data, then fall through to print that as well
    s/\tN\t\t/\tC\teng\t/;
    s/Zürich/Zurich/;
  }

  # The capital of Mexico should be "Mexico City" in English, not just
  # "Mexico". #171718
  if (/-1658079.*\tC\teng/) {
    s/MEXICO\tMexico\tMexico\t/MEXICOCITY\tMexico City\tMexico City\t/;
  }

  print;
}
