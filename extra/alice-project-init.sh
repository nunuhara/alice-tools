#!/usr/bin/env bash

#
# This script initializes a project folder for a blank project targeting a
# specific VM version. It should be passed the path to a .ain file which it
# will read HLL declarations from to set up the project.
#
# Usage: alice-project-init path/to/file.ain
#

set -e

if [[ $# -ne 1 ]]; then
    echo "Wrong number of arguments"
    echo "Usage: $0 <path-to-ain-file>"
    exit 1
fi

set -x

SRCAIN="$1"
AINVER=$(alice ain dump --ain-version "$SRCAIN")
BASE=$(basename "$SRCAIN")
DIR=$(dirname "$SRCAIN")
PROJECT_NAME="${BASE%.*}"

mkdir -p out
mkdir -p src/hll
alice ain dump --hll "$SRCAIN" > src/hll/hll.inc
mv ./*.hll src/hll/

#
# NOTE: Uncomment this line to copy the game directory into the output
#       directory. This will allow for running the produced .ain file
#       directly from the output directory.
#
#cp -r $DIR/* out/

cat > "$PROJECT_NAME.pje" <<- EOF
ProjectName = "$PROJECT_NAME"

CodeName = "$PROJECT_NAME.ain"
CodeVersion = "$AINVER"

SourceDir = "src"
OutputDir = "out"

Source = {
    "src.inc",
}
EOF

cat > "src/src.inc" <<- "EOF"
SystemSource = {
    "hll/hll.inc",
}

Source = {
    "main.jaf",
}
EOF

cat > "src/main.jaf" <<- "EOF"
int main(void)
{
    system.MsgBox("Hello, world!");
    system.Exit(0);
}

void message(int nMsgNum, int nNumofMsg, string szText)
{
    system.Output("%d/%d: %s" % nMsgNum % nNumofMsg % szText);
}

float message::detail::GetMessageSpeedRate(void)
{
	return 1.0;
}

void message::detail::GetReadMessageTextColor(ref int Red, ref int Green, ref int Blue)
{
	Red = 255;
	Green = 0;
	Blue = 0;
}
EOF

cat > "build.sh" <<- EOF
#!/bin/sh
alice project build "$PROJECT_NAME.pje"
EOF

cat > "run.sh" <<- EOF
#!/bin/sh
cd out
env LANG=ja_JP.UTF-8 wine System40.exe
EOF

cat >"xrun.sh" <<- EOF
#!/bin/sh
cd out
xsystem4
EOF

chmod +x build.sh run.sh xrun.sh
