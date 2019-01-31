#!/usr/bin/env python3
import sys
import os
import subprocess as sp
from distutils.core import setup, Extension

try:
    CONFIG_DIR = os.environ['CONFIG_DIR']
except:
    CONFIG_DIR = '../..'
    print("Export CONFIG_DIR where config.h is; using ../..")
pass

libraries = []

if sys.version_info[0] < 3:
    boost_python = 'boost_python'
else:
    if os.path.exists('/usr/local/lib/libboost_python3.so') or os.path.exists('/usr/lib/x86_64-linux-gnu/libboost_python3.so'):
        boost_python = 'boost_python3'
    else:
        boost_python = 'boost_python%d%d' % (sys.version_info[0], sys.version_info[1])
    pass

libraries.extend(['kgraph', 'boost_timer', 'boost_chrono', 'boost_log_setup', 'boost_log', 'boost_filesystem', 'boost_system', 'boost_container', boost_python])

donkey = Extension('donkey',
        language = 'c++',
        extra_compile_args = ['-O3', '-std=c++1y', '-g', '-frtti', '-DBOOST_LOG_DYN_LINK=1'], #, '-DBOOST_ERROR_CODE_HEADER_ONLY'], 
		include_dirs = ['/usr/local/include', CONFIG_DIR, '.'],
        libraries = libraries,
        library_dirs = ['/usr/local/lib'],

        sources = ['python-api.cpp', 'donkey.cpp', 'logging.cpp', 'index-kgraph.cpp', 'index-lsh.cpp', 'kgraph_lite.cpp', 'fixed_monotonic_buffer_resource.cpp'],
        undef_macros = [ "NDEBUG" ]
        )

setup (name = 'donkey',
       version = '0.0.1',
       author = 'Wei Dong',
       author_email = 'wdong@wdong.org',
       license = 'BSD',
       description = 'This is a demo package',
       ext_modules = [donkey],
       )

