# -*- python -*-
# ex: set syntax=python:

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Chrome Buildbot slave configuration

import os
import socket
import sys

from twisted.application import service
# python module paths changed from buildbot-7 to buildbot-8; support both
try:
  from buildbot.slave.bot import BuildSlave
except ImportError:
  from buildslave.bot import BuildSlave

# Register the commands.
from slave import chromium_commands
from slave import slave_utils
# Load default settings.
import config

slave_utils.ImportMasterConfigs()

# Determines the slave type:
ActiveMaster = None


# Slave properties:
slavename = None
password = config.Master.GetBotPassword()
host = None
port = None
basedir = None
keepalive = 600
usepty = 1
umask = None


if slavename is None:
    # Automatically determine the host name.
    slavename = os.environ.get(
        'TESTING_SLAVENAME', socket.getfqdn().split('.')[0].lower())
print 'Using slave name %s' % slavename

if password is None:
    print >> sys.stderr, '*** No password configured in %s.' % repr(__file__)
    sys.exit(1)

if ActiveMaster is None:
    master_name = os.environ.get(
        'TESTING_MASTER', slave_utils.GetActiveMaster())
    if not master_name:
        print >> sys.stderr, '*** Failed to detect the active master'
        sys.exit(1)
    print 'Using master %s' % master_name
    if hasattr(config.Master, 'active_master'):
      ActiveMaster = config.Master.active_master
    elif master_name and getattr(config.Master, master_name):
      ActiveMaster = getattr(config.Master, master_name)
    else:
      print >> sys.stderr, '*** Failed to detect the active master'
      sys.exit(1)

if host is None:
    host = os.environ.get('TESTING_MASTER_HOST', ActiveMaster.master_host)
print 'Using master host %s' % host

if port is None:
    port = ActiveMaster.slave_port
print 'Using master port %s' % port

if basedir is None:
    basedir = os.path.dirname(os.path.abspath(__file__))


application = service.Application('buildslave')
s = BuildSlave(host, port, slavename, password, basedir, keepalive, usepty,
               umask=umask)
s.setServiceParent(application)
