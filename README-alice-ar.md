Extracting Archives
===================

The `alice ar` commands are used to extract files from various AliceSoft
archive formats (.ald, .afa). It should be capable of extracting all .ald/.afa
files up to Haha Ranman (the latest game at the time of this writing).

Extracting Archives
-------------------

First of all, if you're running this tool from the Windows command prompt, you
should run the command `chcp 65001` to change the console's code page to UTF-8,
and then change the console's font to something which supports Japanese
characters (e.g. MS Mincho). Otherwise you will see a lot of garbled text
flying across the screen when running this tool.

To list the contents of an archive,

    alice ar list archive.afa

To extract files from an archive to the current directory,

    alice ar extract archive.afa

To extract files from an archive to the directory "out",

    alice ar extract -o out archive.afa

To view the available command line options,

    alice ar extract --help

When extracting archives containing .flat files, alice-ar will recursively
extract the .flat files by default (these files contain images along with
various other data). Usually you don't care about anything other than the
images when extracting these archives, so you should pass the --images-only
flag,

    alice ar extract --images-only archiveFlat.afa

You can pass the --raw flag to prevent alice-ar from converting any files,

    alice ar extract --raw archive.afa

When the --raw flag is given, .flat files will not be recursively extracted and
images will not be converted to .png format.

Creating Archives
-----------------

To create an archive, you must first create a manifest file that lists the files
to be included in the archive. Multiple manifest formats are supported. The type
of manifest is indicated by the first line of the file, which should be a '#'
character followed by the name of the format. See below for more information.

Once you've created your manifest, run the `pack` command to create the archive,

    alice ar pack manifest_filename
    
Note: At this time only AFAv2 archives can be created.

### ALICEPACK

This is the simplest manifest format. You simply specify the archive name and
then list the files to be included in the archive. E.g.

    #ALICEPACK
    output_filename.afa
    input_filename1.qnt
    input_filename2.qnt
    ...
    
### BATCHPACK

This is a generic batch-conversion manifest format. Each line following the
archive output path specifies an input path/format and an output path/format.
All files in the directory given by the input path are converted to the output
format and saved in the output directory. E.g.

    #BATCHPACK
    output_filename.afa
    src_dir,src_fmt,dst_dir,dst_fmt
    ...

### ALICECG2

This is a format for creating CG archives. It supports conversion between image
formats prior to archive creation. E.g.

    #ALICECG2
    output_filename.afa
    0,src,PNG,dst,QNT
    ...

In the above example, all image files in the directory `src` will be converted
to QNT format, saved in the `dst` directory, and then the contents of the `dst`
directory will be added to the archive.

The format of each line in the file is,

    file_number,src_directory,src_format,dst_directory,dst_format

where:

* `file_number` is the archive number (ALD archives only)
* `src_directory` is the path to a directory containing the source CG files
* `src_format` is the name of the image format of the source CG files
* `dst_directory` is the path to the directory to store the converted CG files
* `dst_format` is the name of the image format to be used for converted CG files

Note: `file_number` and `src_format` are included for compatibility with
AliceSoft's own tools (from System 4 SDK). They are ignored by alice-tools.
