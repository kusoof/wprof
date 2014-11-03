#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Count commits by the commit queue."""

import datetime
import json
import logging
import optparse
import os
import re
import sys
from xml.etree import ElementTree

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import find_depot_tools  # pylint: disable=W0611
import subprocess2


def log(repo, args):
  """If extra is True, grab one revision before and one after."""
  args = args or []
  out = subprocess2.check_output(
      ['svn', 'log', '--with-all-revprops', '--xml', repo] + args)
  data = {}
  for logentry in ElementTree.XML(out).findall('logentry'):
    date_str = logentry.find('date').text
    date = datetime.datetime(*map(int, re.split('[^\d]', date_str)[:-1]))
    entry = {
        'author': logentry.find('author').text,
        'date': date,
        'msg': logentry.find('msg').text,
        'revprops': {},
        'commit-bot': False,
    }
    revprops = logentry.find('revprops')
    if revprops is not None:
      for revprop in revprops.findall('property'):
        entry['revprops'][revprop.attrib['name']] = revprop.text
        if revprop.attrib['name'] == 'commit-bot':
          entry['commit-bot'] = True
    data[logentry.attrib['revision']] = entry
  return data


def log_dates(repo, start_date, days):
  """Formats dates so 'svn log' does the right thing. Fetches everything in UTC.
  """
  # http://svnbook.red-bean.com/nightly/en/svn-book.html#svn.tour.revs.dates
  end_inclusive = start_date + datetime.timedelta(days=days)
  print('Getting data from %s for %d days' % (start_date, days))
  range_str = (
      '{%s 00:00:00 +0000}:{%s 00:00:00 +0000}' % (start_date, end_inclusive))
  data = log(repo, ['-r', range_str])
  # Strip off everything outside the range.
  start_date_time = datetime.datetime(*start_date.timetuple()[:6])
  if data:
    first = sorted(data.keys())[0]
    if data[first]['date'] < start_date_time:
      del data[first]
  return data


def monday_last_week():
  """Returns Monday in 'date' object."""
  today = datetime.date.today()
  last_week = today - datetime.timedelta(days=7)
  return last_week - datetime.timedelta(days=(last_week.isoweekday() - 1))


class JSONEncoder(json.JSONEncoder):
  def default(self, o):  # pylint: disable=E0202
    if isinstance(o, datetime.datetime):
      return str(o)
    return super(JSONEncoder, self)


def main():
  parser = optparse.OptionParser(
      description=sys.modules['__main__'].__doc__)
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option(
    '-r', '--repo', default='http://src.chromium.org/svn/trunk/src')
  parser.add_option('-s', '--since', action='store')
  parser.add_option('-d', '--days', type=int, default=7)
  parser.add_option('--all', action='store_true', help='Get ALL the revisions!')
  parser.add_option('--dump', help='Dump json in file')
  parser.add_option('-o', '--stats_only', action='store_true')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported args: %s' % args)
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)
  else:
    logging.basicConfig(level=logging.ERROR)
  # By default, grab stats for last week.
  if not options.since:
    options.since = monday_last_week()
  else:
    options.since = datetime.date(*map(int, re.split('[^\d]', options.since)))
  if options.all:
    log_data = log(options.repo, [])
  else:
    log_data = log_dates(options.repo, options.since, options.days)

  if options.dump:
    json.dump(log_data, open(options.dump, 'w'), cls=JSONEncoder)

  # Calculate stats.
  num_commit_bot = len([True for v in log_data.itervalues() if v['commit-bot']])
  num_total_commits = len(log_data)
  pourcent = 0.
  if num_total_commits:
    pourcent = float(num_commit_bot) * 100. / float(num_total_commits)
  users = {}
  for i in log_data.itervalues():
    if i['commit-bot']:
      users.setdefault(i['author'], 0)
      users[i['author']] += 1

  if not options.stats_only:
    max_author_len = max(len(i['author']) for i in log_data.itervalues())
    for revision in sorted(log_data.keys()):
      entry = log_data[revision]
      commit_bot = ' '
      if entry['commit-bot']:
        commit_bot = 'c'
      print('%s  %s  %s  %*s' % (
        ('r%s' % revision).rjust(6),
        commit_bot,
        entry['date'].strftime('%Y-%m-%d %H:%M UTC'),
        max_author_len,
        entry['author']))

  print('')
  print('Top users')
  max_author_len = max(len(i) for i in users)

  for author, count in sorted(
      users.iteritems(), key=lambda x: x[1], reverse=True):
    print('%*s: %d' % (max_author_len, author, count))

  print('')
  print('Stats')
  print('Total commits: %d' % num_total_commits)
  print('Total commits by commit bot: %d (%.1f%%)' % (num_commit_bot, pourcent))
  return 0


if __name__ == '__main__':
  sys.exit(main())
