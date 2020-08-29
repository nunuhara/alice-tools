acxdump = ['../conv.c',
           'acxdump.c',
]

acxbuild = ['../conv.c',
            'acxbuild.c',
]

acxbuild += flexgen.process('csv_lexer.l')
acxbuild += bisongen.process('csv_parser.y')

executable('acxdump', acxdump,
           dependencies : tool_deps,
           c_args : ['-Wno-unused-parameter'],
           include_directories : incdir)

executable('acxbuild', acxbuild,
           dependencies : tool_deps,
           c_args : ['-Wno-unused-parameter'],
           include_directories : incdir)
