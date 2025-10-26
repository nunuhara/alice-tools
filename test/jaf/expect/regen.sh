#!/usr/bin/env bash

cd $(dirname "$0")

shopt -s nullglob

if ! command -v jq >/dev/null 2>&1; then
    echo jq not found
    exit 1
fi

for f in *.json; do
    VERSION=$(jq '.version' "$f")
    JAFFILE=${f%%.*}.jaf
    JAMFILE=${f%.json}.jam
    ./make-test.sh "$JAFFILE" "$JAMFILE" "$f" "$VERSION"
done

