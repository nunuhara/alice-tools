Building Projects
-----------------

As mod projects grow larger, manually building all of the modified files quickly
becomes unmanagable. At this point you might decide to write a bash script (or
similar) to automate running all of the necessary commands. However, this too
can become a mess as the number of source files increases (and thus the length
of your command lines).

The `alice project build` tool provides a solution to this problem by
specifying a declarative format for describing all of the inputs and outputs of
the build process.

## Initializing a Project Directory

Run the command `alice project init --mod <ain-file>` to set up a project
directory for creating a mod for the given .ain file. This will create a
directory structure similar to the one described below.

## Project Files

The main configuration for a project is contained in a .pje file in the
project's root directory. For example, suppose we have the following directory
structure:

    Rance10ModCG.manifest
    Rance10ModFlat.manifest
    Rance10ModSound.manifest
    ex/
        1_デバッグ時ボーナス／ボス.txtex
        ...
        Rance10EX.txtex
    flat/
        ...
    flatsrc/
        ...
    my_project.pje
    ogg/
        ....
    out/
        Rance10.ain
        Rance10EX.ex
        Rance10.exe
        ...
    png/
        ...
    qnt/
        ...
    src/
        battle.jaf
        battle.jam
        cards.jaf
        cards.jam
        main.jaf
        quests/
            quests.inc
            my_quest.jaf
        quests.jaf
        mod.inc
    src.ain
    
The file `my_project.pje` might have the following contents:

```
// The name of the project
ProjectName = "my_project"

// The name of the .ain file (will be placed in OutputDir)
CodeName = "Rance10.ain"

// The name of the directory containing .jaf/.jam source files
SourceDir = "src"

// The name of the directory to place built output files
OutputDir = "out"

// The source files (relative to SourceDir)
Source = {
    "mod.inc",
}

// Bytecode files to append to the .ain file's code section
ModJam = {
    "cards.jam",
    "battle.jam",
}

// The input .ain file to modify
ModAin = "src.ain"

// List of archive manifests to build
Archives = {
    "Rance10ModCG.manifest",
    "Rance10ModSound.manifest",
    "Rance10ModFlat.manifest"
}

// The name of the .txtex input file from which the .ex file should be built
ExInput = "ex/Rance10EX.txtex"
// The name of the .ex file (will be placed in OutputDir)
ExName = "Rance10EX.ex"
```

## Include Files

In the "Source" list above there is a single file named "mod.inc" which is found
in the SourceDir "src/". This file should have the following contents:

```
Source = {
    "misc.jaf",
    "battle.jaf",
    "cards.jaf",
    "quests.jaf",
    "quests/quests.inc",
}
```

The file "src/quests/quests.inc" in turn should have the following contents:

```
Source = {
	"my_quest.jaf",
}
```

All of the files in the "Source" lists contained in these files will be built
(in order) when the `project build` command is executed.

It is also possible to specify another "SystemSource" list in these files (and
in the .pje file). Source files listed under "SystemSource" are built before
any files listed under the normal "Source" list.

## Archive Manifests

The "Archives" list in the .pje file should contain a list of archive manifest
files (as understood by `alice ar pack`). When building, the output path in the
manifest is used. As such, it is necessary to specify the OutputDir as part of
this path to ensure the output file is placed in the correct directory.

### Rance10ModCG.manifest

    #BATCHPACK
    out/Rance10ModCG.afa
    png, png, qnt, qnt

### Rance10ModFlat.manifest

    #BATCHPACK
    out/Rance10ModFlat.afa
    flatsrc,txtex,flat,flat

### Rance10ModSound.manifest

    #BATCHPACK
    out/Rance10ModSound.afa
    ogg,ogg,ogg,ogg

## Building the Project

Simply pass the path to the .pje file to the `project build` command to build
the project.

    alice project build my_project.pje

This will produce .ain, .ex and .afa files in the OutputDir specified in the
.pje file.

    out/
        Rance10.ain
        Rance10EX.ex
        Rance10ModCG.afa
        Rance10ModFlat.afa
        Rance10ModSound.afa
        ...

