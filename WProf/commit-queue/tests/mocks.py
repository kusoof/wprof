# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common mocks."""

import copy
import os
import sys

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT_DIR, '..'))

import find_depot_tools  # pylint: disable=W0611
import breakpad
import patch

import async_push
import context
import pending_manager

from testing_support import auto_stub


SVN_PATCH = (
    'Index: chrome/file.cc\n'
    '===================================================================\n'
    '--- chrome/file.cc\t(revision 74690)\n'
    '+++ chrome/file.cc\t(working copy)\n'
    '@@ -80,10 +80,10 @@\n'
    ' // Foo\n'
    ' // Bar\n'
    ' void foo() {\n'
    '-   return bar;\n'
    '+   return foo;\n'
    ' }\n'
    ' \n'
    ' \n')


class RietveldMock(auto_stub.SimpleMock):
  url = 'http://nowhere'
  email = 'fake_email'
  password = 'fake_password'

  def __init__(self, unit_test):
    super(RietveldMock, self).__init__(unit_test)
    self.issues = {
      31337: {
        "description": u"foo",
        "created": "2010-12-27 03:23:31.149045",
        "cc": ["cc@example.com",],
        "reviewers": ["rev@example.com"],
        "owner_email": "author@example.com",
        "patchsets": [1],
        "modified": "2011-01-10 20:52:39.127231",
        "private": False,
        "base_url": "svn://fake/repo",
        "closed": False,
        "owner": "Author",
        "issue": 31337,
        "subject": 'foo',
        "messages": [
          {
            "date": "2010-12-27 03:23:32.489999",
            "text": u"hi!",
            "sender": "author@example.com",
            "recipients": ["rev@example.com", "cc@example.com"],
            "approval": False,
          },
        ],
        "commit": True,
      },
    }
    self.patchsets = []

  def get_pending_issues(self):
    return self.issues.keys()

  def get_issue_properties(self, issue_id, _):
    return copy.deepcopy(self.issues[issue_id])

  def close_issue(self, *args, **kwargs):
    self._register_call(*args, **kwargs)

  def update_description(self, *args, **kwargs):
    self._register_call(*args, **kwargs)

  def set_flag(self, *args, **kwargs):
    self._register_call(*args, **kwargs)
    return True

  def add_comment(self, *args, **kwargs):
    self._register_call(*args, **kwargs)

  def get_patch(self, _issue, _patchset):
    self.patchsets.append(patch.PatchSet([
        patch.FilePatchDiff('chrome/file.cc', SVN_PATCH, []),
        patch.FilePatchDelete('other/place/foo', True),
        patch.FilePatchBinary('foo', 'data', [], True),
    ]))
    return self.patchsets[-1]

  def _send(self, *args, **kwargs):
    self._register_call(*args, **kwargs)


class SvnCheckoutMock(auto_stub.SimpleMock):
  def __init__(self, *args):
    super(SvnCheckoutMock, self).__init__(*args)
    self.project_path = os.getcwd()
    self.project_name = os.path.basename(self.project_path)
    self.post_processors = []

  def prepare(self, revision):
    self._register_call(revision)
    # prepare() should always return a valid revision.
    return revision or 124

  def apply_patch(self, *args):
    self._register_call(*args)

  def commit(self, *args):
    self._register_call(*args)
    return 125

  @staticmethod
  def get_settings(_key):
    return None


class AsyncPushMock(auto_stub.SimpleMock, async_push.AsyncPushNoop):
  def __init__(self, *args):
    auto_stub.SimpleMock.__init__(self, *args)
    async_push.AsyncPushNoop.__init__(self)
    self.queue = []

  def send(self, packet, pending):
    self.queue.append(self._package(packet, pending))

  def pop_packets(self):
    packets = self.queue
    self.queue = []
    return packets

  def check_packets(self, expected):
    self.assertEquals(expected, self.pop_packets())

  def check_names(self, expected):
    self.assertEquals(expected, [i['verification'] for i in self.pop_packets()])


class TestCase(auto_stub.TestCase):
  def setUp(self):
    super(TestCase, self).setUp()
    self.mock(breakpad, 'SendStack', self._send_stack_mock)
    self.context = context.Context(
        RietveldMock(self), SvnCheckoutMock(self), AsyncPushMock(self))
    self.pending = pending_manager.PendingCommit(
        42, 'owner@example.com', [], 23, '', 'bleh', [])

  def tearDown(self):
    super(TestCase, self).tearDown()
    if not self.has_failed():
      self.context.rietveld.check_calls([])
      self.context.checkout.check_calls([])
      self.context.status.check_packets([])

  def _send_stack_mock(self, last_tb, stack, *_args, **_kwargs):
    """Fails a test that calls SendStack.

    In practice it doesn't happen when a test pass but will when a test fails so
    hook it here so breakpad doesn't send too many stack traces to maintainers.
    """
    self.fail('%s, %s' % (last_tb, stack))
