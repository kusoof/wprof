#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration tests for project.py."""

import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT_DIR, '..'))

import projects
from verification import presubmit_check
from verification import tree_status

# From /tests
import mocks


def _try_comment(pc, issue=31337):
  return (
      "add_comment(%d, '%shttp://localhost/user@example.com/%d/1\\n')" %
      (issue, pc.TRYING_PATCH.replace('\n', '\\n'),
        issue))


class CredentialsMock(object):
  def __init__(self, _):
    pass

  @staticmethod
  def get(user):
    return '1%s1' % user


class TestCase(mocks.TestCase):
  def setUp(self):
    super(TestCase, self).setUp()
    self.read_lines = []
    self.mock(projects.creds, 'Credentials', CredentialsMock)
    self.mock(projects, '_read_lines', self._read_lines)
    class Dummy(object):
      @staticmethod
      def get_list():
        return []
    if not projects.chromium_committers:
      projects.chromium_committers = Dummy()
    self.mock(
        projects.chromium_committers, 'get_list', self._get_committers_list)
    if not projects.nacl_committers:
      projects.nacl_committers = Dummy()
    self.mock(projects.nacl_committers, 'get_list', self._get_committers_list)
    self.mock(tree_status.TreeStatus, 'postpone', lambda _: False)
    self.mock(
        presubmit_check.subprocess2,
        'check_output',
        lambda *args, **kwargs: self._check_output.pop(0))
    self._check_output = []

  def tearDown(self):
    if not self.has_failed():
      self.assertEquals([], self.read_lines)
    super(TestCase, self).tearDown()

  @staticmethod
  def _get_committers_list():
    return ['user@example.com', 'user@example.org']

  def _read_lines(self, root, error):
    a = self.read_lines.pop(0)
    self.assertEquals(a[0], root)
    self.assertEquals(a[1], error)
    return a[2]


class ProjectTest(TestCase):
  def test_loaded(self):
    members = (
        'chromium', 'chromium_deps', 'gyp', 'nacl', 'tools')
    self.assertEquals(sorted(members), sorted(projects.supported_projects()))

  def test_all(self):
    # Make sure it's possible to load each project.
    mapping = {
      'chromium': {
        'lines': [
          'root_dir/.chromium_status_pwd', 'chromium-status password', ['foo'],
        ],
        'pre_patch_verifiers': ['project_bases', 'reviewer_lgtm'],
        'verifiers': ['presubmit', 'tree status'],
      },
      'chromium_deps': {
        'lines': [
          'root_dir/.chromium_status_pwd', 'chromium-status password', ['foo'],
        ],
        'pre_patch_verifiers': ['project_bases', 'reviewer_lgtm'],
        'verifiers': ['presubmit'],
      },
      'gyp': {
        'lines': [
          'root_dir/.chromium_status_pwd', 'chromium-status password', ['foo'],
        ],
        'pre_patch_verifiers': ['project_bases', 'reviewer_lgtm'],
        'verifiers': ['tree status'],
      },
      'nacl': {
        'lines': [
          'root_dir/.chromium_status_pwd', 'chromium-status password', ['foo'],
        ],
        'pre_patch_verifiers': ['project_bases', 'reviewer_lgtm'],
        'verifiers': ['tree status'],
      },
      'tools': {
        'lines': [
          'root_dir/.chromium_status_pwd', 'chromium-status password', ['foo'],
        ],
        'pre_patch_verifiers': ['project_bases', 'reviewer_lgtm'],
        'verifiers': ['presubmit'],
      },
    }
    for project in sorted(projects.supported_projects()):
      logging.debug(project)
      self.assertEquals([], self.read_lines)
      expected = mapping.pop(project)
      self.read_lines = [expected['lines']]
      p = projects.load_project(
          project, 'user', 'root_dir', self.context.rietveld, True)
      self.assertEquals(
          expected['pre_patch_verifiers'],
          [x.name for x in p.pre_patch_verifiers],
          (expected['pre_patch_verifiers'],
           [x.name for x in p.pre_patch_verifiers],
           project))
      self.assertEquals(
          expected['verifiers'], [x.name for x in p.verifiers],
          (expected['verifiers'],
           [x.name for x in p.verifiers],
           project))
      if project == 'tools':
        self.maxDiff = None
        # Add special checks for it.
        project_bases_verifier = p.pre_patch_verifiers[0]
        self.assertEquals(
            [
              '^svn\\:\\/\\/svn\\.chromium\\.org\\/chrome/trunk/tools(|/.*)$',
              '^svn\\:\\/\\/chrome\\-svn\\/chrome/trunk/tools(|/.*)$',
              '^svn\\:\\/\\/chrome\\-svn\\.corp\\/chrome/trunk/tools(|/.*)$',
              '^svn\\:\\/\\/chrome\\-svn\\.corp\\.google\\.com\\/chrome/trunk/'
                  'tools(|/.*)$',
              '^http\\:\\/\\/src\\.chromium\\.org\\/svn/trunk/tools(|/.*)$',
              '^https\\:\\/\\/src\\.chromium\\.org\\/svn/trunk/tools(|/.*)$',
              '^http\\:\\/\\/src\\.chromium\\.org\\/chrome/trunk/tools(|/.*)$',
              '^https\\:\\/\\/src\\.chromium\\.org\\/chrome/trunk/tools(|/.*)$',
              '^https\\:\\/\\/git\\.chromium\\.org\\/chromium\\/tools\\/'
                  '([a-z\\-_]+)\\.git\\@[a-z\\-_]+$',
              '^http\\:\\/\\/git\\.chromium\\.org\\/chromium\\/tools\\/'
                  '([a-z\\-_]+)\\.git\\@[a-z\\-_]+$',
              '^https\\:\\/\\/git\\.chromium\\.org\\/git\\/chromium\\/tools\\/'
                  '([a-z\\-_]+)\\@[a-z\\-_]+$',
              '^http\\:\\/\\/git\\.chromium\\.org\\/git\\/chromium\\/tools\\/'
                  '([a-z\\-_]+)\\@[a-z\\-_]+$',
              '^https\\:\\/\\/git\\.chromium\\.org\\/git\\/chromium\\/tools\\/'
                  '([a-z\\-_]+)\\.git\\@[a-z\\-_]+$',
              '^http\\:\\/\\/git\\.chromium\\.org\\/git\\/chromium\\/tools\\/'
                  '([a-z\\-_]+)\\.git\\@[a-z\\-_]+$',
            ],
            project_bases_verifier.project_bases)
    self.assertEquals({}, mapping)

class ProjectChromiumTest(TestCase):
  def setUp(self):
    super(ProjectChromiumTest, self).setUp()

  def test_tbr(self):
    self.read_lines = [
        ['root_dir/.chromium_status_pwd', 'chromium-status password', ['foo']],
    ]
    pc = projects.load_project(
        'chromium', 'commit-bot-test', 'root_dir', self.context.rietveld, True)
    pc.context = self.context
    issue = self.context.rietveld.issues[31337]
    # A TBR= patch without reviewer nor messages, like a webkit roll.
    issue['description'] += '\nTBR='
    issue['reviewers'] = []
    issue['messages'] = []
    issue['owner_email'] = 'user@example.com'
    issue['base_url'] = 'svn://svn.chromium.org/chrome/trunk/src'
    pc.look_for_new_pending_commit()
    self._check_output = [0]
    pc.process_new_pending_commit()
    self.assertEquals([], self._check_output)
    pc.update_status()
    pc.scan_results()
    self.assertEquals(0, len(pc.queue.pending_commits))
    self.context.rietveld.check_calls([
      _try_comment(pc),
      'close_issue(31337)',
      "update_description(31337, u'foo\\nTBR=')",
      "add_comment(31337, 'Change committed as 125')",
      ])
    self.context.checkout.check_calls([
      'prepare(None)',
      'apply_patch(%r)' % self.context.rietveld.patchsets[0],
      'prepare(None)',
      'apply_patch(%r)' % self.context.rietveld.patchsets[1],
      "commit(u'foo\\nTBR=\\n\\nReview URL: http://nowhere/31337', "
          "'user@example.com')",
      ])
    self.context.status.check_names(['initial', 'commit'])


if __name__ == '__main__':
  logging.basicConfig(
      level=[logging.WARNING, logging.INFO, logging.DEBUG][
        min(sys.argv.count('-v'), 2)],
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')
  unittest.main()
