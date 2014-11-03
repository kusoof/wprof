#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to kill any leftover test processes, executed by buildbot.

Only works on Windows."""

import optparse
import os
import re
import subprocess
import sys
import time


def KillAll(process_names, must_die=True):
  """Tries to kill all copies of each process in the processes list."""
  killed_processes = []

  for process_name in process_names:
    if ProcessExists(process_name):
      Kill(process_name)
      killed_processes.append(process_name)

  # If we allow any processes to continue after trying to kill them, return
  # now.
  if not must_die:
    return True

  # Give our processes time to exit.
  for _ in range(60):
    if not AnyProcessExists(killed_processes):
      break
    time.sleep(1)

  # We require that all processes we tried to kill must be killed.  Let's
  # verify that.
  return not AnyProcessExists(killed_processes)


def ProcessExists(process_name):
  """Return whether process_name is found in tasklist output."""
  # Use tasklist.exe to find if a given process_name is running.
  command = ('tasklist.exe /fi "imagename eq %s" | findstr.exe "K"' %
             process_name)
  # findstr.exe exits with code 0 if the given string is found.
  return os.system(command) == 0


def ProcessExistsByPid(pid):
  """Return whether pid is found in tasklist output."""
  # Use tasklist.exe to find if a given process_name is running.
  command = ('tasklist.exe /fi "pid eq %d" | findstr.exe "K"' %
             pid)
  # findstr.exe exits with code 0 if the given string is found.
  return os.system(command) == 0


def AnyProcessExists(process_list):
  """Return whether any process from the list is still running."""
  return any(ProcessExists(process) for process in process_list)


def AnyProcessExistsByPid(pid_list):
  """Return whether any process from the list is still running."""
  return any(ProcessExistsByPid(pid) for pid in pid_list)


def Kill(process_name):
  command = ['taskkill.exe', '/f', '/t', '/im']
  subprocess.call(command + [process_name])


def KillByPid(pid):
  command = ['taskkill.exe', '/f', '/t', '/pid']
  subprocess.call(command + [str(pid)])


def KillProcessesUsingCurrentDirectory(handle_exe):
  if not os.path.exists(handle_exe):
    return False
  try:
    handle = subprocess.Popen([handle_exe,
                               os.path.join(os.getcwd(), 'src'),
                               '/accepteula'],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
  except WindowsError, e:  # pylint: disable=E0602
    print e
    return False
  stdout, stderr = handle.communicate()

  # Do a basic sanity check to make sure the tool is working fine.
  if stderr or ('.exe' not in stdout and
                'Non-existant Process' not in stdout and
                'No matching handles found' not in stdout):
    print 'Error running handle.exe: ' + repr((stdout, stderr))
    return False

  pid_list = []
  for line in stdout.splitlines():  # pylint: disable=E1103
    # Killing explorer.exe would hose the bot, don't do that.
    if 'explorer.exe' in line:
      continue

    if '.exe' in line:
      match = re.match('.*pid: (\d+).*', line)
      if match:
        pid = int(match.group(1))

        # Do not kill self.
        if int(pid) == int(os.getpid()):
          continue

        print 'Killing: ' + line
        pid_list.append(pid)
        KillByPid(pid)

  # Give our processes time to exit.
  for _ in range(60):
    if not AnyProcessExistsByPid(pid_list):
      break
    time.sleep(1)

  return True


# rdpclip.exe is part of Remote Desktop.  It has a bug that sometimes causes
# it to keep the clipboard open forever, denying other processes access to it.
# Killing BuildConsole.exe usually stops an IB build within a few seconds.
# Unfortunately, killing devenv.com or devenv.exe doesn't stop a VS build, so
# we don't bother pretending.
processes = [
    # Utilities we don't build, but which we use or otherwise can't
    # have hanging around.
    'BuildConsole.exe',
    'httpd.exe',
    'outlook.exe',
    'perl.exe',
    'python_slave.exe',
    'rdpclip.exe',
    'svn.exe',

    # These processes are spawned by some tests and should be killed by same.
    # It may occur that they are left dangling if a test crashes, so we kill
    # them here too.
    'firefox.exe',
    'iexplore.exe',
    #'ieuser.exe',
    'acrord32.exe',

    # The JIT debugger may start when devenv.exe crashes.
    'vsjitdebugger.exe',
    # This process is also crashing once in a while during compile.
    'midlc.exe',

    # Things built by/for Chromium.
    'base_unittests.exe',
    'cacheinvalidation_unittests.exe',
    'chrome.exe',
    'chromedriver.exe',
    'chrome_frame_helper.exe',
    'chrome_frame_net_tests.exe',
    'chrome_frame_tests.exe',
    'chrome_launcher.exe',
    'chrome_frame_unittests.exe',
    'content_unittests.exe',
    'crash_service.exe',
    'crypto_unittests.exe',
    'debug_message.exe',
    'DumpRenderTree.exe',
    'flush_cache.exe',
    'gl_tests.exe',
    'gpu_tests.exe',
    'ie_unittests.exe',
    'image_diff.exe',
    'installer_util_unittests.exe',
    'interactive_ui_tests.exe',
    'ipc_tests.exe',
    'jingle_unittests.exe',
    'mediumtest_ie.exe',
    'memory_test.exe',
    'nacl64.exe',
    'net_unittests.exe',
    'page_cycler_tests.exe',
    'perf_tests.exe',
    'performance_ui_tests.exe',
    'printing_unittests.exe',
    'reliability_tests.exe',
    'selenium_tests.exe',
    'startup_tests.exe',
    'sync_integration_tests.exe',
    'tab_switching_test.exe',
    'test_shell.exe',
    'test_shell_tests.exe',
    'tld_cleanup.exe',
    'unit_tests.exe',
    'v8_shell.exe',
    'v8_mksnapshot.exe',
    'v8_shell_sample.exe',
    'wow_helper.exe',
]

# Some processes may be present occasionally unrelated to the current build.
# For these, it's not an error if we attempt to kill them and they don't go
# away.
lingering_processes = [
    # When VC crashes during compilation, this process which manages the .pdb
    # file generation sometime hangs.  However, Incredibuild will spawn
    # mspdbsrv.exe, so don't trigger an error if it is still present after
    # we attempt to kill it.
    'mspdbsrv.exe',
]

def main():
  handle_exe_default = os.path.join(os.getcwd(), '..', '..', '..',
                                    'third_party', 'psutils', 'handle.exe')

  parser = optparse.OptionParser(
    usage='%prog [options]')
  parser.add_option('--handle_exe', default=handle_exe_default,
                    help='The path to handle.exe. Defaults to %default.')
  (options, args) = parser.parse_args()

  if args:
    parser.error('Unknown arguments passed in, %s' % args)

  # Kill all lingering processes.  It's okay if these aren't killed or end up
  # reappearing.
  KillAll(lingering_processes, must_die=False)

  rc = 0
  if not KillProcessesUsingCurrentDirectory(options.handle_exe):
    rc = 88

  # Kill all regular processes.  We must guarantee that these are killed since
  # we exit with an error code if they're not.
  if KillAll(processes, must_die=True):
    return rc

  # Some processes were not killed, exit with non-zero status.
  return 1


if '__main__' == __name__:
  sys.exit(main())
