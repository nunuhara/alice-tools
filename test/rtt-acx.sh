#!/usr/bin/env bash

SRC_ACX="$1"
SRC_CSV=$(mktemp)
DST_CSV=$(mktemp)
DST_ACX=$(mktemp "${SRC_ACX}.XXXXXX")

echo "Dumping $SRC_ACX"
alice acx dump -o "$SRC_CSV" "$SRC_ACX"
echo "Rebuilding .acx file"
alice acx build -o "$DST_ACX" "$SRC_CSV"
echo "Comparing .acx files"
alice acx dump -o "$DST_CSV" "$DST_ACX"
diff "$SRC_CSV" "$DST_CSV" > /dev/null
if [ $? -eq 0 ]; then
    echo ".acx files match"
else
    echo "FAIL: .acx files differ"
fi
rm "$SRC_CSV"
rm "$DST_ACX"
rm "$DST_CSV"
