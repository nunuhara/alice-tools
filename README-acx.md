Editing .acx files
==================

The `alice acx` commands can be used to dump .acx files to .csv format and
rebuild them.

Usage
-----

To dump a .acx file:

    alice acx dump -o out.csv in.acx

To rebuild a .acx file from a .csv:

    alice acx build -o out.acx in.csv

NOTE: You should edit the .csv files with a text editor, NOT EXCEL. Excel likes
to destroy .csv files in all sorts of idiotic ways. Don't use excel.
