#!/usr/bin/env python3

import sys
import os
import os.path
import glob
import re

if sys.version_info < (3,):
    def isbasestring(s):
        return isinstance(s,basestring)
else:
    def isbasestring(s):
        return isinstance(s, (str,bytes))

def add_kel_source_files(self, sources, filetype, lib_env=None, shared=False, target_post=""):

    if isbasestring(filetype):
        dir_path = self.Dir('.').abspath
        filetype = sorted(glob.glob(dir_path+"/"+filetype))

    for path in filetype:
        target_name = re.sub( r'(.*?)(\.cpp|\.c\+\+)', r'\1' + target_post, path )
        if shared:
            target_name+='.os'
            sources.append( self.SharedObject( target=target_name, source=path ) )
        else:
            target_name+='.o'
            sources.append( self.StaticObject( target=target_name, source=path ) )
    pass

env=Environment(CPPPATH=['#source'],
    CXX='clang++',
    CPPDEFINES=['GIN_UNIX'],
    CXXFLAGS=['-std=c++17','-g','-Wall','-Wextra'],
    LIBS=[])
env.__class__.add_source_files = add_kel_source_files

env.sources = []
env.headers = []
env.objects = []

Export('env')
SConscript('source/SConscript')
SConscript('test/SConscript')

# Clang format part
env.Append(BUILDERS={'ClangFormat' : Builder(action = 'clang-format --style=file -i $SOURCE')})
env.format_actions = []
def format_iter(env,files):
    for f in files:
        env.format_actions.append(env.AlwaysBuild(env.ClangFormat(target=f+"-clang-format",source=f)))
    pass

format_iter(env,env.sources + env.headers)

env.Alias('format', env.format_actions)
env.Alias('test', env.test_program)
