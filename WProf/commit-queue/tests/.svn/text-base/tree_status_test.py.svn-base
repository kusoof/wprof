#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for verification/tree_status.py."""

import logging
import os
import StringIO
import sys
import time
import unittest
import urllib2

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT_DIR, '..'))

# From tests/
import mocks

from verification import tree_status


class TreeStatusTest(mocks.TestCase):
  def setUp(self):
    super(TreeStatusTest, self).setUp()
    reference = time.mktime((2010, 1, 1, 12, 0, 0, 0, 0, -1)) - time.altzone
    self.mock(tree_status.time, 'time', lambda: reference)
    self._data = ''
    self.mock(urllib2, 'urlopen', lambda x: StringIO.StringIO(self._data))

  def test_fail(self):
    self._data = (
        '[{"date": "2010-01-01 11:56:00.0", "general_state": "open"},'
        ' {"date": "2010-01-01 11:57:00.0", "general_state": "closed"},'
        ' {"date": "2010-01-01 11:58:00.0", "general_state": "open"}]')
    self.assertEquals(True, tree_status.TreeStatus('foo').postpone())

  def test_pass(self):
    self._data = (
        '[{"date": "2010-01-01 11:54:00.0", "general_state": "open"},'
        ' {"date": "2010-01-01 11:57:00.0", "general_state": "open"},'
        ' {"date": "2010-01-01 11:53:00.0", "general_state": "closed"}]')
    self.assertEquals(False, tree_status.TreeStatus('foo').postpone())

  def test_state(self):
    t = tree_status.TreeStatus('foo')
    self.assertEquals(tree_status.base.SUCCEEDED, t.get_state())


if __name__ == '__main__':
  logging.basicConfig(
      level=[logging.ERROR, logging.WARNING, logging.INFO, logging.DEBUG][
        min(sys.argv.count('-v'), 3)],
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')
  unittest.main()
