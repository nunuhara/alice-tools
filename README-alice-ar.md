alice-ar
========

This is a tool for extracting files from various AliceSoft archive formats
(.ald, .afa). It should be capable of extracting all .ald/.afa file up to
Haha Ranman (the latest game at the time of this writing).

Usage
-----

First of all, if you're running this tool from the Windows command prompt, you
should run the command `chcp 65001` to change the console's code page to UTF-8,
and then change the console's font to something which supports Japanese
characters (e.g. MS Mincho). Otherwise you will see a lot of garbled text
flying across the screen when running this tool.

To list the contents of an archive,

    alice-ar -l archive.afa

To extract files from an archive to the current directory,

    alice-ar -x archive.afa

To extract files from an archive to the directory "out",

    alice-ar -x -o out archive.afa

To view the available command line options,

    alice-ar --help

When extracting archives containing .flat files, alice-ar will recursively
extract the .flat files by default (these files contain images along with
various other data). Usually you don't care about anything other than the
images when extracting these archives, so you should pass the --images-only
flag,

    alice-ar -x --images-only archiveFlat.afa

You can pass the --raw flag to prevent alice-ar from converting any files,

    alice-ar -x --raw archive.afa

When the --raw flag is given, alice-ar will not recursively extract .flat files
or convert images to .png format.
