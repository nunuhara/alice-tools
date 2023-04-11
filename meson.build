project('alice-tools', 'c', 'cpp',
        default_options : ['c_std=c11'])
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

static_libs = false
if host_machine.system() == 'windows'
    static_libs = true
endif

zlib = dependency('zlib', static : static_libs)
libm = meson.get_compiler('c').find_library('m', required: false)

flex = find_program('flex')
bison = find_program('bison')

libsys4_proj = subproject('libsys4')
libsys4_dep = libsys4_proj.get_variable('libsys4_dep')

if meson.get_compiler('c').has_function('iconv')
    tool_deps = [libm, zlib, libsys4_dep]
else
    iconv = dependency('iconv', static : static_libs)
    tool_deps = [libm, zlib, iconv, libsys4_dep]
endif

incdir = include_directories('include')

flexgen = generator(flex,
                    output : '@BASENAME@.yy.c',
                    arguments : ['-o', '@OUTPUT@', '@INPUT@'])

bisongen = generator(bison,
                     output: ['@BASENAME@.tab.c', '@BASENAME@.tab.h'],
                     arguments : ['--verbose', '--debug', '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

subdir('src')
