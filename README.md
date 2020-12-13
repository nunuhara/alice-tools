alice-tools
===========

This is a collection of command-line tools for viewing and editing file formats
used in AliceSoft games.

Building
--------

First install the dependencies (corresponding Debian package in parentheses):

* bison (bison)
* flex (flex)
* meson (meson)
* libpng (libpng-dev)
* libturbojpeg (libturbojpeg0-dev)
* libwebp (libwebp-dev)
* zlib (zlib1g-dev)

Then fetch the git submodules,

    git submodule init
    git submodule update

(Alternatively, pass `--recurse-submodules` when cloning this repository)

Then build the tools with meson,

    mkdir build
    meson build
    ninja -C build

Usage
-----

All of the tools are accessed through the single `alice` executable. Running
`alice` or any command without arguments will print the relevant usage
instructions. E.g.

    alice
    alice ain
    alice ain dump
    
The currently implemented commands are:

    alice acx build   - Build a .acx file from a .csv
    alice acx dump    - Dump the contents of a .acx file to .csv
    alice ain compare - Compare .ain files
    alice ain dump    - Dump various info fram a .ain file
    alice ain edit    - Edit a .ain file
    alice ar  extract - Extract an archive file
    alice ar  list    - List the contents of an archive file
    alice ex  build   - Build a .ex file
    alice ex  compare - Compare .ex files
    alice ex  dump    - Dump the contents of a .ex file

### Editing .ain files

See [README-ain.md](https://haniwa.technology/alice-tools/README-ain.html)

### Editing .ex files

See [README-ex.md](https://haniwa.technology/alice-tools/README-ex.html)

### Editing .acx files

See [README-acx.md](https://haniwa.technology/alice-tools/README-acx.html)

### Extracting archives

See [README-alice-ar.md](https://haniwa.technology/alice-tools/README-alice-ar.html)

Known Limitations/Bugs
----------------------

* Non-ASCII file paths aren't handled correctly on Windows. You will have to
  rename any files with Japanese characters in their name before using these
  tools on them.
* aindump only supports dumping to a single file, which can be quite large.

Source Code
-----------

The source code is available [on github](http://github.com/nunuhara/alice-tools).

Reporting Bugs
--------------

You can report bugs on the issue tracker at github, contact me via email at
nunuhara@haniwa.technology, or find me on /haniho/.

Version History
---------------

### [Version 0.8.0](https://haniwa.technology/alice-tools/alice-tools-0.8.0.zip)

* Add `ar pack` command for creating AFAv2 archives
* Fix issues with ALD files on Windows
* Support indexing string table by value when using `ain edit -t` command

### [Version 0.7.0](https://haniwa.technology/alice-tools/alice-tools-0.7.0.zip)

* Combine all commands into the single binary 'alice'
* Fix issue with the Dohna Dohna trial version .ain file
* Fix issue with unescaped carriage returns characters in output
* Improve error messages

### [Version 0.6.0](https://haniwa.technology/alice-tools/alice-tools-0.6.0.zip)

* Add acxdump/acxbuild tools for editing .acx files
* Support extracting .ogg files from afa v3 archives
* Fix crash when extracting CG archive from MangaGamer version of Sengoku Rance
* Support dumping/editing .ain files for Hentai Labyrinth and Evenicle 2
  Clinical Trial Edition

### [Version 0.5.1](https://haniwa.technology/alice-tools/alice-tools-0.5.1.zip)

* Fixes issue when dumping ain files with ascii-incompatible encodings

### [Version 0.5.0](https://haniwa.technology/alice-tools/alice-tools-0.5.0.zip)

* !!! Breaks bytecode compatibility with previous versions !!!
* Removed `--inline-strings` options from aindump and ainedit
* Strings are now inlined in `S_PUSH` instructions, etc.
* Added a few more bytecode macros

### [Version 0.4.0](https://haniwa.technology/alice-tools/alice-tools-0.4.0.zip)

* Added alice-ar tool for extracting archive files

### [Version 0.3.0](https://haniwa.technology/alice-tools/alice-tools-0.3.0.zip)

* Now supports ain files up to version 14 (Evenicle 2, Haha Ranman)
* Improved ex file compatibility, now works with Rance 03, Rance IX and Evenicle 2
* aindump now emits macro instructions by default (makes bytecode easier to read)
* Most error messages now include line numbers

### [Version 0.2.1](https://haniwa.technology/alice-tools/alice-tools-0.2.1.zip)

* Added `--input-encoding` and `--output-encoding` options to control the text
  encoding of input and output files
* Added a `--transcode` option to ainedit to change the text encoding of an ain
  file
* Fixed an issue where the `--split` option to exdump would produce garbled
  filenames on Windows

### [Version 0.2.0](https://haniwa.technology/alice-tools/alice-tools-0.2.0.zip)

* Added exdump and exbuild tools

### [Version 0.1.1](https://haniwa.technology/alice-tools/alice-tools-0.1.1.zip)

* Fixed an issue where non-ASCII characters could not be reinserted using
  `ainedit -t`

### [Version 0.1.0](https://haniwa.technology/alice-tools/alice-tools-0.1.0.zip)

* Initial release
* Supports dumping/editing .ain files up to version 12 (Rance X)
