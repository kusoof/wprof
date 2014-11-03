# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility classes to generate and manage a BuildFactory to be passed to a
builder dictionary as the 'factory' member, for each builder in c['builders'].

Specifically creates a base BuildFactory that will execute a gclient checkout
first."""

import os
import re

from buildbot.process.properties import WithProperties
from buildbot.steps import trigger

from master.factory.build_factory import BuildFactory
from master.factory import commands


def ShouldRunTest(tests, name):
  """Returns True if |name| is an entry in |tests|."""
  if not tests:
    return False

  if name in tests:
    return True
  return False


def ShouldRunMatchingTest(tests, pattern):
  """Returns True if regex |pattern| matches an entry in |tests|."""
  if not tests:
    return False

  for test in tests:
    if re.match(pattern, test):
      return True
  return False


class GClientSolution(object):
  """Defines a GClient solution."""

  def __init__(self, svn_url, name=None, custom_deps_list=None,
               needed_components=None, custom_vars_list=None,
               custom_deps_file=None, safesync_url=None):
    """ Initialize the GClient Solution.
    Params:
      svn_url: SVN path for this solution.
      name: Name for this solution. if None, it uses the last item in the path.
      custom_deps_list: Modifications to make on the DEPS file.
      needed_components: A map used to skip dependencies when a test is not run.
          The map key is the test name. The map value is an array containing the
          dependencies that are not needed when this test is not run.
      custom_vars_list: Modifications to make on the vars in the DEPS file.
      custom_deps_file: Change the default DEPS filename.
      safesync_url: Select to build based on a lkgr url.
    """
    self.svn_url = svn_url
    self.name = name
    self.custom_deps_list = custom_deps_list or []
    self.custom_vars_list = custom_vars_list or []
    self.custom_deps_file = custom_deps_file
    self.needed_components = needed_components
    self.safesync_url = safesync_url

    if not self.name:
      self.name = svn_url.split('/')[-1]

  def GetSpec(self, tests=None):
    """Returns the specs for this solution.
    Params:
      tests: List of tests to run. This is required only when needed_components
             is not None.
    """

    final_custom_deps_list = self.custom_deps_list[:]
    # Extend the custom deps with everything that is not going to be used
    # in this factory
    if self.needed_components:
      for test, dependencies in self.needed_components.iteritems():
        if ShouldRunMatchingTest(tests, test):
          continue
        final_custom_deps_list.extend(dependencies)

    # Create the custom_deps string
    custom_deps = ''
    for dep in final_custom_deps_list:
      if dep[1] is None:
        dep_url = None
      else:
        dep_url = '"%s"' % dep[1]
      custom_deps += '"%s" : %s, ' % (dep[0], dep_url)

    custom_vars = ''
    for var in self.custom_vars_list:
      if var[1] is None:
        var_value = None
      else:
        var_value = '"%s"' % var[1]
      custom_vars += '"%s" : %s, ' % (var[0], var_value)

    extras = ''
    if self.custom_deps_file:
      extras += '"deps_file": "%s",' % self.custom_deps_file
    if self.safesync_url:
      extras += '"safesync_url": "%s"' % self.safesync_url

    # This must not contain any line breaks or other characters that would
    # require escaping on the command line, since it will be passed to gclient.
    spec = (
      '{ "name": "%s", '
        '"url": "%s", '
        '"custom_deps": {'
                          '%s'
                       '},'
        '"custom_vars": {'
                          '%s'
                       '},'
        '%s'
      '},' % (self.name, self.svn_url, custom_deps, custom_vars, extras)
    )
    return spec


class GClientFactory(object):
  """Encapsulates data and methods common to both (all) master.cfg files."""

  def __init__(self, build_dir, solutions, project=None, target_platform=None,
               nohooks_on_update=False, target_os=None):
    self._build_dir = build_dir
    self._solutions = solutions
    self._target_platform = target_platform or 'win32'
    self._target_os = target_os
    self._nohooks_on_update = nohooks_on_update

    if self._target_platform == 'win32':
      # Look for a solution named for its enclosing directory.
      self._project = project or os.path.basename(self._build_dir) + '.sln'
    else:
      self._project = project

  def BuildGClientSpec(self, tests=None):
    spec = 'solutions = ['
    for solution in self._solutions:
      spec += solution.GetSpec(tests)
    spec += ']'

    if self._target_os:
      spec += ';target_os = ["' + self._target_os + '"]'

    return spec

  def BaseFactory(self, gclient_spec=None, official_release=False,
                  factory_properties=None, build_properties=None,
                  delay_compile_step=False, sudo_for_remove=False,
                  gclient_deps=None, slave_type=None):
    if gclient_spec is None:
      gclient_spec = self.BuildGClientSpec()
    factory_properties = factory_properties or {}
    factory = BuildFactory(build_properties)
    factory_cmd_obj = commands.FactoryCommands(factory,
        target_platform=self._target_platform)
    # First kill any svn.exe tasks so we can update in peace, and
    # afterwards use the checked-out script to kill everything else.
    if (self._target_platform == 'win32' and
        not factory_properties.get('no_kill')):
      factory_cmd_obj.AddSvnKillStep()
    factory_cmd_obj.AddUpdateScriptStep(
        gclient_jobs=factory_properties.get('update_scripts_gclient_jobs'))
    # Once the script is updated, the zombie processes left by the previous
    # run can be killed.
    if (self._target_platform == 'win32' and
        not factory_properties.get('no_kill')):
      factory_cmd_obj.AddTaskkillStep()
    env = factory_properties.get('gclient_env', {})
    # Allow gclient_deps to also come from the factory_properties.
    if gclient_deps == None:
      gclient_deps = factory_properties.get('gclient_deps', None)
    # svn timeout is 2 min; we allow 5
    timeout = factory_properties.get('gclient_timeout')
    if official_release or factory_properties.get('nuke_and_pave'):
      no_gclient_branch = factory_properties.get('no_gclient_branch', False)
      factory_cmd_obj.AddClobberTreeStep(gclient_spec, env, timeout,
          gclient_deps=gclient_deps, gclient_nohooks=self._nohooks_on_update,
          no_gclient_branch=no_gclient_branch)
    elif not delay_compile_step:
      self.AddUpdateStep(gclient_spec, factory_properties, factory,
                         slave_type, sudo_for_remove, gclient_deps=gclient_deps)
    return factory

  def BuildFactory(self, target='Release', clobber=False, tests=None, mode=None,
                   slave_type='BuilderTester', options=None,
                   compile_timeout=1200, build_url=None, project=None,
                   factory_properties=None, gclient_deps=None,
                   target_arch=None):
    factory_properties = factory_properties or {}
    if (options and
        '--build-tool=ninja' in options and '--compiler=goma-clang' in options):
      # Ninja needs CC and CXX set at gyp time.
      factory_properties['gclient_env']['CC'] = 'clang'
      factory_properties['gclient_env']['CXX'] = 'clang++'

    # Create the spec for the solutions
    gclient_spec = self.BuildGClientSpec(tests)

    # Initialize the factory with the basic steps.
    factory = self.BaseFactory(gclient_spec,
                               factory_properties=factory_properties,
                               slave_type=slave_type,
                               gclient_deps=gclient_deps)

    # Get the factory command object to create new steps to the factory.
    factory_cmd_obj = commands.FactoryCommands(factory, target,
                                               self._build_dir,
                                               self._target_platform,
                                               target_arch)

    # Update clang if necessary.
    gclient_env = factory_properties.get('gclient_env', {})
    if ('clang=1' in gclient_env.get('GYP_DEFINES', '') or
        'asan=1' in gclient_env.get('GYP_DEFINES', '')):
      factory_cmd_obj.AddUpdateClangStep()

    # Add a step to cleanup temporary files and data left from a previous run
    # to prevent the drives from becoming full over time.
    factory_cmd_obj.AddTempCleanupStep()

    # Add the compile step if needed.
    if slave_type in ['BuilderTester', 'Builder', 'Trybot', 'Indexer']:
      factory_cmd_obj.AddCompileStep(project or self._project, clobber,
                                     mode=mode, options=options,
                                     timeout=compile_timeout)

    # Archive the full output directory if the machine is a builder.
    if slave_type == 'Builder':
      factory_cmd_obj.AddZipBuild(halt_on_failure=True,
                                  factory_properties=factory_properties)

    # Download the full output directory if the machine is a tester.
    if slave_type == 'Tester':
      factory_cmd_obj.AddExtractBuild(build_url,
                                      factory_properties=factory_properties)

    return factory

  # pylint: disable=R0201
  def TriggerFactory(self, factory, slave_type, factory_properties):
    """Add post steps on a build created by BuildFactory."""
    # Trigger any schedulers waiting on the build to complete.
    factory_properties = factory_properties or {}
    if factory_properties.get('trigger') is None:
      return

    trigger_name = factory_properties.get('trigger')
    # Propagate properties to the children if this is set in the factory.
    trigger_properties = factory_properties.get('trigger_properties', [])
    factory.addStep(trigger.Trigger(
        schedulerNames=[trigger_name],
        updateSourceStamp=False,
        waitForFinish=False,
        set_properties={
            # Here are the standard names of the parent build properties.
            'parent_buildername': WithProperties('%(buildername:-)s'),
            'parent_buildnumber': WithProperties('%(buildnumber:-)s'),
            'parent_branch': WithProperties('%(branch:-)s'),
            'parent_got_revision': WithProperties('%(got_revision:-)s'),
            'parent_got_webkit_revision':
                WithProperties('%(got_webkit_revision:-)s'),
            'parent_got_nacl_revision':
                WithProperties('%(got_nacl_revision:-)s'),
            'parent_revision': WithProperties('%(revision:-)s'),
            'parent_scheduler': WithProperties('%(scheduler:-)s'),
            'parent_slavename': WithProperties('%(slavename:-)s'),

            # And some scripts were written to use non-standard names.
            'parent_cr_revision': WithProperties('%(got_revision:-)s'),
            'parent_wk_revision': WithProperties('%(got_webkit_revision:-)s'),
            'parentname': WithProperties('%(buildername)s'),
            'parentslavename': WithProperties('%(slavename:-)s'),
            },
        copy_properties=trigger_properties))

  def AddUpdateStep(self, gclient_spec, factory_properties, factory,
                    slave_type, sudo_for_remove=False, gclient_deps=None):
    if gclient_spec is None:
      gclient_spec = self.BuildGClientSpec()
    factory_properties = factory_properties or {}

    # Get the factory command object to add update step to the factory.
    factory_cmd_obj = commands.FactoryCommands(factory,
        target_platform=self._target_platform)

    # Get variables needed for the update.
    env = factory_properties.get('gclient_env', {})
    timeout = factory_properties.get('gclient_timeout')

    no_gclient_branch = factory_properties.get('no_gclient_branch', False)

    gclient_transitive = factory_properties.get('gclient_transitive', False)
    primary_repo = factory_properties.get('primary_repo', '')
    gclient_jobs = factory_properties.get('gclient_jobs')

    # Add the update step.
    factory_cmd_obj.AddUpdateStep(
        gclient_spec,
        env=env,
        timeout=timeout,
        sudo_for_remove=sudo_for_remove,
        gclient_deps=gclient_deps,
        gclient_nohooks=True,
        no_gclient_branch=no_gclient_branch,
        gclient_transitive=gclient_transitive,
        primary_repo=primary_repo,
        gclient_jobs=gclient_jobs)

    if slave_type == 'Trybot':
      factory_cmd_obj.AddApplyIssueStep()

    if not self._nohooks_on_update:
      factory_cmd_obj.AddRunHooksStep(env=env, timeout=timeout)
