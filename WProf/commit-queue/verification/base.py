# coding=utf8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Defines base classes for pending change verification classes."""

import model


# Verifier state in priority level.
# SUCCEEDED : This verifier is fine to commit this patch.
# PROCESSING: No decision was made yet. The verifier runs asynchronously.
# FAILED    : Verification failed, this patch must not be committed.
# IGNORED   : This patch must be ignored and no comment added to the code
#             review.
SUCCEEDED, PROCESSING, FAILED, IGNORED = range(4)
VALID_STATES = set((SUCCEEDED, PROCESSING, FAILED, IGNORED))


class DiscardPending(Exception):
  """Exception to be raised when a pending item should be discarded."""

  def __init__(self, pending, status):
    super(DiscardPending, self).__init__(status)
    self.pending = pending
    self.status = status


class Verified(model.PersistentMixIn):
  """A set of verifications that are for a specific patch."""
  persistent = [
      'verifications',
  ]

  def __init__(self):
    super(Verified, self).__init__()
    self.verifications = {}

  def pending_name(self):
    raise NotImplementedError()

  def get_state(self):
    """Returns the combined state of all the verifiers for this item.

    Use priority with the states: IGNORED > FAILED > PROCESSING > SUCCEEDED.
    """
    if not self.verifications:
      return PROCESSING
    states = set(v.get_state() for v in self.verifications.itervalues())
    assert states.issubset(VALID_STATES)
    return max(states)

  def postpone(self):
    """This item shouldn't be committed right now.

    Call repeatedly until it returns False. This is a potentially slow call so
    only call it when get_state() returns SUCEEDED.
    """
    return any(v.postpone() for v in self.verifications.itervalues())

  def error_message(self):
    """Returns all the error messages concatenated if any."""
    out = (i.error_message for i in self.verifications.itervalues())
    return '\n\n'.join(filter(None, out))

  def apply_patch(self, context, prepare):
    """Applies a patch from the codereview tool to the checkout."""
    raise NotImplementedError()


class IVerifierStatus(model.PersistentMixIn):
  """Interface for objects in Verified.verifications dictionary."""
  persistent = [
      'error_message'
  ]

  def __init__(self, error_message=None):
    super(IVerifierStatus, self).__init__()
    self.error_message = error_message

  def get_state(self):
    """See Verified.get_state()."""
    raise NotImplementedError()

  def postpone(self):  # pylint: disable=R0201
    """See Verified.postpone()."""
    return False


class SimpleStatus(IVerifierStatus):
  """Base class to be used for simple true/false verifiers."""
  persistent = IVerifierStatus.persistent + [
      'state',
  ]

  def __init__(self, state=PROCESSING, error_message=None):
    super(SimpleStatus, self).__init__(error_message)
    self.state = state

  def get_state(self):
    return self.state


class Verifier(object):
  name = None

  def __init__(self):
    assert self.name is not None

  def verify(self, pending):
    """Verifies a pending change.

    Called with os.getcwd() == checkout.project_path.
    """
    raise NotImplementedError()

  def update_status(self, queue):
    """Updates the status of all pending changes, for asynchronous checks.

    It is not necessarily called from inside checkout.project_path.
    """
    raise NotImplementedError()

  def loop(self, queue, gen_obj, pending_only):
    """Loops in a pending queue and returns the verified item corresponding to
    the Verifier.
    """
    for pending in queue:
      if self.name not in pending.verifications:
        pending.verifications[self.name] = gen_obj()
      if (not pending_only or
          pending.verifications[self.name].get_state() == PROCESSING):
        yield pending, pending.verifications[self.name]


class VerifierCheckout(Verifier):  # pylint: disable=W0223
  """A verifier that needs a rietveld and checkout objects.

  When verify() is called, it is guaranteed that the patch is applied on the
  checkout.
  """
  def __init__(self, context_obj):
    super(VerifierCheckout, self).__init__()
    self.context = context_obj

  def send_status(self, pending, data):
    self.context.status.send(
        pending, {'verification': self.name, 'payload': data})
