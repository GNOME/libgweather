#!/usr/bin/python3

from ftplib import FTP
import datetime
import os
import re
import sys

file = sys.argv[1]
file_0 = '%s.0' % file
file_1 = '%s.1' % file
file_2 = '%s.2' % file

# Rename and read in earlier observations
os.rename(file_1, file_2)
os.rename(file_0, file_1)
f = open(file_1)
recent_1 = [line.rstrip() for line in f.readlines()]
f.close()
f = open(file_2)
recent_2 = [line.rstrip() for line in f.readlines()]
f.close()

# Create the regular expression for matching recent observations.
# The listing looks like:
#   -rw-r--r--    1 3110     0              63 May 20 17:05 ZYTX.TXT
# We use ctime rather than strftime to avoid the month being localized

today = datetime.date.today()
stamp1 = today.ctime()[4:10].replace('  ', ' 0')
yesterday = today - datetime.timedelta(1)
stamp2 = yesterday.ctime()[4:10].replace('  ', ' 0')
rex = re.compile("(%s|%s) \\d\\d:\\d\\d (....)\\.TXT$" % (stamp1, stamp2))

recent_0 = []
f = open(file_0, 'w')

def check_observation(line):
    match = rex.search(line)
    if match is None:
        return

    obs = match.group(2)
    recent_0.append(obs)
    f.write('%s\n' % obs)

ftp = FTP('tgftp.nws.noaa.gov')
ftp.login()
ftp.cwd('/data/observations/metar/stations')
ftp.retrlines('LIST', check_observation)
ftp.quit()
f.close()

if len(recent_0) < 3000:
    sys.stderr.write('Error parsing METAR observations!\n')
    sys.exit(1)

recent_0.sort()

recent = []
for obs in recent_0:
    if obs in recent_1 and obs in recent_2:
        recent.append(obs)

f = open(file, 'w')
for obs in recent:
    f.write('%s\n' % obs)
f.close()
