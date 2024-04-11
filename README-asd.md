Editing .asd files
==================

The `alice asd` commands can be used to dump .asd files to .json format and
rebuild them.

Usage
-----

To dump an .asd file:

    alice asd dump in.asd -o out.json

To rebuild an .asd file from a .json:

    alice asd build out.json -o out.asd

