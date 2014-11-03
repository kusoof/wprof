# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Defines a utility class to easily convert classes from and to dict for
serialization.
"""

import json
import sys
import os


TYPE_FLAG = '__persistent_type__'
MODULE_FLAG = '__persistent_module__'


def as_dict(value):
  """Recursively converts an object into a dictionary.

  Converts tuple into list and recursively process each items.
  """
  if hasattr(value, 'as_dict') and callable(value.as_dict):
    return value.as_dict()
  elif isinstance(value, (list, tuple)):
    return [as_dict(v) for v in value]
  elif isinstance(value, dict):
    return dict((as_dict(k), as_dict(v))
                for k, v in value.iteritems())
  elif isinstance(value, (float, int, basestring)) or value is None:
    return value
  else:
    raise AttributeError('Can\'t type %s into a dictionary' % type(value))


def _inner_from_dict(value):
  """Recursively regenerates an object."""
  if isinstance(value, dict):
    if TYPE_FLAG in value:
      return PersistentMixIn.from_dict(value)
    return dict((_inner_from_dict(k), _inner_from_dict(v))
                for k, v in value.iteritems())
  elif isinstance(value, list):
    return [_inner_from_dict(v) for v in value]
  elif isinstance(value, (float, int, basestring)) or value is None:
    return value
  else:
    raise AttributeError('Can\'t load type %s' % type(value))


def to_yaml(obj):
  """Converts a PersisntetMixIn into a yaml-inspired format."""
  def align(x):
    y = x.splitlines(True)
    if len(y) > 1:
      return ''.join(y[0:1] + ['  ' + z for z in y[1:]])
    return x
  def align_value(x):
    if '\n' in x:
      return '\n  ' + align(x)
    return x

  if hasattr(obj, 'as_dict') and callable(obj.as_dict):
    out = (to_yaml(obj.as_dict()),)
  elif isinstance(obj, (float, int, basestring)) or obj is None:
    out = (align(str(obj)),)
  elif isinstance(obj, dict):
    if TYPE_FLAG in obj:
      out = ['%s:' % obj[TYPE_FLAG]]
    else:
      out = []
    for k, v in obj.iteritems():
      if k.startswith('__') or v in (None, '', False, 0):
        continue
      r = align_value(to_yaml(v))
      if not r:
        continue
      out.append('- %s: %s' % (k, r))
  elif hasattr(obj, '__iter__') and callable(obj.__iter__):
    out = ['- %s' % align(to_yaml(x)) for x in obj]
  else:
    out = ('%s' % obj.__class__.__name__,)
  return '\n'.join(out)


class PersistentMixIn(object):
  """Class to be used as a base class to persistent data in a simplistic way.

  persistent class member needs to be set to a tuple containing the instance
  member variable that needs to be saved or loaded.

  TODO(maruel): Use __reduce__!
  """
  persistent = None

  def __new__(cls, *args, **kwargs):
    """Override __new__() to be able to instantiate derived classes without
    calling their __init__() function. This is useful when objects are created
    from a dict.
    """
    result = super(PersistentMixIn, cls).__new__(cls)
    if args or kwargs:
      result.__init__(*args, **kwargs)
    return result

  def as_dict(self):
    """Create a dictionary out of this object."""
    assert isinstance(self.persistent, (list, tuple))
    out = {}
    for member in self.persistent:
      assert isinstance(member, str)
      out[member] = as_dict(getattr(self, member))
    out[TYPE_FLAG] = self.__class__.__name__
    out[MODULE_FLAG] = self.__class__.__module__
    return out

  @staticmethod
  def from_dict(data):
    """Returns an instance of a class inheriting from PersistentMixIn,
    initialized with 'data' dict."""
    datatype = data[TYPE_FLAG]
    if MODULE_FLAG in data and data[MODULE_FLAG] in sys.modules:
      objtype = getattr(sys.modules[data[MODULE_FLAG]], datatype)
    else:
      # Fallback to search for the type in the loaded modules.
      for module in sys.modules.itervalues():
        objtype = getattr(module, datatype, None)
        if objtype:
          break
      else:
        raise KeyError('Couldn\'t find type %s' % datatype)
    obj = PersistentMixIn.__new__(objtype)
    assert isinstance(obj, PersistentMixIn)
    for member in obj.persistent:
      setattr(obj, member, _inner_from_dict(data.get(member, None)))
    return obj

  def __str__(self):
    return to_yaml(self)


def load_from_json_file(filename):
  """Loads one object from a JSON file."""
  try:
    f = open(filename, 'r')
    return PersistentMixIn.from_dict(json.load(f))
  finally:
    f.close()


def save_to_json_file(filename, obj):
  """Save one object in a JSON file."""
  try:
    old = filename + '.old'
    if os.path.exists(filename):
      os.rename(filename, old)
  finally:
    try:
      f = open(filename, 'w')
      json.dump(obj.as_dict(), f, sort_keys=True, indent=2)
      f.write('\n')
    finally:
      f.close()
