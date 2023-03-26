#!/usr/bin/env bash

RTT="$(dirname $0)/rtt-ex.sh"
EXDIR="$(dirname $0)/ex"

shopt -s nullglob
for f in $EXDIR/*.ex; do
    $RTT "$f"
done
