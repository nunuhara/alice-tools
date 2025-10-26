#!/usr/bin/env bash

if [ "$#" -eq 1 ]; then
    JAF_FILE="$1"
    JAM_FILE="${JAF_FILE%.*}.jam"
    JSON_FILE="${JAF_FILE%.*}.json"
    VERSION=4
elif [ "$#" -eq 2 ]; then
    JAF_FILE="$1"
    JAM_FILE="${JAF_FILE%.*}.jam"
    JSON_FILE="${JAF_FILE%.*}.json"
    VERSION="$2"
elif [ "$#" -eq 3 ]; then
    JAF_FILE="$1"
    JAM_FILE="$2.jam"
    JSON_FILE="$2.json"
    VERSION="$3"
elif [ "$#" -eq 4 ]; then
    JAF_FILE="$1"
    JAM_FILE="$2"
    JSON_FILE="$3"
    VERSION="$4"
else
    echo Wrong number of arguments to run_test.
    exit 1
fi

printf "Running test $JAF_FILE (v$VERSION)... "

# compile jaf file
ACTUAL_AIN="$(mktemp --suffix=.ain)"
if ! ${ALICE:-alice} ain edit --jaf "$JAF_FILE" -o "$ACTUAL_AIN" --ain-version "$VERSION" --silent; then
    echo compile failed
    rm -f "$ACTUAL_AIN"
    exit 1
fi

# assemble jam/json files
EXPECTED_AIN="$(mktemp --suffix=.ain)"
if ! ${ALICE:-alice} ain edit --json "$JSON_FILE" --jam "$JAM_FILE" --no-validate -o "$EXPECTED_AIN" --ain-version "$VERSION" --silent; then
    echo assemble failed
    rm -f "$ACTUAL_AIN" "$EXPECTED_AIN"
    exit 1
fi

# compare ain files
if ! ${ALICE:-alice} ain compare "$ACTUAL_AIN" "$EXPECTED_AIN"; then
    rm -f "$ACTUAL_AIN" "$EXPECTED_AIN"
    exit 1
fi

rm -f "$ACTUAL_AIN" "$EXPECTED_AIN"
