#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for classes in gtest_command.py."""

import unittest

import test_env  # pylint: disable=W0611

from common import gtest_utils


FAILURES = ['NavigationControllerTest.Reload',
            'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0',
            'BadTest.TimesOut',
            'MoreBadTest.TimesOutAndFails',
            'SomeOtherTest.SwitchTypes',
            'SomeOtherTest.FAILS_ThisTestTimesOut']

FAILS_FAILURES = ['SomeOtherTest.FAILS_Bar']
FLAKY_FAILURES = ['SomeOtherTest.FLAKY_Baz']

TIMEOUT_MESSAGE = 'Killed (timed out).'

RELOAD_ERRORS = ('C:\b\slave\chrome-release-snappy\build\chrome\browser'
'\navigation_controller_unittest.cc:381: Failure' + """
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

""")

SPDY_ERRORS = ('C:\b\slave\chrome-release-snappy\build\chrome\browser'
'\navigation_controller_unittest.cc:439: Failure' + """
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

""")

SWITCH_ERRORS = ('C:\b\slave\chrome-release-snappy\build\chrome\browser'
'\navigation_controller_unittest.cc:615: Failure' + """
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

""" + 'C:\b\slave\chrome-release-snappy\build\chrome\browser'
'\navigation_controller_unittest.cc:617: Failure' + """
Value of: contents->controller()->GetPendingEntry()
  Actual: true
Expected: false

""")

TIMEOUT_ERRORS = ('[61613:263:0531/042613:2887943745568888:ERROR:/b/slave'
'/chromium-rel-mac-builder/build/src/chrome/browser/extensions'
'/extension_error_reporter.cc(56)] Extension error: Could not load extension '
'from \'extensions/api_test/geolocation/no_permission\'. Manifest file is '
'missing or unreadable.')

MOREBAD_ERRORS = """
Value of: entry->page_type()
  Actual: 2
Expected: NavigationEntry::NORMAL_PAGE
"""

TEST_DATA = ("""
[==========] Running 7 tests from 3 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.All
[       OK ] HunspellTest.All (62 ms)
[----------] 1 test from HunspellTest (62 ms total)

[----------] 4 tests from NavigationControllerTest
[ RUN      ] NavigationControllerTest.Defaults
[       OK ] NavigationControllerTest.Defaults (48 ms)
[ RUN      ] NavigationControllerTest.Reload
%(reload_errors)s
[  FAILED  ] NavigationControllerTest.Reload (2 ms)
[ RUN      ] NavigationControllerTest.Reload_GeneratesNewPage
[       OK ] NavigationControllerTest.Reload_GeneratesNewPage (22 ms)
[ RUN      ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0
%(spdy_errors)s
[  FAILED  ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0 (2 ms)
[----------] 4 tests from NavigationControllerTest (74 ms total)

  YOU HAVE 2 FLAKY TESTS

[----------] 1 test from BadTest
[ RUN      ] BadTest.TimesOut
%(timeout_errors)s
""" % {'reload_errors': RELOAD_ERRORS,
       'spdy_errors': SPDY_ERRORS,
       'timeout_errors': TIMEOUT_ERRORS} +
'[0531/042642:ERROR:/b/slave/chromium-rel-mac-builder/build/src/chrome'
'/test/test_launcher/out_of_proc_test_runner.cc(79)] Test timeout (30000 ms) '
'exceeded for BadTest.TimesOut' + """
Handling SIGTERM.
Successfully wrote to shutdown pipe, resetting signal handler.
""" +
'[61613:19971:0531/042642:2887973024284693:INFO:/b/slave/chromium-rel-mac-'
'builder/build/src/chrome/browser/browser_main.cc(285)] Handling shutdown for '
'signal 15.' + """

[----------] 1 test from MoreBadTest
[ RUN      ] MoreBadTest.TimesOutAndFails
%(morebad_errors)s
""" % {'morebad_errors': MOREBAD_ERRORS} +
'[0531/042642:ERROR:/b/slave/chromium-rel-mac-builder/build/src/chrome/test'
'/test_launcher/out_of_proc_test_runner.cc(79)] Test timeout (30000 ms) '
'exceeded for MoreBadTest.TimesOutAndFails' + """
Handling SIGTERM.
Successfully wrote to shutdown pipe, resetting signal handler.
[  FAILED  ] MoreBadTest.TimesOutAndFails (31000 ms)

[----------] 5 tests from SomeOtherTest
[ RUN      ] SomeOtherTest.SwitchTypes
%(switch_errors)s
[  FAILED  ] SomeOtherTest.SwitchTypes (40 ms)
[ RUN      ] SomeOtherTest.Foo
[       OK ] SomeOtherTest.Foo (20 ms)
[ RUN      ] SomeOtherTest.FAILS_Bar
Some error message for a failing test.
[  FAILED  ] SomeOtherTest.FAILS_Bar (40 ms)
[ RUN      ] SomeOtherTest.FAILS_ThisTestTimesOut
""" %  {'switch_errors' : SWITCH_ERRORS} +
'[0521/041343:ERROR:test_launcher.cc(384)] Test timeout (5000 ms) '
'exceeded for SomeOtherTest.FAILS_ThisTestTimesOut' + """
[ RUN      ] SomeOtherTest.FLAKY_Baz
Some error message for a flaky test.
[  FAILED  ] SomeOtherTest.FLAKY_Baz (40 ms)
[----------] 2 tests from SomeOtherTest (60 ms total)

[----------] Global test environment tear-down
[==========] 8 tests from 3 test cases ran. (3750 ms total)
[  PASSED  ] 4 tests.
[  FAILED  ] 4 tests, listed below:
[  FAILED  ] NavigationControllerTest.Reload
[  FAILED  ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0
[  FAILED  ] SomeOtherTest.SwitchTypes
[  FAILED  ] SomeOtherTest.FAILS_ThisTestTimesOut

 1 FAILED TEST
  YOU HAVE 10 DISABLED TESTS

  YOU HAVE 2 FLAKY TESTS

program finished with exit code 1
""")

TEST_DATA_CRASH = """
[==========] Running 7 tests from 3 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.Crashes
Oops, this test crashed!
"""

VALGRIND_HASH = 'B254345E4D3B6A00'

VALGRIND_SUPPRESSION = """Suppression (error hash=#%(hash)s#):
{
   <insert_a_suppression_name_here>
   Memcheck:Leak
   fun:_Znw*
   fun:_ZN31NavigationControllerTest_Reload8TestBodyEv
}""" % {'hash' : VALGRIND_HASH}

TEST_DATA_VALGRIND = """
[==========] Running 5 tests from 2 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.All
[       OK ] HunspellTest.All (62 ms)
[----------] 1 test from HunspellTest (62 ms total)

[----------] 4 tests from NavigationControllerTest
[ RUN      ] NavigationControllerTest.Defaults
[       OK ] NavigationControllerTest.Defaults (48 ms)
[ RUN      ] NavigationControllerTest.Reload
[       OK ] NavigationControllerTest.Reload (2 ms)
[ RUN      ] NavigationControllerTest.Reload_GeneratesNewPage
[       OK ] NavigationControllerTest.Reload_GeneratesNewPage (22 ms)
[ RUN      ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0
[       OK ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0 (2 ms)
[----------] 4 tests from NavigationControllerTest (74 ms total)

[----------] Global test environment tear-down
[==========] 5 tests from 1 test cases ran. (136 ms total)
[  PASSED  ] 5 tests.

%(suppression)s
program finished with exit code 255
""" % {'suppression': VALGRIND_SUPPRESSION}


FAILING_TESTS_OUTPUT = """
Failing tests:
ChromeRenderViewTest.FAILS_AllowDOMStorage
PrerenderBrowserTest.PrerenderHTML5VideoJs
"""

FAILING_TESTS_EXPECTED = ['ChromeRenderViewTest.FAILS_AllowDOMStorage',
                          'PrerenderBrowserTest.PrerenderHTML5VideoJs']


TEST_DATA_SHARD_0 = ("""Note: This is test shard 1 of 30.
[==========] Running 6 tests from 3 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.All
[       OK ] HunspellTest.All (62 ms)
[----------] 1 test from HunspellTest (62 ms total)

[----------] 1 test from BadTest
[ RUN      ] BadTest.TimesOut
%(timeout_errors)s
""" % {'timeout_errors': TIMEOUT_ERRORS} +
'[0531/042642:ERROR:/b/slave/chromium-rel-mac-builder/build/src/chrome/test'
'/test_launcher/out_of_proc_test_runner.cc(79)] Test timeout (30000 ms) '
'exceeded for BadTest.TimesOut' + """
Handling SIGTERM.
Successfully wrote to shutdown pipe, resetting signal handler.
""" +
'[61613:19971:0531/042642:2887973024284693:INFO:/b/slave/chromium-rel-mac-'
'builder/build/src/chrome/browser/browser_main.cc(285)] Handling shutdown for '
'signal 15.' + """

[----------] 4 tests from SomeOtherTest
[ RUN      ] SomeOtherTest.SwitchTypes
%(switch_errors)s
[  FAILED  ] SomeOtherTest.SwitchTypes (40 ms)
[ RUN      ] SomeOtherTest.Foo
[       OK ] SomeOtherTest.Foo (20 ms)
[ RUN      ] SomeOtherTest.FAILS_Bar
Some error message for a failing test.
[  FAILED  ] SomeOtherTest.FAILS_Bar (40 ms)
[ RUN      ] SomeOtherTest.FAILS_ThisTestTimesOut
"""  % {'switch_errors' : SWITCH_ERRORS} +
'[0521/041343:ERROR:test_launcher.cc(384)] Test timeout (5000 ms) exceeded '
'for SomeOtherTest.FAILS_ThisTestTimesOut' + """
[ RUN      ] SomeOtherTest.FLAKY_Baz
Some error message for a flaky test.
[  FAILED  ] SomeOtherTest.FLAKY_Baz (40 ms)
[----------] 2 tests from SomeOtherTest (60 ms total)

[----------] Global test environment tear-down
[==========] 7 tests from 3 test cases ran. (3750 ms total)
[  PASSED  ] 5 tests.
[  FAILED  ] 2 test, listed below:
[  FAILED  ] SomeOtherTest.SwitchTypes
[  FAILED  ] SomeOtherTest.FAILS_ThisTestTimesOut

 1 FAILED TEST
  YOU HAVE 10 DISABLED TESTS

  YOU HAVE 2 FLAKY TESTS
""")

TEST_DATA_SHARD_1 = ("""Note: This is test shard 13 of 30.
[==========] Running 5 tests from 2 test cases.
[----------] Global test environment set-up.
[----------] 4 tests from NavigationControllerTest
[ RUN      ] NavigationControllerTest.Defaults
[       OK ] NavigationControllerTest.Defaults (48 ms)
[ RUN      ] NavigationControllerTest.Reload
%(reload_errors)s
[  FAILED  ] NavigationControllerTest.Reload (2 ms)
[ RUN      ] NavigationControllerTest.Reload_GeneratesNewPage
[       OK ] NavigationControllerTest.Reload_GeneratesNewPage (22 ms)
[ RUN      ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0
%(spdy_errors)s
""" % {'reload_errors' : RELOAD_ERRORS,
       'spdy_errors'   : SPDY_ERRORS} +
'[  FAILED  ] NavigationControllerTest/SpdyNetworkTransTest.Constructor'
'/0 (2 ms)' + """
[----------] 4 tests from NavigationControllerTest (74 ms total)

  YOU HAVE 2 FLAKY TESTS

[----------] 1 test from MoreBadTest
[ RUN      ] MoreBadTest.TimesOutAndFails
%(morebad_errors)s
""" % {'morebad_errors': MOREBAD_ERRORS} +
'[0531/042642:ERROR:/b/slave/chromium-rel-mac-builder/build/src/chrome/test'
'/test_launcher/out_of_proc_test_runner.cc(79)] Test timeout (30000 ms) '
'exceeded for MoreBadTest.TimesOutAndFails' + """
Handling SIGTERM.
Successfully wrote to shutdown pipe, resetting signal handler.
[  FAILED  ] MoreBadTest.TimesOutAndFails (31000 ms)

[----------] Global test environment tear-down
[==========] 5 tests from 2 test cases ran. (3750 ms total)
[  PASSED  ] 3 tests.
[  FAILED  ] 2 tests, listed below:
[  FAILED  ] NavigationControllerTest.Reload
[  FAILED  ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0

 1 FAILED TEST
  YOU HAVE 10 DISABLED TESTS

  YOU HAVE 2 FLAKY TESTS
""")

TEST_DATA_SHARD_EXIT = 'program finished with exit code '

TEST_DATA_CRASH_SHARD = """Note: This is test shard 5 of 5.
[==========] Running 7 tests from 3 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.Crashes
Oops, this test crashed!"""


class TestGTestLogParserTests(unittest.TestCase):

  def testGTestLogParserNoSharing(self):
    # Tests for log parsing without sharding.
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA.splitlines():
      parser.ProcessLine(line)

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertFalse(parser.RunningTests())

    self.assertEqual(sorted(FAILURES), sorted(parser.FailedTests()))
    self.assertEqual(sorted(FAILURES + FAILS_FAILURES),
                     sorted(parser.FailedTests(include_fails=True)))
    self.assertEqual(sorted(FAILURES + FLAKY_FAILURES),
                     sorted(parser.FailedTests(include_flaky=True)))
    self.assertEqual(sorted(FAILURES + FAILS_FAILURES + FLAKY_FAILURES),
        sorted(parser.FailedTests(include_fails=True, include_flaky=True)))

    self.assertEqual(10, parser.DisabledTests())
    self.assertEqual(2, parser.FlakyTests())

    test_name = 'NavigationControllerTest.Reload'
    self.assertEqual('\n'.join(['%s: ' % test_name, RELOAD_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))

    test_name = 'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0'
    self.assertEqual('\n'.join(['%s: ' % test_name, SPDY_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))

    test_name = 'SomeOtherTest.SwitchTypes'
    self.assertEqual('\n'.join(['%s: ' % test_name, SWITCH_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))

    test_name = 'BadTest.TimesOut'
    self.assertEqual('\n'.join(['%s: ' % test_name,
                                TIMEOUT_ERRORS, TIMEOUT_MESSAGE]),
                     '\n'.join(parser.FailureDescription(test_name)))

    test_name = 'MoreBadTest.TimesOutAndFails'
    self.assertEqual('\n'.join(['%s: ' % test_name,
                                MOREBAD_ERRORS, TIMEOUT_MESSAGE]),
                     '\n'.join(parser.FailureDescription(test_name)))

    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_CRASH.splitlines():
      parser.ProcessLine(line)

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertTrue(parser.RunningTests())
    self.assertEqual(['HunspellTest.Crashes'], parser.FailedTests())
    self.assertEqual(0, parser.DisabledTests())
    self.assertEqual(0, parser.FlakyTests())

    test_name = 'HunspellTest.Crashes'
    self.assertEqual('\n'.join(['%s: ' % test_name, 'Did not complete.']),
                     '\n'.join(parser.FailureDescription(test_name)))

  def testGTestLogParserSharing(self):
    # Same tests for log parsing with sharding_supervisor.
    parser = gtest_utils.GTestLogParser()
    test_data_shard = TEST_DATA_SHARD_0 + TEST_DATA_SHARD_1
    for line in test_data_shard.splitlines():
      parser.ProcessLine(line)
    parser.ProcessLine(TEST_DATA_SHARD_EXIT + '2')

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertFalse(parser.RunningTests())

    self.assertEqual(sorted(FAILURES), sorted(parser.FailedTests()))
    self.assertEqual(sorted(FAILURES + FAILS_FAILURES),
                     sorted(parser.FailedTests(include_fails=True)))
    self.assertEqual(sorted(FAILURES + FLAKY_FAILURES),
                     sorted(parser.FailedTests(include_flaky=True)))
    self.assertEqual(sorted(
        FAILURES + FAILS_FAILURES + FLAKY_FAILURES),
        sorted(parser.FailedTests(include_fails=True, include_flaky=True)))

    self.assertEqual(10, parser.DisabledTests())
    self.assertEqual(2, parser.FlakyTests())

    test_name = 'NavigationControllerTest.Reload'
    self.assertEqual('\n'.join(['%s: ' % test_name, RELOAD_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))

    test_name = (
        'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0')
    self.assertEqual('\n'.join(['%s: ' % test_name, SPDY_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))

    test_name = 'SomeOtherTest.SwitchTypes'
    self.assertEqual('\n'.join(['%s: ' % test_name, SWITCH_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))

    test_name = 'BadTest.TimesOut'
    self.assertEqual(
        '\n'.join(['%s: ' % test_name,
        TIMEOUT_ERRORS, TIMEOUT_MESSAGE]),
        '\n'.join(parser.FailureDescription(test_name)))

    test_name = 'MoreBadTest.TimesOutAndFails'
    self.assertEqual(
        '\n'.join(['%s: ' % test_name,
        MOREBAD_ERRORS, TIMEOUT_MESSAGE]),
        '\n'.join(parser.FailureDescription(test_name)))

    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_CRASH.splitlines():
      parser.ProcessLine(line)

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertTrue(parser.RunningTests())
    self.assertEqual(['HunspellTest.Crashes'], parser.FailedTests())
    self.assertEqual(0, parser.DisabledTests())
    self.assertEqual(0, parser.FlakyTests())

    test_name = 'HunspellTest.Crashes'
    self.assertEqual('\n'.join(['%s: ' % test_name, 'Did not complete.']),
                     '\n'.join(parser.FailureDescription(test_name)))

  def testGTestLogParserValgrind(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_VALGRIND.splitlines():
      parser.ProcessLine(line)

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertFalse(parser.RunningTests())
    self.assertFalse(parser.FailedTests())
    self.assertEqual([VALGRIND_HASH], parser.SuppressionHashes())
    self.assertEqual(VALGRIND_SUPPRESSION,
                     '\n'.join(parser.Suppression(VALGRIND_HASH)))

    parser = gtest_utils.GTestLogParser()
    for line in FAILING_TESTS_OUTPUT.splitlines():
      parser.ProcessLine(line)
    self.assertEqual(FAILING_TESTS_EXPECTED,
                     parser.FailedTests(True, True))


if __name__ == '__main__':
  unittest.main()
