#!/usr/bin/env bash

SRC_EX="$1"
SRC_X=$(mktemp)
DST_EX=$(mktemp "${SRC_EX}.XXXXXX")

printf "Running RTT for $SRC_EX... "

if ! ${ALICE:-alice} ex dump -o "$SRC_X" "$SRC_EX"; then
    echo dump failed
    rm -f "$SRC_X"
    exit 1
fi

if ! ${ALICE:-alice} ex build -o "$DST_EX" "$SRC_X"; then
    echo build failed
    rm -f "$SRC_X" "$DST_EX"
    exit 1
fi

if ! ${ALICE:-alice} ex compare "$SRC_EX" "$DST_EX"; then
    echo compare failed
    rm -f "$SRC_X" "$DST_EX"
    exit 1
fi

rm -f "$SRC_X" "$DST_EX"
