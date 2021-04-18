Editing .flat Files
===================

The .flat format is used to compose/animate sprites, effects, etc. For example
in Rance 10 it's used for the enemy sprites in battle, attack effects and so on.

It is a poorly understood format at present. All that can be done with .flat
files with the current tools is CG replacement.

## Extracting the .flat archive

.flat files are stored in a .afa archive ending in "Flat.afa", e.g.
"Rance10Flat.afa". Run the folliwing command to extract the archive to a
directory named "flat".

    alice ar extract --raw -o flat Rance10Flat.afa
    
Note that we have to pass the `--raw` option to prevent the `ar extract` command
from recursively extracting the .flat files (the .flat extraction performed by
`ar extract` does not preserve all of the information needed to rebuild the
file; we want to use the `flat extract` command instead).

## Extracting a .flat file

Supposing there is a .flat file named "test.flat" in the current directory, it
can be extracted as follows,

    alice flat extract -o test.txtex test.flat

This will create a .txtex file "test.txtex" in the current directory, along with
several other files with the prefix "test.txtex.".

## Converting the CGs

The CGs extracted by `flat extract` are typically in .ajp format. To convert to
a .ajp to a .png for editing, run the following command (substituting the file
name as appropriate),

    alice cg convert -t png test.txtex.0.ajp

Then open the file "test.txtex" in a text editor and change the line which
refers to that file to instead refer to the converted .png file (thankfully,
AliceSoft games are able to read .png files so it's not necessary to convert
back to .ajp).

At this point you can edit the .png file to your liking.

## Rebuilding the .flat file

To rebuild the .flat file, run the following command,

    alice flat build -o test.flat test.txtex
    
## Rebuilding the .flat archive

See [here](https://haniwa.technology/alice-tools/README-alice-ar.html) for
general information on how to build .afa archives.

It is also possible to use the "BATCHPACK" manifest format to automatically
convert .txtex files to .flat files, rather than manually running the
`flat build` command as described above.
