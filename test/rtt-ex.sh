#!/usr/bin/env bash

SRC_EX="$1"
SRC_X=$(mktemp)
DST_EX=$(mktemp "${SRC_EX}.XXXXXX")

echo "Dumping $SRC_EX"
alice ex dump -o "$SRC_X" "$SRC_EX"
echo "Rebuilding .ex file"
alice ex build -o "$DST_EX" "$SRC_X"
echo "Comparing .ex files"
alice ex compare "$SRC_EX" "$DST_EX"
rm "$SRC_X"
rm "$DST_EX"
