#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Commit queue executable.

Reuse Rietveld and the Chromium Try Server to process and automatically commit
patches.
"""

import logging
import optparse
import os
import sys
import time
import traceback

import find_depot_tools  # pylint: disable=W0611
import checkout
import fix_encoding
import rietveld
import subprocess2

import async_push
import creds
import errors
import projects


class ReadOnlyRietveld(object):
  def __init__(self, obj):
    self._obj = obj
    self._original_send = self._obj._send  # pylint: disable=W0212
    self._obj._send = self._send

  def _send(self, request_path, **kwargs):
    """Ignore all post requests."""
    if kwargs.get('payload'):
      return
    return self._original_send(request_path, **kwargs)

  def __getattr__(self, attr):
    return getattr(self._obj, attr)


class FakeCheckout(object):
  def __init__(self):
    self.project_path = os.getcwd()
    self.project_name = os.path.basename(self.project_path)

  @staticmethod
  def prepare(_revision):
    logging.info('FakeCheckout is syncing')
    return 'FAKE'

  @staticmethod
  def apply_patch(*_args):
    logging.info('FakeCheckout is applying a patch')

  @staticmethod
  def commit(*_args):
    logging.info('FakeCheckout is committing patch')
    return 'FAKED'

  @staticmethod
  def get_settings(_key):
    return None


def main():
  parser = optparse.OptionParser(
      description=sys.modules['__main__'].__doc__)
  project_choices = projects.supported_projects()
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option(
      '--no-dry-run',
      action='store_false',
      dest='dry_run',
      default=True,
      help='Run for real instead of dry-run mode which is the default')
  parser.add_option(
      '--fake',
      action='store_true',
      help='Run with a fake checkout to speed up testing')
  parser.add_option(
      '--no-try',
      action='store_true',
      help='Don\'t send try jobs.')
  parser.add_option(
      '-p',
      '--poll-interval',
      type='int',
      default=10,
      help='Minimum delay between each polling loop, default: %default')
  parser.add_option(
      '--query-only',
      action='store_true',
      help='Return internal state')
  parser.add_option(
      '--project',
      choices=project_choices,
      help='Project to run the commit queue against: %s' %
           ', '.join(project_choices))
  parser.add_option(
      '-u',
      '--user',
      default='commit-bot@chromium.org',
      help='User to use instead of %default')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported args: %s' % args)
  if not options.project:
    parser.error('Need to pass a valid project to --project.\nOptions are: %s' %
        ', '.join(project_choices))
  if options.verbose:
    level = logging.DEBUG
  else:
    level = logging.INFO
  logging.basicConfig(
      level=level,
      format='%(asctime)s %(levelname)7s %(message)s')
  try:
    root_dir = os.path.dirname(os.path.abspath(__file__))
    work_dir = os.path.join(root_dir, 'workdir')
    # Use our specific subversion config.
    checkout.SvnMixIn.svn_config = checkout.SvnConfig(
        os.path.join(root_dir, 'subversion_config'))

    gaia_creds = creds.Credentials(os.path.join(work_dir, '.gaia_pwd'))
    rietveld_obj = rietveld.Rietveld(
        'https://chromiumcodereview.appspot.com',
        options.user,
        gaia_creds.get(options.user))
    pc = projects.load_project(
        options.project,
        options.user,
        work_dir,
        rietveld_obj,
        options.no_try)

    if options.dry_run:
      if options.fake:
        # Disable the checkout.
        pc.context.checkout = FakeCheckout()
      else:
        pc.context.checkout = checkout.ReadOnlyCheckout(pc.context.checkout)
      # Make sure rietveld is not modified and save pushed events on disk.
      pc.context.rietveld = ReadOnlyRietveld(pc.context.rietveld)
      pc.context.status = async_push.AsyncPushStore()

    db_path = os.path.join(work_dir, pc.context.checkout.project_name + '.json')
    if os.path.isfile(db_path):
      pc.load(db_path)

    # Sync every 5 minutes.
    SYNC_DELAY = 5*60
    try:
      if options.query_only:
        pc.look_for_new_pending_commit()
        pc.update_status()
        print(str(pc.queue))
        return 0

      now = time.time()
      next_loop = now + options.poll_interval
      # First sync is on second loop.
      next_sync = now + options.poll_interval * 2
      while True:
        # In theory, we would gain in performance to parallelize these tasks. In
        # practice I'm not sure it matters.
        pc.look_for_new_pending_commit()
        pc.process_new_pending_commit()
        pc.update_status()
        pc.scan_results()

        # More than a second to wait and due to sync.
        now = time.time()
        if (next_loop - now) >= 1 and (next_sync - now) <= 0:
          if sys.stdout.isatty():
            sys.stdout.write('Syncing while waiting                \r')
            sys.stdout.flush()
          try:
            pc.context.checkout.prepare(None)
          except subprocess2.CalledProcessError, e:
            # Don't crash, most of the time it's the svn server that is dead.
            # How fun. Send a stack trace to annoy the maintainer.
            errors.send_stack(e)
          next_sync = time.time() + SYNC_DELAY

        # Always wait at least one second to make Ctrl-C'ing easier.
        now = time.time()
        next_loop = max(now + 1, next_loop)
        while True:
          delay = next_loop - now
          if delay <= 0:
            break
          if sys.stdout.isatty():
            sys.stdout.write('Sleeping for %1.1f seconds          \r' % delay)
            sys.stdout.flush()
          time.sleep(min(delay, 0.1))
          now = time.time()
        if sys.stdout.isatty():
          sys.stdout.write('Running (please do not interrupt)   \r')
          sys.stdout.flush()
        next_loop = time.time() + options.poll_interval
    finally:
      print >> sys.stderr, 'Saving db...     '
      pc.save(db_path)
      print >> sys.stderr, 'Done!            '
  except KeyboardInterrupt, e:
    print 'Bye bye'
    # 23 is an arbitrary value to signal loop.sh that it must stop looping.
    return 23
  except SystemExit, e:
    traceback.print_exc()
    print >> sys.stderr, ('Tried to exit: %s', e)
    return e.code
  except errors.ConfigurationError, e:
    parser.error(str(e))
    return 1
  return 0


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  sys.exit(main())
