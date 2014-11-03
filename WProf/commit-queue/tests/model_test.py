#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for model.py."""

import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT_DIR, '..'))
from model import MODULE_FLAG, PersistentMixIn, TYPE_FLAG


class Invalid(PersistentMixIn):
  pass


class MemberNotFound(PersistentMixIn):
  persistent = ('a',)

  @staticmethod
  def test_me(x):
    return x + 1


class Basic(PersistentMixIn):
  persistent = ('a', 'b')
  a = None
  b = None


class Inner(PersistentMixIn):
  persistent = ('c', 'd')

  def __init__(self):
    super(Inner, self).__init__()
    self.c = Basic()
    self.c.a = 'hello'
    self.d = 'foo'
    self.extra = 'extra'


class Serialize(unittest.TestCase):
  def testInvalid(self):
    a = Invalid()
    self.assertRaises(AssertionError, a.as_dict)

  def testMemberNotFound(self):
    a = MemberNotFound()
    self.assertRaises(AttributeError, a.as_dict)

  def testBasic(self):
    a = Basic()
    a.b = 23.2
    expected = {
        'a': None,
        'b': 23.2,
        TYPE_FLAG: 'Basic',
        MODULE_FLAG: '__main__',
    }
    self.assertEquals(expected, a.as_dict())

  def testInner(self):
    a = Inner()
    expected = {
      'c': {
        'a': 'hello',
        'b': None,
        TYPE_FLAG: 'Basic',
        MODULE_FLAG: '__main__',
      },
      TYPE_FLAG: 'Inner',
      MODULE_FLAG: '__main__',
      'd': 'foo',
    }
    self.assertEquals(expected, a.as_dict())

  def testInnerList(self):
    """Test serialization of:
    - Embedded objects
    - list
    - dict
    - string
    - int
    """
    a = Basic()
    a.b = [Basic(), 23]
    a.a = {'x': 'y'}
    expected = {
      'a': {'x': 'y'},
      'b': [
        {
          'a': None,
          'b': None,
          TYPE_FLAG: 'Basic',
          MODULE_FLAG: '__main__',
        },
        23
      ],
      TYPE_FLAG: 'Basic',
      MODULE_FLAG: '__main__',
    }
    self.assertEquals(expected, a.as_dict())


class Unas_dict(unittest.TestCase):
  def testInvalid(self):
    data = { TYPE_FLAG: 'Invalid' }
    self.assertRaises(TypeError, PersistentMixIn.from_dict, data)

  def testNotFound(self):
    data = { TYPE_FLAG: 'DoesNotExists' }
    self.assertRaises(KeyError, PersistentMixIn.from_dict, data)

  def testEmpty(self):
    data = { }
    self.assertRaises(KeyError, PersistentMixIn.from_dict, data)

  def testBasic(self):
    data = {
        'a': None,
        'b': 23.2,
        TYPE_FLAG: 'Basic',
        MODULE_FLAG: '__main__',
    }
    a = PersistentMixIn.from_dict(data)
    self.assertEquals(Basic, type(a))
    self.assertEquals(None, a.a)
    self.assertEquals(23.2, a.b)

  def testInner(self):
    data = {
      'c': {
        'a': 42,
        'b': [1, 2],
        TYPE_FLAG: 'Basic',
        MODULE_FLAG: '__main__',
      },
      TYPE_FLAG: 'Inner',
      MODULE_FLAG: '__main__',
      'd': 'foo2',
    }
    a = PersistentMixIn.from_dict(data)
    self.assertEquals(Inner, type(a))
    self.assertEquals(Basic, type(a.c))
    self.assertEquals(42, a.c.a)
    self.assertEquals([1, 2], a.c.b)
    self.assertEquals('foo2', a.d)
    # Make sure __init__ is not called.
    self.assertFalse(hasattr(a, 'extra'))

  def testInnerList(self):
    """Test unserialization of:
    - Embedded objects
    - list
    - dict
    - string
    - int
    """
    data = {
      'a': None,
      'b': [
        {
          'a': {'x': 'y'},
          'b': None,
          TYPE_FLAG: 'Basic',
          MODULE_FLAG: '__main__',
        },
        23
      ],
      TYPE_FLAG: 'Basic',
      MODULE_FLAG: '__main__',
    }
    a = PersistentMixIn.from_dict(data)
    self.assertEquals(Basic, type(a))
    self.assertEquals(None, a.a)
    self.assertEquals(2, len(a.b))
    self.assertEquals(Basic, type(a.b[0]))
    self.assertEquals({'x': 'y'}, a.b[0].a)
    self.assertEquals(None, a.b[0].b)
    self.assertEquals(23, a.b[1])

  def testMemberFunction(self):
    data = {
      TYPE_FLAG: 'MemberNotFound',
    }
    a = PersistentMixIn.from_dict(data)
    self.assertEquals(MemberNotFound, type(a))
    # Make sure the member functions are accessible.
    self.assertEquals(3, a.test_me(2))


if __name__ == '__main__':
  unittest.main()
