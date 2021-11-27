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

cli_sources = ['cli/alice.c',
               'cli/acx_build.c',
               'cli/acx_dump.c',
               'cli/ain_dump.c',
               'cli/ain_compare.c',
               'cli/ain_edit.c',
               'cli/ar_extract.c',
               'cli/ar_list.c',
               'cli/ar_pack.c',
               'cli/cg_convert.c',
               'cli/ex_edit.c',
               'cli/ex_build.c',
               'cli/ex_compare.c',
               'cli/ex_dump.c',
               'cli/fnl_dump.c',
               'cli/flat_build.c',
               'cli/flat_extract.c',
               'cli/project_build.c',
]

libalice = static_library('alice', core_sources,
                          dependencies : tool_deps,
                          c_args : ['-Wno-unused-parameter'],
                          include_directories : incdir)

executable('alice', cli_sources,
           dependencies : tool_deps,
           c_args : ['-Wno-unused-parameter'],
           link_with : libalice,
           include_directories : incdir)