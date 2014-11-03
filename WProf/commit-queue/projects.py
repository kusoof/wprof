# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Define the supported projects."""

import json
import logging
import os
import re
import sys
import urllib2

import find_depot_tools  # pylint: disable=W0611
import checkout

import async_push
import context
import creds
import errors
import pending_manager
from post_processors import chromium_copyright
from verification import presubmit_check
from verification import project_base
from verification import reviewer_lgtm
from verification import tree_status
from verification import try_server

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(ROOT_DIR, '..', 'commit-queue-internal'))

# These come from commit-queue in the internal repo.
try:
  import chromium_committers  # pylint: disable=F0401
  import gyp_committers  # pylint: disable=F0401
  import nacl_committers  # pylint: disable=F0401
except ImportError, e:
  print >> sys.stderr, (
      'Failed to find commit-queue-internal, will fail to start: %s' % e)
  chromium_committers = None
  gyp_committers = None
  nacl_committers = None


# It's tricky here because 'chrome' is remapped to 'svn' on src.chromium.org but
# the other repositories keep their repository name. So don't list it here.
SVN_HOST_ALIASES = [
    'svn://svn.chromium.org',
    'svn://chrome-svn',
    'svn://chrome-svn.corp',
    'svn://chrome-svn.corp.google.com'
]

CHROME_SVN_BASES = [item + '/chrome' for item in SVN_HOST_ALIASES] + [
    'http://src.chromium.org/svn',
    'https://src.chromium.org/svn',
    'http://src.chromium.org/chrome',
    'https://src.chromium.org/chrome',
]

IGNORED_STEPS = (
  'svnkill', 'update_scripts', 'taskkill', 'cleanup_temp', 'process_dumps')


def _read_lines(filepath, what):
  try:
    return open(filepath).readlines()
  except IOError:
    raise errors.ConfigurationError('Put the %s in %s' % (what, filepath))


def _get_chromium_committers():
  """Gets the list of all allowed committers."""
  if not chromium_committers:
    # Fake values.
    entries = ['georges']
  else:
    entries = chromium_committers.get_list()
  logging.info('Found %d committers' % len(entries))
  return ['^%s$' % re.escape(i) for i in entries]


def _get_nacl_committers():
  """Gets the list of all allowed committers."""
  if not nacl_committers:
    # Fake values.
    entries = ['georges']
  else:
    entries = nacl_committers.get_list()
  logging.info('Found %d committers' % len(entries))
  return ['^%s$' % re.escape(i) for i in entries]


def _get_gyp_committers():
  """Gets the list of all allowed committers."""
  if not gyp_committers:
    # Fake values.
    entries = ['georges']
  else:
    entries = gyp_committers.get_list()
  logging.info('Found %d committers' % len(entries))
  return ['^%s$' % re.escape(i) for i in entries]


def _chromium_lkgr():
  try:
    return int(
        urllib2.urlopen('https://chromium-status.appspot.com/lkgr').read())
  except (ValueError, IOError):
    return None


def _nacl_lkgr():
  try:
    return int(
        urllib2.urlopen('https://nativeclient-status.appspot.com/lkgr').read())
  except (ValueError, IOError):
    return None


def _chromium_status_pwd(root_dir):
  filepath = os.path.join(root_dir, '.chromium_status_pwd')
  return _read_lines(filepath, 'chromium-status password')[0].strip()


def _gen_chromium(user, root_dir, rietveld_obj, no_try):
  """Generates a PendingManager commit queue for chrome/trunk/src."""
  svn_creds = creds.Credentials(os.path.join(root_dir, '.svn_pwd'))
  local_checkout = checkout.SvnCheckout(
      root_dir,
      'chromium',
      user,
      svn_creds.get(user),
      'svn://svn.chromium.org/chrome/trunk/src',
      [chromium_copyright.process])
  context_obj = context.Context(
      rietveld_obj,
      local_checkout,
      async_push.AsyncPush(
        'https://chromium-status.appspot.com/cq',
        _chromium_status_pwd(root_dir)))

  project_bases = [
      '^%s/trunk/src(|/.*)$' % re.escape(base) for base in CHROME_SVN_BASES]

  aliases = (
    # Old path.
    'git.chromium.org/git/chromium.git',
    # New path.
    'git.chromium.org/chromium/src.git',
    'git.chromium.org/git/chromium/src',
    'git.chromium.org/git/chromium/src.git',
  )
  aliases_urls = sum((['https://' + i, 'http://' + i] for i in aliases), [])
  project_bases.extend(r'^%s\@[a-z\-_]+$' % re.escape(i) for i in aliases_urls)
  verifiers_no_patch = [
      project_base.ProjectBaseUrlVerifier(project_bases),
      reviewer_lgtm.ReviewerLgtmVerifier(
          _get_chromium_committers(),
          [re.escape(user)]),
  ]
  verifiers = [
      presubmit_check.PresubmitCheckVerifier(context_obj),
  ]
  if not no_try:
    # To add tests to this list, they MUST be in
    # /chrome/trunk/tools/build/masters/master.chromium/master_gatekeeper_cfg.py
    # or somehow close the tree whenever they break.
    standard_tests = (
        'base_unittests',
        'browser_tests',
        'check_deps',
        'cacheinvalidation_unittests',
        'chrome_frame_net_tests',
        'chrome_frame_unittests',
        'content_unittests',
        'crypto_unittests',
        'gfx_unittests',
        # Broken in release.
        #'googleurl_unittests',
        'gpu_unittests',
        'ipc_tests',
        'interactive_ui_tests',
        'installer_util_unittests',
        'jingle_unittests',
        'media_unittests',
        'mini_installer_test',
        #'nacl_integration',
        'net_unittests',
        'printing_unittests',
        # Too flaky.
        #'pyauto_functional_tests',
        'remoting_unittests',
        'safe_browsing_tests',
        'sql_unittests',
        'sync_unit_tests',
        # Tends to be broken by webkit roll and not fixed fast enough.
        #'test_shell_tests',
        'unit_tests',
        'ui_tests',
        'views_unittests',
        #'webkit_unit_tests',
    )
    builders_and_tests = {
      # TODO(maruel): Figure out a way to run 'sizes' where people can
      # effectively update the perf expectation correctly.  This requires a
      # clobber=True build running 'sizes'. 'sizes' is not accurate with
      # incremental build. Reference:
      # http://chromium.org/developers/tree-sheriffs/perf-sheriffs.
      # TODO(maruel): An option would be to run 'sizes' but not count a failure
      # of this step as a try job failure.
      'linux_rel': standard_tests,
      'mac_rel': standard_tests,
      'win_rel': standard_tests,
      'win': ['compile'],
      'linux_clang': ['compile'],
      'linux_chromeos': ['compile'],
      # tests are ignored for the annotated build of android trybots at this
      # time.
      'android': [],
    }
    verifiers.append(try_server.TryRunner(
        context_obj,
        'http://build.chromium.org/p/tryserver.chromium/',
        user,
        builders_and_tests,
        IGNORED_STEPS,
        ['--root', 'src'],
        _chromium_lkgr,
        'src'))

  verifiers.append(tree_status.TreeStatusVerifier(
      'http://chromium-status.appspot.com'))
  return pending_manager.PendingManager(
      context_obj,
      verifiers_no_patch,
      verifiers)


def _gen_nacl(user, root_dir, rietveld_obj, no_try):
  """Generates a PendingManager commit queue for Native Client."""
  svn_creds = creds.Credentials(os.path.join(root_dir, '.svn_pwd'))
  offset = 'trunk/src/native_client'
  local_checkout = checkout.SvnCheckout(
      root_dir,
      'nacl',
      user,
      svn_creds.get(user),
      'svn://svn.chromium.org/native_client/' + offset)
  context_obj = context.Context(
      rietveld_obj,
      local_checkout,
      async_push.AsyncPush(
        'https://nativeclient-status.appspot.com/cq',
        _chromium_status_pwd(root_dir)))

  host_aliases = SVN_HOST_ALIASES + [
      'http://src.chromium.org', 'https://src.chromium.org']
  svn_bases = [i + '/native_client' for i in host_aliases]
  project_bases = [
      '^%s/%s(|/.*)$' % (re.escape(base), offset) for base in svn_bases
  ]
  git_url = (
      'http://git.chromium.org/native_client/src/native_client.git@')
  project_bases.append('^%s[a-z\\-_]+$' % re.escape(git_url))
  verifiers_no_patch = [
      project_base.ProjectBaseUrlVerifier(project_bases),
      reviewer_lgtm.ReviewerLgtmVerifier(
          _get_nacl_committers(),
          [re.escape(user)]),
  ]
  verifiers = [
      # Disable Native Client presubmit check until it works in a stand-alone
      # checkout and doesn't have side-effects.
      #presubmit_check.PresubmitCheckVerifier(context_obj),
  ]
  if not no_try:
    # Grab the list of all the builders here. The commit queue needs to know
    # which builders were triggered. TODO: makes this more automatic.
    url = 'http://build.chromium.org/p/tryserver.nacl/json/builders'
    builders_and_tests = dict(
      (key, []) for key in json.load(urllib2.urlopen(url))
      if (key.startswith('nacl-') and
          'toolchain' not in key and
          'valgrind' not in key and
          'perf_panda' not in key and
          'arm_hw' not in key and
          'shared' not in key and
          'coverage' not in key)
    )
    verifiers.append(try_server.TryRunner(
        context_obj,
        'http://build.chromium.org/p/tryserver.nacl/',
        user,
        builders_and_tests,
        IGNORED_STEPS,
        ['--root', 'native_client'],
        _nacl_lkgr,
        'native_client'))

  verifiers.append(tree_status.TreeStatusVerifier(
      'http://nativeclient-status.appspot.com'))
  return pending_manager.PendingManager(
      context_obj,
      verifiers_no_patch,
      verifiers)


def _gen_gyp(user, root_dir, rietveld_obj, no_try):
  """Generates a PendingManager commit queue for GYP."""
  svn_creds = creds.Credentials(os.path.join(root_dir, '.svn_pwd'))
  naked_url = '://gyp.googlecode.com/svn/trunk'
  local_checkout = checkout.SvnCheckout(
      root_dir,
      'gyp',
      user,
      svn_creds.get(user),
      'https' + naked_url)
  context_obj = context.Context(
      rietveld_obj,
      local_checkout,
      async_push.AsyncPush(
        'https://chromium-status.appspot.com/cq/receiver',
        _chromium_status_pwd(root_dir)))

  project_bases = [
      '^%s(|/.*)$' % re.escape(base + naked_url) for base in ('http', 'https')
  ]
  verifiers_no_patch = [
      project_base.ProjectBaseUrlVerifier(project_bases),
      reviewer_lgtm.ReviewerLgtmVerifier(
          _get_gyp_committers(),
          [re.escape(user)]),
  ]
  verifiers = []
  if not no_try:
    # Grab the list of all the builders here. The commit queue needs to know
    # which builders were triggered. TODO: makes this more automatic.
    # GYP is using the Nacl try server.
    url = 'http://build.chromium.org/p/tryserver.nacl/json/builders'
    builders_and_tests = dict(
      (key, []) for key in json.load(urllib2.urlopen(url))
      if key.startswith('gyp-')
    )
    verifiers.append(try_server.TryRunner(
        context_obj,
        'http://build.chromium.org/p/tryserver.nacl/',
        user,
        builders_and_tests,
        IGNORED_STEPS,
        ['--root', 'gyp'],
        lambda: None,
        'gyp'))

  verifiers.append(tree_status.TreeStatusVerifier(
      'http://gyp-status.appspot.com/status'))
  return pending_manager.PendingManager(
      context_obj,
      verifiers_no_patch,
      verifiers)


def _gen_tools(user, root_dir, rietveld_obj, _no_try):
  """Generates a PendingManager commit queue for everything under
  chrome/trunk/tools.

  These don't have a try server but have presubmit checks.
  """
  # Ignore no_try.
  path = 'tools'
  project_bases = [
      '^%s/trunk/%s(|/.*)$' % (re.escape(base), path)
      for base in CHROME_SVN_BASES
  ]
  aliases = (
    re.escape('git.chromium.org/chromium/tools/') +
        r'([a-z\-_]+)\.git\@[a-z\-_]+',
    re.escape('git.chromium.org/git/chromium/tools/') +
        r'([a-z\-_]+)\@[a-z\-_]+',
    re.escape('git.chromium.org/git/chromium/tools/') +
        r'([a-z\-_]+)\.git\@[a-z\-_]+',
  )
  aliases_urls = sum(
      ([r'https\:\/\/' + i, r'http\:\/\/' + i] for i in aliases), [])
  project_bases.extend('^%s$' % i for i in aliases_urls)
  return _internal_simple(path, project_bases, user, root_dir, rietveld_obj)


def _gen_chromium_deps(user, root_dir, rietveld_obj, _no_try):
  """Generates a PendingManager commit queue for
  chrome/trunk/deps/.
  """
  # Ignore no_try.
  path = 'deps'
  project_bases = [
      '^%s/trunk/%s(|/.*)$' % (re.escape(base), path)
      for base in CHROME_SVN_BASES
  ]
  return _internal_simple(path, project_bases, user, root_dir, rietveld_obj)


def _internal_simple(path, project_bases, user, root_dir, rietveld_obj):
  """Generates a PendingManager commit queue for chrome/trunk/tools/build."""
  svn_creds = creds.Credentials(os.path.join(root_dir, '.svn_pwd'))
  local_checkout = checkout.SvnCheckout(
      root_dir,
      os.path.basename(path),
      user,
      svn_creds.get(user),
      'svn://svn.chromium.org/chrome/trunk/' + path,
      [chromium_copyright.process])
  context_obj = context.Context(
      rietveld_obj,
      local_checkout,
      async_push.AsyncPush(
        'https://chromium-status.appspot.com/cq',
        _chromium_status_pwd(root_dir)))

  verifiers_no_patch = [
      project_base.ProjectBaseUrlVerifier(project_bases),
      reviewer_lgtm.ReviewerLgtmVerifier(
          _get_chromium_committers(),
          [re.escape(user)]),
  ]
  verifiers = [
      presubmit_check.PresubmitCheckVerifier(context_obj, timeout=900),
  ]

  return pending_manager.PendingManager(
      context_obj,
      verifiers_no_patch,
      verifiers)


def supported_projects():
  """List the projects that can be managed by the commit queue."""
  return sorted(
      x[5:] for x in dir(sys.modules[__name__]) if x.startswith('_gen_'))


def load_project(project, user, root_dir, rietveld_obj, no_try):
  """Loads the specified project."""
  return getattr(sys.modules[__name__], '_gen_' + project)(
      user, root_dir, rietveld_obj, no_try)
