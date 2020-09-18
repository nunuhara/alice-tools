Editing .ex files
-----------------

First, dump the file with the `alice ex dump` command as follows:

     alice ex dump -o out.x Rance10EX.ex

This will create a file named "out.x" containing all of the data structures
from the file "Rance10EX.ex".

This file uses a C-like syntax. At the top level, there should be a series of
assignment statements of the form:

    <type> <name> = <data>;

where `<type>` is one of `int`, `float`, `string`, `table`, `tree` and `list`,
and `<data>` is an expression corresponding to the type of data. `<name>` is
just a name, and may be surrounded in quotation marks if the name contains
special characters.

Once you've finished editing out.x, you can rebuild the .ex file using the
following command:

    alice ex build -o out.ex out.x

This takes the data from the file "out.x" (which you have just edited) and
builds the .ex file "out.ex". You can then replace the .ex file in your game
directory with this file.

If you are building a file for a game older than Evenicle (e.g. Rance 01) you
should pass the "--old" flag to the command. E.g.

    alice ex build -o out.ex --old out.x

### Splitting the dump into multiple files

If the -s,--split option is passed to exdump, the .ex file will be dumped to
multiple files (one per top-level data structure). E.g.

    alice ex dump -o out.x -s Rance10EX.ex

After running this command, there should be a number of .x files created in
the current directory containing the data from "Rance10EX.ex". The file "out.x"
should contain a list of `#include "..."` directives which will stitch the full
dump back together when rebuilding with exbuild.

