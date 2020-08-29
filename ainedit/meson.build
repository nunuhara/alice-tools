# sources for ainedit
ainedit = ['../cJSON.c',
           '../conv.c',
           'ainedit.c',
           'asm.c',
           'jaf_ast.c',
           'jaf_compile.c',
           'jaf_eval.c',
           'jaf_static_analysis.c',
           'jaf_types.c',
           'json.c',
           'pje.c',
           'repack.c',
           'text.c',
           'transcode.c',
]

ainedit += flexgen.process('asm_lexer.l', 'text_lexer.l', 'jaf_lexer.l')
ainedit += bisongen.process('asm_parser.y', 'text_parser.y', 'jaf_parser.y')

executable('ainedit', ainedit,
           dependencies : tool_deps,
           c_args : ['-Wno-unused-parameter'],
           include_directories : incdir)
