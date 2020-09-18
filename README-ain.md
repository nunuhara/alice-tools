Editing .ain files
------------------

There are two common usage patterns: one for editing text, and one for editing
code. In either case, you will have to enter commands at a command prompt
(e.g. cmd.exe) and edit some (rather large) text files.

In the following examples, I assume you have a file named "Rance10.ain" in the
current directory which you intend to edit.

### Editing Text

First, dump the text using the following command:

    alice ain dump -t -o out.txt Rance10.ain

This will create a file named "out.txt" containing all of the strings/messages
in the .ain file, sorted by function.

The syntax of this file is relatively simple. Anything following a ";"
character up until the end of a line is considered a comment and ignored when
the file is read back in. Initially all lines are commented out, meaning that
this file will cause no change to the output .ain file when given to ainedit.
To change a line of text, you must first remove the leading ";" character from
that line.

An uncommented line should contain one of the following forms:

    s[<number>] = "<text>"
    m[<number>] = "<text>"

The first form represents a string (text used by the game code) while the
second represents a message (text displayed to the player). Simply edit
`<text>` to change the text.

`<number>` is the ID of the string/message. You should not change this. It may
happen that the same string appears multiple times in the section of text you
are editing. It is only necessary to change the string once. If you set
different values for multiple instances of the same string, only the last
instance will be reflected in the output .ain file.

Once you've finished editing the text, you can reinsert it back into the ain
file by issuing the following command:

    alice ain edit -t out.txt -o out.ain Rance10.ain

This will create a file called "out.ain", which is a modified version of
"Rance10.ain" containing the modified text from the file "out.txt". You can
then replace the .ain file in your game directory with this file.

### Editing Code

First, dump the code with the following command:

    alice ain dump -c -o out.jam Rance10.ain

This will create a file named "out.jam" containing the disassembled bytecode
from the file "Rance10.ain".

A full tutorial on System 4 bytecode is outside the scope of this README.
Suffice to say, it's very low level and you probably don't want to make any
advanced mods using this method (but you're welcome to try). The syntax is
very close to what you'll see in the "Disassembled" and "Combined" views in
SomeLoliCatgirl's "AinDecompiler" tool.

Once you've finished editing out.jam, you can reinsert the code back into the
.ain file using the following command:

    alice ain edit -c out.jam -o out.ain Rance10.ain

This will create a file called "out.ain", which is a modified version of
"Rance10.ain" containing the modified code from the file "out.jam". You can
then replace the .ain file in your game directory with this file.
