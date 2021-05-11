# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

VERSION='0.0.1'
APPNAME='ndn-example'

def options(opt):
    opt.load('compiler_cxx default-compiler-flags')

def configure(conf):
    conf.load("compiler_cxx default-compiler-flags")

    conf.check_cfg(package='libndn-cxx', args=['--cflags', '--libs'],
                   uselib_store='NDN_CXX', global_define=True, mandatory=True)

    conf.check_cfg(package='python-2.7', args=['--cflags', '--libs'],
                   uselib_store='PYTHON', global_define=True, mandatory=True)

    conf.check_cfg(package='Consumer-Producer-API', args=['--cflags', '--libs'],
               uselib_store='CONSUMERPRODUCERAPI', global_define=True, mandatory=True)

def build (bld):
    bld(target='client',
        features=['cxx', 'cxxprogram'],
        source='client.cpp',
        use='NDN_CXX')

    bld(target='server',
        features=['cxx', 'cxxprogram'],
        source='server.cpp',
        use='NDN_CXX')

    bld(target='function',
        features=['cxx', 'cxxprogram'],
        source='function.cpp',
        use='NDN_CXX PYTHON CONSUMERPRODUCERAPI')

    bld(target='consumer',
        features=['cxx', 'cxxprogram'],
        source='consumer.cpp',
        use='NDN_CXX PYTHON CONSUMERPRODUCERAPI')

    bld(target='producer',
        features=['cxx', 'cxxprogram'],
        source='producer.cpp',
        use='NDN_CXX PYTHON CONSUMERPRODUCERAPI')

    bld(target='sfc_function',
        features=['cxx', 'cxxprogram'],
        source='sfc_function.cpp',
        use='NDN_CXX PYTHON')

    bld(target='sfc_producer',
        features=['cxx', 'cxxprogram'],
        source='sfc_producer.cpp',
        use='NDN_CXX PYTHON')

    bld(target='sfc_consumer',
        features=['cxx', 'cxxprogram'],
        source='sfc_consumer.cpp',
        use='NDN_CXX PYTHON')
