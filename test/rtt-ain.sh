#!/usr/bin/env bash

SRC_AIN="$1"
SRC_JAM=$(mktemp)
DST_AIN=$(mktemp "${SRC_AIN}.XXXXXX")

echo "Dumping code from $SRC_AIN"
alice ain dump -c -o "$SRC_JAM" "$SRC_AIN"
echo "Assembling dumped code"
alice ain edit -c "$SRC_JAM" -o "$DST_AIN" "$SRC_AIN"
echo "Comparing AIN files"
alice ain compare "$SRC_AIN" "$DST_AIN"
rm "$SRC_JAM"
rm "$DST_AIN"
