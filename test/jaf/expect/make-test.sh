#!/usr/bin/env bash

function usage {
    echo Usage:
    echo "    $0 <jaf-file>"
    echo "    $0 <jaf-file> <version>"
    echo "    $0 <jaf-file> <out-name> <version>"
    echo "    $0 <jaf-file> <jam-file> <json-file> <version>"
}

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
    echo Wrong number of arguments.
    usage
    exit 1
fi

if [[ "${JAF_FILE##*.}" != "jaf" ]]; then
    echo Wrong file extension: "$JAF_FILE" "(expected .jaf)"
    usage
    exit 1
fi

if [[ "${JAM_FILE##*.}" != "jam" ]]; then
    echo Wrong file extension: "$JAM_FILE" "(expected .jam)"
    usage
    exit 1
fi

if [[ "${JSON_FILE##*.}" != "json" ]]; then
    echo Wrong file extension: "$JSON_FILE" "(expected .json)"
    usage exit 1
fi

AIN_FILE=$(mktemp --suffix=.ain)
if ! ${ALICE:-alice} ain edit --jaf "$JAF_FILE" -o "$AIN_FILE" --ain-version "$VERSION" --silent; then
    echo compile failed
    rm -f "$AIN_FILE"
    exit 1;
fi

if ! ${ALICE:-alice} ain dump -c -o "$JAM_FILE" "$AIN_FILE"; then
    echo disassemble failed
    rm -f "$AIN_FILE" "$JAM_FILE"
    exit 1
else
    echo Generated "$JAM_FILE"
fi

if ! ${ALICE:-alice} ain dump --json -o "$JSON_FILE" "$AIN_FILE"; then
    echo json dump failed
    rm -f "$AIN_FILE" "$JAM_FILE" "$JSON_FILE"
    exit 1
else
    echo Generated "$JSON_FILE"
fi
