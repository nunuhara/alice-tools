#!/usr/bin/env bash

HAVE_R10=1
HAVE_DD=1

function build_project {
    if ! [ -f "$1" ]; then
        if [ -f "$2" ]; then
            cp "$2" "$1"
        else
            return 1
        fi
    fi

    ${ALICE:-alice} project build "$3"
    return 0
}

build_project Rance10.src.ain rance10/Rance10.ain Rance10.pje
build_project dohnadohna.src.ain dohna/dohnadohna.ain Dohna.pje
