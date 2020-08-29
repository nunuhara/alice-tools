exdump = ['../conv.c',
          'dump.c',
          'exdump.c',
]

exbuild = ['../conv.c',
           'ast.c',
           'exbuild.c',
           'pack.c',
]

exbuild += flexgen.process('ex_lexer.l')
exbuild += bisongen.process('ex_parser.y')

executable('exdump', exdump,
           dependencies : tool_deps,
           c_args : ['-Wno-unused-parameter'],
           include_directories : incdir)

executable('exbuild', exbuild,
           dependencies : tool_deps,
           c_args : ['-Wno-unused-parameter'],
           include_directories : incdir)
