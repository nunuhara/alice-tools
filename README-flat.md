Editing .flat Files
===================

The .flat format is used to compose/animate sprites, effects, etc. For example
in Rance 10 it's used for the enemy sprites in battle, attack effects and so on.

It is a poorly understood format. All that can be done with .flat files at
present is CG replacement.

## Extracting the .flat archive

.flat files are stored in .afa archives ending in "Flat.afa", e.g.
"Rance10Flat.afa". Run the folliwing command to extract the archive to a
directory named "flat".

    alice ar extract -o flat --manifest manifest.txt Rance10Flat.afa

Note that we use the `--manifest` option here to automatically generate a
manifest which we will use later to rebuild the archive.

## Converting the CGs

The CGs extracted by `ar extract` (or `flat extract`) are typically in .ajp
format. To convert to a .ajp to a .png for editing, run the following command
(substituting the file name as appropriate),

    alice cg convert -t png test.x.0.ajp

Then open the file "test.x" in a text editor and change the line which
refers to that file to instead refer to the converted .png file (thankfully,
AliceSoft games are able to read .png files so it's not necessary to convert
back to .ajp).

At this point you can edit the .png file to your liking.

## Rebuilding the .flat archive

To rebuild the flat archive that we extracted earlier, run the following command,

    alice ar pack manifest.txt

Note that this will overwrite the original archive, so it's a good idea to make
a backup first. You can also change the output archive name by editing the file
`manifest.txt` and changing the second line.

## Alternate workflows

### `--raw` Option

The workflow described above will unpack every .flat file in the archive and
rebuild them again when repacking. If you only want to modify a small number
of .flat files, this can be optimized by passing the `--raw` option when
extracting the archive. E.g.,

    alice ar extract -o flat --manifest manifest.txt --raw Rance10Flat.afa

This will prevent the .flat files from being unpacked during the extraction
process.

Then you can unpack the individual .flat files you want to edit with the
`flat extract` command:

    alice flat extract -o test.x test.flat

At this point you can convert/edit the CGs as described above.

Before repacking the archive, you should edit the file `manifest.txt` and
instruct it to rebuild the .flat files you unpacked. E.g. your manifest
might look like this:

    #ALICEPACK --src-dir=flat
    Rance10Flat.afa
    <...>
    test.flat
    <...>

You should change each line corresponding to a file you unpacked as follows:

    #ALICEPACK --src-dir=flat
    Rance10Flat.afa
    <...>
    test.x,flat
    <...>

Then run `alice ar pack manifest.txt` to rebuild the archive as usual.

### `--flat-png` Option

If you pass the `--flat-png` option to the `ar extract` command, all images
will be automatically converted to PNG format. This simplifies the process,
as it eliminates the step of converting CGs and updating .x files.

However, this will increase your final .afa file size by approximately 4x.
