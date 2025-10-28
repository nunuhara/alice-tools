core_sources = ['core/acx.c',
                'core/ain/asm.c',
                'core/ain/dasm.c',
                'core/ain/dump.c',
                'core/ain/guess_filenames.c',
                'core/ain/json_dump.c',
                'core/ain/json_read.c',
                'core/ain/macros.c',
                'core/ain/repack.c',
                'core/ain/text.c',
                'core/ain/transcode.c',
                'core/ar/extract.c',
                'core/ar/manifest_parser.c',
                'core/ar/open.c',
                'core/ar/pack.c',
                'core/ar/write_afa.c',
                'core/ex/ast.c',
                'core/ex/dump.c',
                'core/ex/pack.c',
                'core/flat.c',
                'core/jaf/ain.c',
                'core/jaf/ast.c',
                'core/jaf/compile.c',
                'core/jaf/declaration.c',
                'core/jaf/error.c',
                'core/jaf/eval.c',
                'core/jaf/resolve.c',
                'core/jaf/static_analysis.c',
                'core/jaf/types.c',
                'core/jaf/visitor.c',
                'core/pje.c',
                'core/cJSON.c',
                'core/conv.c',
                'core/port.c',
                'core/scale.c',
                'core/util.c',
]

core_sources += flexgen.process('core/csv_lexer.l')
core_sources += bisongen.process('core/csv_parser.y')

core_sources += flexgen.process('core/ain/asm_lexer.l')
core_sources += bisongen.process('core/ain/asm_parser.y')

core_sources += flexgen.process('core/ain/text_lexer.l')
core_sources += bisongen.process('core/ain/text_parser.y')

core_sources += flexgen.process('core/ar/manifest_lexer.l')
core_sources += bisongen.process('core/ar/manifest_parser.y')

core_sources += flexgen.process('core/ex/txtex_lexer.l')
core_sources += bisongen.process('core/ex/txtex_parser.y')

core_sources += flexgen.process('core/jaf/jaf_lexer.l')
core_sources += bisongen.process('core/jaf/jaf_parser.y')

libalice = static_library('alice', core_sources,
                          dependencies : tool_deps,
                          c_args : ['-Wno-unused-parameter'],
                          include_directories : incdir)

cli_sources = ['cli/alice.c',
               'cli/acx_build.c',
               'cli/acx_dump.c',
               'cli/ain_dump.c',
               'cli/ain_compare.c',
               'cli/ain_edit.c',
               'cli/ar_extract.c',
               'cli/ar_list.c',
               'cli/ar_pack.c',
               'cli/asd_build.c',
               'cli/asd_dump.c',
               'cli/cg_convert.c',
               'cli/cg_thumbnail.c',
               'cli/ex_edit.c',
               'cli/ex_build.c',
               'cli/ex_compare.c',
               'cli/ex_dump.c',
               'cli/fnl_dump.c',
               'cli/flat_build.c',
               'cli/flat_extract.c',
               'cli/project_build.c',
               'cli/project_init.c',
]

static_link_args = []
if host_machine.system() == 'windows'
    static_link_args = ['-static']
endif

executable('alice', cli_sources,
           dependencies : tool_deps,
           c_args : ['-Wno-unused-parameter'],
           link_args : static_link_args,
           link_with : libalice,
           include_directories : incdir,
           install : true)

gui_sources = ['gui/galice.cpp',
               'gui/acx_model.cpp',
               'gui/acx_view.cpp',
               'gui/ex_table_model.cpp',
               'gui/ex_table_view.cpp',
               'gui/ex_view.cpp',
               'gui/filesystem_view.cpp',
               'gui/jaf_view.cpp',
               'gui/jam_view.cpp',
               'gui/mainwindow.cpp',
               'gui/navigator.cpp',
               'gui/navigator_model.cpp',
               'gui/navigator_node.cpp',
               'gui/navigator_view.cpp',
               'gui/sjisdecoder.cpp',
               'gui/syntax_highlighter.cpp',
               'gui/viewer.cpp',
]

gui_moc = ['gui/acx_model.hpp',
           'gui/acx_view.hpp',
           'gui/ex_table_model.hpp',
           'gui/ex_table_view.hpp',
           'gui/ex_view.hpp',
           'gui/filesystem_view.hpp',
           'gui/galice.hpp',
           'gui/jaf_view.hpp',
           'gui/jam_view.hpp',
           'gui/mainwindow.hpp',
           'gui/navigator.hpp',
           'gui/navigator_model.hpp',
           'gui/navigator_view.hpp',
           'gui/sjisdecoder.hpp',
           'gui/syntax_highlighter.hpp',
           'gui/viewer.hpp',
]

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules : ['Core', 'Gui', 'Widgets'],
                     required : false)

if qt5_dep.found()
    generated_files = qt5.preprocess(moc_headers : gui_moc,
                                     dependencies : [qt5_dep])

    # NOTE: static linking of Qt not supported by meson per
    #       https://github.com/mesonbuild/meson/pull/7733
    executable('galice', gui_sources, generated_files,
               dependencies : [tool_deps, qt5_dep],
               link_with : libalice,
               include_directories : incdir,
               gui_app : true,
               install : true)
endif
