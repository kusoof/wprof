# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A StatusReceiver module to potentially close the tree when needed.

The GateKeeper class can be given a dictionary of categories of builder to a set
of critical steps to validate. If no steps are given for a category, we simply
check the results for FAILURES. If no categories are given, we check all
builders for FAILURES results. A dictionary of builder steps exclusions can also
be given so that some specific steps of specific builders could be ignored.
The rest of the arguments are the same as the MailNotifier, refer to its
documentation for more details.

Since the behavior is very similar to the MainNotifier, we simply inherit from
it and also reuse some of its methods to send emails.
"""

import urllib

from buildbot.status.builder import FAILURE
from twisted.python import log
from twisted.web import client

from master import build_utils
from master import chromium_notifier
from master import get_password


class GateKeeper(chromium_notifier.ChromiumNotifier):
  """This is a status notifier which closes the tree upon failures.

  See builder.interfaces.IStatusReceiver to have more information about the
  parameters type."""


  def __init__(self, tree_status_url, tree_message=None,
               check_revisions=True, **kwargs):
    """Constructor with following specific arguments (on top of base class').

    @type tree_status_url: String.
    @param tree_status_url: Web end-point for tree status updates.

    @type tree_message: String.
    @param tree_message: Message posted to the tree status site when closed.

    @type check_revisions: Boolean, default to True.
    @param check_revisions: Check revisions and users for closing the tree.

    @type password: String.
    @param password: Password for service.  If None, look in .status_password.
    """
    # Set defaults.
    kwargs.setdefault('sheriffs', ['sheriff'])
    kwargs.setdefault('sendToInterestedUsers', True)
    kwargs.setdefault(
        'status_header',
        'Automatically closing tree for "%(steps)s" on "%(builder)s"')
    chromium_notifier.ChromiumNotifier.__init__(self, **kwargs)

    self.tree_status_url = tree_status_url
    self.check_revisions = check_revisions
    self.tree_message = (
        tree_message or
        'Tree is closed (Automatic: "%(steps)s" on "%(builder)s"%(blame)s)')
    self._last_closure_revision = 0

    self.password = get_password.Password('.status_password').GetPassword()


  def isInterestingStep(self, build_status, step_status, results):
    """Look at most cases that could make us ignore the step results.

    Do not look at current tree status here since it's too slow."""
    # If we have not failed, or are not interested in this builder,
    # then we have nothing to do.
    if results[0] != FAILURE:
      return False

    # Check if the slave is still alive. We should not close the tree for
    # inactive slaves.
    slave_name = build_status.getSlavename()
    if slave_name in self.master_status.getSlaveNames():
      # @type self.master_status: L{buildbot.status.builder.Status}
      # @type self.parent: L{buildbot.master.BuildMaster}
      # @rtype getSlave(): L{buildbot.status.builder.SlaveStatus}
      slave_status = self.master_status.getSlave(slave_name)
      if slave_status and not slave_status.isConnected():
        log.msg('[gatekeeper] Slave %s was disconnected, '
                'not closing the tree' % slave_name)
        return False

    # If the previous build step failed with the same result, we don't care
    # about this step.
    previous_build_status = build_status.getPreviousBuild()
    if previous_build_status:
      step_name = self.getName(step_status)
      step_type = self.getGenericName(step_name)
      previous_steps = [step for step in previous_build_status.getSteps()
                        if self.getGenericName(self.getName(step)) == step_type]
      if len(previous_steps) == 1:
        if previous_steps[0].getResults()[0] == FAILURE:
          # The previous same step failed on the previous build. Ignore.
          log.msg('[gatekeeper] Slave %s failed, but previously failed on '
                  'the same step (%s). So not closing tree.' % (
                      (step_name, slave_name)))
          return False
      else:
        log.msg('[gatekeeper] len(previous_steps) == %d which is weird' %
                len(previous_steps))

    # If check_revisions=False that means that the tree closure request is
    # coming from nightly scheduled bots, that need not necessarily have the
    # revision info.
    if not self.check_revisions:
      return True

    # If we don't have a version stamp nor a blame list, then this is most
    # likely a build started manually, and we don't want to close the
    # tree.
    latest_revision = build_utils.getLatestRevision(build_status)
    if not latest_revision or not build_status.getResponsibleUsers():
      log.msg('[gatekeeper] Slave %s failed, but no version stamp, '
              'so skipping.' % slave_name)
      return False

    # If the tree is open, we don't want to close it again for the same
    # revision, or an earlier one in case the build that just finished is a
    # slow one and we already fixed the problem and manually opened the tree.
    # TODO(maruel): This is not git-friendly.
    if latest_revision <= self._last_closure_revision:
      log.msg('[gatekeeper] Slave %s failed, but we already closed it '
              'for a previous revision (old=%s, new=%s)' % (
                  slave_name, str(self._last_closure_revision),
                  str(latest_revision)))
      return False

    log.msg('[gatekeeper] Decided to close tree because of slave %s '
            'on revision %s' % (slave_name, str(latest_revision)))

    # Up to here, in theory we'd check if the tree is closed but this is too
    # slow to check here. Instead, take a look only when we want to close the
    # tree.
    return True

  def buildMessage(self, builder_name, build_status, results, step_name):
    """Check for the tree status before sending a job failure email.

    Asynchronously check for the tree status and return a defered."""
    def send_email(*args):
      return chromium_notifier.ChromiumNotifier.buildMessage(
                 self, builder_name, build_status, results, step_name)

    def stop_if_closed(result):
      """If the tree is already closed, we don't care about this failure."""
      if result.strip() == '0':
        return
      return send_email()

    if not self.tree_status_url:
      # Inconditionally send an email when there is no url.
      return send_email()
    d = client.getPage(self.tree_status_url, agent='buildbot')
    # Add send_email as errback so an email is still sent if the url request
    # fails.
    d.addCallbacks(stop_if_closed, send_email)
    return d

  def getFinishedMessage(self, result, builder_name, build_status, step_name):
    """Closes the tree."""
    log.msg(
        '[gatekeeper] Trying to close the tree at %s.' % self.tree_status_url)

    # isInterestingStep verified that latest_revision has expected properties.
    latest_revision = build_utils.getLatestRevision(build_status)

    # Don't blame last committers if they are not to blame.
    blame_text = ''
    if self.shouldBlameCommitters(step_name):
      blame_text = (' from %s: %s' %
                    (str(latest_revision),
                     ', '.join(build_status.getResponsibleUsers())))

    # Post a request to close the tree.
    tree_message = self.tree_message % {
        'steps': step_name,
        'builder': builder_name,
        'blame': blame_text
    }
    params = urllib.urlencode(
        {
          'message': tree_message,
          'username': 'buildbot@chromium.org',
          'password': self.password,
        })

    def success(result):
      log.msg('[gatekeeper] Tree closed successfully at rev %s' %
              str(latest_revision))
      self._last_closure_revision = latest_revision

    def failure(result):
      log.msg('[gatekeeper] Failed to close the tree at rev %s' %
              str(latest_revision))

    # Trigger the HTTP POST request to update the tree status.
    headers = {'Content-Type': 'application/x-www-form-urlencoded'}
    connection = client.getPage(self.tree_status_url, method='POST',
                                postdata=params, headers=headers,
                                agent='buildbot')
    connection.addCallbacks(success, failure)
    return connection
