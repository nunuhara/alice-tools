project('alice-tools', 'c', 'cpp',
        default_options : ['c_std=c11'])
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

static_libs = false
if host_machine.system() == 'windows'
    static_libs = true
endif

zlib = dependency('zlib', static : static_libs)
libm = meson.get_compiler('c').find_library('m', required: false)

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules : ['Core', 'Gui', 'Widgets'],
                     required : false)

flex = find_program('flex')
bison = find_program('bison')
update_mime = find_program('update-mime-database', required: false)
install_desktop = find_program('desktop-file-install', required: false)

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

# install mime types so that file associations can be established
if update_mime.found()
    install_data('linux/technology.haniwa.galice-mime.xml',
        install_dir : get_option('datadir') / 'mime' / 'packages')
    meson.add_install_script('sh', '-c', ' '.join([
        update_mime.full_path(), '-V', '${MESON_INSTALL_DESTDIR_PREFIX}' / get_option('datadir') / 'mime',
    ]))
endif

# .desktop file and icon for galice
if qt5_dep.found() and install_desktop.found()
    install_data('linux/technology.haniwa.galice.svg',
        install_dir : get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps')
    meson.add_install_script('sh', '-c', ' '.join([
        install_desktop.full_path(),
        '--rebuild-mime-info-cache',
        '--dir', '${MESON_INSTALL_DESTDIR_PREFIX}' / get_option('datadir') / 'applications',
        '${MESON_SOURCE_ROOT}' / 'linux' / 'technology.haniwa.galice.desktop'
    ]))
endif

# thumbnailer
install_data('linux/alicetools.thumbnailer', install_dir : get_option('datadir') / 'thumbnailers')
