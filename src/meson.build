sources = ['acx/build.c',
           'acx/dump.c',
           'aincmp.c',
           'aindump/aindump.c',
           'aindump/guess_filenames.c',
           'aindump/json.c',
           'aindump/macros.c',
           'ainedit/ainedit.c',
           'ainedit/asm.c',
           'ainedit/jaf_ast.c',
           'ainedit/jaf_compile.c',
           'ainedit/jaf_eval.c',
           'ainedit/jaf_static_analysis.c',
           'ainedit/jaf_types.c',
           'ainedit/json.c',
           'ainedit/pje.c',
           'ainedit/repack.c',
           'ainedit/text.c',
           'ainedit/transcode.c',
           'alice.c',
           'ar/afa_extract.c',
           'ar/extract.c',
           'ar/list.c',
           'ar/open.c',
           'cJSON.c',
           'conv.c',
           'dasm.c',
           'ex/ast.c',
           'ex/dump.c',
           'ex/exbuild.c',
           'ex/excmp.c',
           'ex/exdump.c',
           'ex/pack.c',
           'util.c',
]

sources += flexgen.process('acx/csv_lexer.l')
sources += bisongen.process('acx/csv_parser.y')

sources += flexgen.process('ainedit/asm_lexer.l', 'ainedit/text_lexer.l', 'ainedit/jaf_lexer.l')
sources += bisongen.process('ainedit/asm_parser.y', 'ainedit/text_parser.y', 'ainedit/jaf_parser.y')

sources += flexgen.process('ex/ex_lexer.l')
sources += bisongen.process('ex/ex_parser.y')

executable('alice', sources,
           dependencies : tool_deps,
           c_args : ['-Wno-unused-parameter'],
           include_directories : incdir)